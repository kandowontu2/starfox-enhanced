param([string]$Binary='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/model-memo-profile',
 [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12',
 [ValidateRange(120,10000)][int]$Frames=180)
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a fresh result directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$binaryHash=(Get-FileHash -LiteralPath $Binary).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$preferencesHash=(Get-FileHash -LiteralPath $preferences).Hash
$runs=@()
foreach($trial in @(@{Name='duplicate-1';Duplicate=$true},@{Name='memo-1';Duplicate=$false},
                    @{Name='memo-2';Duplicate=$false},@{Name='duplicate-2';Duplicate=$true})) {
 $directory=Join-Path $output $trial.Name
 & (Join-Path $PSScriptRoot 'benchmark_native_defaults.ps1') -Executable $Binary -OutputDirectory $directory `
  -Frames $Frames -Fps 60 -Experiences ORIGINAL -Renderers GPU -Levels LEVEL1_1 -Stereo 2 -RenderScale 1 `
  -GpuDriver $GpuBackend -GodMode -FixedTemporalClock -TraceModelCost -TraceSceneGpuDraws `
  -DuplicateModelUploads:$trial.Duplicate
 $log=Join-Path $directory 'ORIGINAL-LEVEL1_1-GPU.log'
 $draws=& (Join-Path $PSScriptRoot 'summarize_scene_draws.ps1') -Log $log -Output (Join-Path $directory 'draws.json')
 $lines=Get-Content -LiteralPath $log
 $headers=@($lines | Where-Object {$_.StartsWith('model-host-cost-trace:')})
 if($headers.Count -ne 1 -or $headers[0] -notmatch '^model-host-cost-trace: deferred=1 count=(\d+) dropped=0$') {
  throw 'Missing, dropped or multiple deferred host model traces'
 }
 $expected=[int]$Matches[1];$records=@();$totals=@{pack=0L;prepare=0L;upload=0L;encode=0L;bytes=0L}
 foreach($line in $lines) {
  if(!$line.StartsWith('model-host-cost-us')){continue}
  if($line -notmatch '^model-host-cost-us shape=(\d+) vertices=(\d+) polygons=(\d+) pack=(\d+) prepare=(\d+) upload=(\d+) encode=(\d+) bytes=(\d+)$') {
   throw "Malformed deferred model trace: $line"
  }
  $records+=@{shape=[long]$Matches[1];vertices=[long]$Matches[2];polygons=[long]$Matches[3];bytes=[long]$Matches[8]}
  foreach($field in @(@('pack',4),@('prepare',5),@('upload',6),@('encode',7),@('bytes',8))) {
   $totals[$field[0]] += [long]$Matches[[int]$field[1]]
  }
 }
 if(!$records.Count -or $records.Count -ne $expected){throw 'Incomplete host trace'}
 if($runs.Count) {
  $reference=$runs[0].Records
  if($reference.Count -ne $records.Count){throw 'Different model draw counts'}
  for($i=0;$i -lt $records.Count;++$i) {
   foreach($field in 'shape','vertices','polygons') {
    if($reference[$i][$field] -ne $records[$i][$field]){throw "Changed authored model workload at draw $i"}
   }
   if(!$trial.Duplicate -and $records[$i].bytes -gt $reference[$i].bytes){throw 'Memo uploaded more than duplicate control'}
  }
 }
 $frameLine=@($lines | Where-Object {$_ -match '^frame-work-distribution-us median=(\d+) p95=(\d+) p99=(\d+) max=(\d+)$'})
 if($frameLine.Count -ne 1){throw 'Missing measured frame distribution'}
 $null=$frameLine[0] -match '^frame-work-distribution-us median=(\d+) p95=(\d+) p99=(\d+) max=(\d+)$'
 $frame=@{median_us=[long]$Matches[1];p95_us=[long]$Matches[2];p99_us=[long]$Matches[3];max_us=[long]$Matches[4]}
 $runs+=@{Name=$trial.Name;Duplicate=$trial.Duplicate;Records=$records;HostTotals=$totals;
  FrameWork=$frame;GpuDraws=$draws;LogSha256=(Get-FileHash -LiteralPath $log).Hash}
 Write-Output "Completed $($trial.Name): $($records.Count) authored model draws; $($totals.bytes) uploaded bytes"
}
if($runs[0].HostTotals.bytes -ne $runs[3].HostTotals.bytes -or
   $runs[1].HostTotals.bytes -ne $runs[2].HostTotals.bytes -or
   $runs[1].HostTotals.bytes -ge $runs[0].HostTotals.bytes){throw 'Transfer reduction was not reproducible'}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed'}
if((Get-FileHash -LiteralPath $preferences).Hash -ne $preferencesHash){throw 'Saved preferences changed'}
$summary=@(foreach($run in $runs) {
 @{name=$run.Name;duplicate=$run.Duplicate;draws=$run.Records.Count;host_all_draw_totals=$run.HostTotals;
  measured_frame_work=$run.FrameWork;gpu_measured_draws=$run.GpuDraws;log_sha256=$run.LogSha256}
})
@{complete=$true;sha256=$binaryHash;backend=$GpuBackend;runs=$summary;
 scope='Loaded ABBA diagnostic; other projects remain running. Host totals include cold setup/all draws; GPU/frame distributions discard 60 warmup pairs. Not isolated or sustained FPS acceptance.'} |
 ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output 'Completed loaded memo/duplicate diagnostic; transfer counts reproducible, preferences unchanged.'
