param([string]$Binary='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/stereo-face-profile',
 [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12',
 [ValidateRange(120,10000)][int]$Frames=180,[switch]$Quiet,[switch]$RayUploads)
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a fresh result directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$binaryHash=(Get-FileHash -LiteralPath $Binary).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$preferencesHash=(Get-FileHash -LiteralPath $preferences).Hash
$runs=@()
if($RayUploads){$Quiet=$true} # No per-model clocks or GPU timestamps in this paired ray measurement.
foreach($trial in @(@{Name='duplicate-1';Duplicate=$true},@{Name='shared-1';Duplicate=$false},
                    @{Name='shared-2';Duplicate=$false},@{Name='duplicate-2';Duplicate=$true})) {
 $directory=Join-Path $output $trial.Name
 $rayOptions=if($RayUploads){@{RenderScale=2;RayTracing=1;Reflections=3;EnhancedGround=$true;GroundMaterial=5;EnhancedSky=$true;
  DuplicateStereoRayUploads=$trial.Duplicate;DuplicateStereoFaces=$false}}else{@{RenderScale=1;DuplicateStereoFaces=$trial.Duplicate}}
 & (Join-Path $PSScriptRoot 'benchmark_native_defaults.ps1') -Executable $Binary -OutputDirectory $directory `
  -Frames $Frames -Fps 60 -Experiences ORIGINAL -Renderers GPU -Levels LEVEL1_1 -Stereo 2 @rayOptions `
  -GpuDriver $GpuBackend -GodMode -FixedTemporalClock -TraceModelThreadCost:(!$Quiet) -TraceStereoInputUploads `
  -SeparateSmallModel
 $log=Join-Path $directory 'ORIGINAL-LEVEL1_1-GPU.log'
 $policy=if($trial.Duplicate -and !$RayUploads){'duplicated'}else{'pair-shared'}
 $markers=@(Select-String -LiteralPath $log -Pattern "^stereo-face-policy: $policy sources=(\d+) prepared=(\d+)$")
 if(!$markers.Count){throw 'Actual face preparation policy was not observed'}
 if(!$trial.Duplicate -and $markers[0].Matches[0].Groups[2].Value -eq '0'){throw 'No ordinary face source was consumed'}
 $hostCosts=$null
 if(!$Quiet){$hostCosts=& (Join-Path $PSScriptRoot 'read_model_thread_cost.ps1') -Log $log -Output (Join-Path $directory 'thread.json')}
 if($runs.Count -and !$Quiet) {
  $reference=$runs[0].Host.records;$records=$hostCosts.records
  if($reference.Count -ne $records.Count){throw 'Changed authored model count'}
  for($i=0;$i -lt $records.Count;++$i) {
   foreach($field in 'shape','vertices','polygons','batch','index') {
    if($reference[$i][$field] -ne $records[$i][$field]){throw "Changed authored model workload at $i field $field"}
   }
  }
 }
 $frameLines=@(Get-Content -LiteralPath $log | Where-Object {$_ -match '^frame-work-distribution-us median=(\d+) p95=(\d+) p99=(\d+) max=(\d+)$'})
 if($frameLines.Count -ne 1){throw 'Missing full-frame work distribution'}
 $null=$frameLines[0] -match '^frame-work-distribution-us median=(\d+) p95=(\d+) p99=(\d+) max=(\d+)$'
 $frame=@{median_us=[long]$Matches[1];p95_us=[long]$Matches[2];p99_us=[long]$Matches[3];max_us=[long]$Matches[4]}
 # Includes the pool uploads, unlike the per-model host clock. Pair-level
 # records retain actual completed images; never infer traffic from a pointer.
 $traffic=@(foreach($line in Get-Content -LiteralPath $log) {
  if($line -match '^stereo-model-uploads: input=(\d+) uploaded=(\d+) shared=(\d+) copies=(\d+) storage=(\d+)$') {
   @{input=[long]$Matches[1];uploaded=[long]$Matches[2];shared=[long]$Matches[3];copies=[long]$Matches[4];storage=[long]$Matches[5]}
  }
 })
 if($traffic.Count -ne $Frames){throw 'Missing per-completed-pair input upload accounting'}
 $rayMarkers=@()
 if($RayUploads) {
  $rayMarkers=@(Select-String -LiteralPath $log -Pattern '^stereo-ray-uploads: shared=(\d+) buffers=(\d+)$')
  if($rayMarkers.Count -ne $Frames){throw 'Missing per-completed-pair ray upload accounting'}
  if($trial.Duplicate) {
   foreach($marker in $rayMarkers){if([long]$marker.Matches[0].Groups[1].Value -ne 0 -or [int]$marker.Matches[0].Groups[2].Value -ne 0){throw 'Duplicate ray uploads still borrow GPU connectivity'}}
  } elseif(!@($rayMarkers | Where-Object {[long]$_.Matches[0].Groups[1].Value -gt 0}).Count){throw 'No ray GPU input borrowed'}
 }
 if($runs.Count) {
  for($i=0;$i -lt $traffic.Count;++$i) {
   if($traffic[$i].input -ne $runs[0].Traffic[$i].input){throw 'Changed authored input byte demand'}
  }
 }
 # Start-Process's stderr redirection writer can retain its handle briefly
 # after the process/complete trace has ended. Share that handle for this
 # read-only hash; do not restart a completed benchmark over a reader lock.
 $stream=[IO.File]::Open($log,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
 $hasher=[Security.Cryptography.SHA256]::Create()
 try {$logHash=[BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-','')}
 finally {$hasher.Dispose();$stream.Dispose()}
 $runs+=@{Name=$trial.Name;Host=$hostCosts;Frame=$frame;Traffic=$traffic;Policy=$markers[0].Line;LogHash=$logHash;RayPolicy=@($rayMarkers | ForEach-Object {$_.Line})}
 Write-Output "Completed $($trial.Name): $Frames complete pairs; frame median $($frame.median_us) us"
}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed'}
if((Get-FileHash -LiteralPath $preferences).Hash -ne $preferencesHash){throw 'Saved preferences changed'}
$summary=@(foreach($run in $runs) {
 @{name=$run.Name;measured_records=$run.Host.measured_records;
  model_cpu_us=$run.Host.measured_cpu_us;model_wall_us=$run.Host.measured_wall_us;
  model_uploaded_bytes=$run.Host.measured_upload_bytes;frame_work=$run.Frame;
  pair_upload_records=$run.Traffic;face_policy=$run.Policy;ray_uploads=$run.RayPolicy;log_sha256=$run.LogHash}
})
@{complete=$true;sha256=$binaryHash;backend=$GpuBackend;runs=$summary;quiet=[bool]$Quiet;ray_uploads=[bool]$RayUploads;
 scope='Loaded same-binary ABBA ordinary-face or ray-connectivity upload comparison. Same authored input demand and completed stereo pairs. Quiet mode omits model clocks/per-draw GPU timestamps; verbose mode also checks every annotated model. Full-frame work includes source preparation; per-model CPU clocks exclude shared preparation and must NOT stand in for total CPU savings. Concurrent projects remain running; not isolated or sustained FPS acceptance.'} |
 ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output 'Completed loaded stereo face comparison; preferences unchanged.'
