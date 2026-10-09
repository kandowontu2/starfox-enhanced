param([string]$Binary='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/model-pose-profile',
 [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12',
 [ValidateRange(120,10000)][int]$Frames=180,[switch]$Rays)
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a fresh result directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$binaryHash=(Get-FileHash -LiteralPath $Binary).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$preferencesHash=(Get-FileHash -LiteralPath $preferences).Hash
$runs=@()
foreach($trial in @(@{Name='buffered-1';Buffered=$true},@{Name='constants-1';Buffered=$false},
                    @{Name='constants-2';Buffered=$false},@{Name='buffered-2';Buffered=$true})) {
 $directory=Join-Path $output $trial.Name
 $extra=if($Rays){@{RenderScale=2;RayTracing=1;Reflections=3;EnhancedGround=$true;GroundMaterial=5;EnhancedSky=$true}}else{@{RenderScale=1}}
 & (Join-Path $PSScriptRoot 'benchmark_native_defaults.ps1') -Executable $Binary -OutputDirectory $directory `
  -Frames $Frames -Fps 60 -Experiences ORIGINAL -Renderers GPU -Levels LEVEL1_1 -Stereo 2 @extra `
  -GpuDriver $GpuBackend -GodMode -FixedTemporalClock -TraceStereoInputUploads -SeparateSmallModel -BufferedModelPoses:$trial.Buffered -InlineModelPoses:(!$trial.Buffered)
 $log=Join-Path $directory 'ORIGINAL-LEVEL1_1-GPU.log'
 $lines=Get-Content -LiteralPath $log
 $frameLines=@($lines | Where-Object {$_ -match '^frame-work-distribution-us median=(\d+) p95=(\d+) p99=(\d+) max=(\d+)$'})
 if($frameLines.Count -ne 1){throw 'Missing full-frame distribution'}
 $null=$frameLines[0] -match '^frame-work-distribution-us median=(\d+) p95=(\d+) p99=(\d+) max=(\d+)$'
 $frame=@{median_us=[long]$Matches[1];p95_us=[long]$Matches[2];p99_us=[long]$Matches[3];max_us=[long]$Matches[4]}
 $uploads=@(Select-String -LiteralPath $log -Pattern '^stereo-model-uploads: input=(\d+) uploaded=(\d+) shared=(\d+) copies=(\d+) storage=(\d+)$')
 $uniforms=@(Select-String -LiteralPath $log -Pattern '^stereo-pose-uniforms: bytes=(\d+) pushes=(\d+)$')
 if($uploads.Count -ne $Frames -or $uniforms.Count -ne $Frames){throw 'Missing completed-pair accounting'}
 $traffic=@(for($i=0;$i -lt $Frames;++$i) {
  $up=$uploads[$i].Matches[0].Groups;$un=$uniforms[$i].Matches[0].Groups
  if($trial.Buffered -and ([long]$un[1].Value -or [int]$un[2].Value)){throw 'Buffered policy still used constants'}
  if($runs.Count -and [long]$up[1].Value -ne $runs[0].traffic[$i].input){throw 'Changed authored input demand'}
  @{input=[long]$up[1].Value;storage_upload=[long]$up[2].Value;copies=[long]$up[4].Value;
   uniform_bytes=[long]$un[1].Value;uniform_pushes=[int]$un[2].Value}
 })
 if(!$trial.Buffered -and !@($traffic | Where-Object {$_.uniform_pushes -gt 0}).Count){throw 'No actual constants used'}
 # The redirected stderr writer can retain a handle after process completion.
 # Read with sharing instead of restarting a finished benchmark over that lock.
 $stream=[IO.File]::Open($log,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
 $hasher=[Security.Cryptography.SHA256]::Create()
 try {$logHash=[BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-','')}
 finally {$hasher.Dispose();$stream.Dispose()}
 $runs+=@{name=$trial.Name;frame=$frame;traffic=$traffic;log_sha256=$logHash}
 Write-Output "Completed $($trial.Name): $Frames actual pairs; median $($frame.median_us) us"
}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed'}
if((Get-FileHash -LiteralPath $preferences).Hash -ne $preferencesHash){throw 'Preferences changed'}
@{complete=$true;sha256=$binaryHash;backend=$GpuBackend;rays=[bool]$Rays;runs=$runs;
 scope='Quiet loaded same-binary ABBA: identical logical model input demand and actual completed stereo pairs; no model clocks or per-draw GPU timestamps. Constants also consume GPU transport. Concurrent projects continue; not isolated or sustained FPS acceptance.'} |
 ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'results.json')
