param([string]$Binary='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/small-model-thread-profile',
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
foreach($trial in @(@{Name='separate-1';Fused=$false},@{Name='single-1';Fused=$true},
                    @{Name='single-2';Fused=$true},@{Name='separate-2';Fused=$false})) {
 $directory=Join-Path $output $trial.Name
 & (Join-Path $PSScriptRoot 'benchmark_native_defaults.ps1') -Executable $Binary -OutputDirectory $directory `
  -Frames $Frames -Fps 60 -Experiences ORIGINAL -Renderers GPU -Levels LEVEL1_1 -Stereo 2 -RenderScale 1 `
  -GpuDriver $GpuBackend -GodMode -FixedTemporalClock -TraceModelThreadCost `
  -FusedSmallModel:$trial.Fused -SeparateSmallModel:(!$trial.Fused)
 $log=Join-Path $directory 'ORIGINAL-LEVEL1_1-GPU.log'
 $draws=& (Join-Path $PSScriptRoot 'summarize_scene_draws.ps1') -Log $log -Output (Join-Path $directory 'draws.json')
 $hostCosts=& (Join-Path $PSScriptRoot 'read_model_thread_cost.ps1') -Log $log -Output (Join-Path $directory 'thread.json')
 if($runs.Count) {
  $reference=$runs[0].Host.records;$records=$hostCosts.records
  if($reference.Count -ne $records.Count){throw 'Changed model count'}
  for($i=0;$i -lt $records.Count;++$i) {
   foreach($field in 'shape','vertices','polygons','bytes','batch','index') {
    if($reference[$i][$field] -ne $records[$i][$field]){throw "Changed model workload/annotation at $i field $field"}
   }
  }
 }
 $frameLines=@(Get-Content -LiteralPath $log | Where-Object {$_ -match '^frame-work-distribution-us median=(\d+) p95=(\d+) p99=(\d+) max=(\d+)$'})
 if($frameLines.Count -ne 1){throw 'Missing frame distribution'}
 $null=$frameLines[0] -match '^frame-work-distribution-us median=(\d+) p95=(\d+) p99=(\d+) max=(\d+)$'
 $frame=@{median_us=[long]$Matches[1];p95_us=[long]$Matches[2]}
 $runs+=@{Name=$trial.Name;Host=$hostCosts;Gpu=$draws;Frame=$frame}
 Write-Output "Completed $($trial.Name): $($hostCosts.measured_records) annotated measured models"
}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed'}
if((Get-FileHash -LiteralPath $preferences).Hash -ne $preferencesHash){throw 'Saved preferences changed'}
$summary=@(foreach($run in $runs) {
 @{name=$run.Name;measured_records=$run.Host.measured_records;
  cpu_us=$run.Host.measured_cpu_us;wall_us=$run.Host.measured_wall_us;uploaded_bytes=$run.Host.measured_upload_bytes;
  frame_work=$run.Frame;gpu_draws=$run.Gpu;host_log_sha256=$run.Host.log_sha256}
})
@{complete=$true;sha256=$binaryHash;backend=$GpuBackend;runs=$summary;
 scope='Loaded ABBA stage diagnostic, same full-quality authored workload and input transfer counts. Thread CPU time excludes descheduling and other/driver worker threads; calls add overhead. Other projects remain running; not isolated/sustained FPS acceptance.'} |
 ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output 'Completed loaded small-model stage CPU/GPU comparison; preferences unchanged.'
