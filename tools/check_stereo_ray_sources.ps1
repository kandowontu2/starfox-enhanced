param([string]$Binary='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/stereo-ray-sources',
 [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12',
 [string[]]$CaseNames=@(),[switch]$GpuUploads)
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a fresh result directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$binaryHash=(Get-FileHash -LiteralPath $Binary).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$preferencesHash=(Get-FileHash -LiteralPath $preferences).Hash
$cases=@(
 @{Name='water-rays';Experience='ORIGINAL';Preroll=1000;Options=@{Stage='LEVEL1_1';RenderScale=2;Ground=5;Sky=1;Bloom=2;RayTracing=1;Reflections=3}},
 # The selected asteroid recording has no ordinary ray-source consumers.
 # Rays remain enabled: verify zero extra preparation and exact specialized
 # sprite/fallback output instead of inventing an ordinary caster requirement.
 @{Name='asteroids';Experience='ORIGINAL';Preroll=1000;NoSources=$true;Options=@{Stage='LEVEL1_2';RenderScale=2;GroundEnabled=0;RayTracing=1;Reflections=3}},
 @{Name='lava-rays';Experience='EX';Preroll=120;Options=@{Stage='LEVEL6_6';RenderScale=2;Ground=9;Sky=1;Bloom=2;RayTracing=1;Reflections=3}},
 @{Name='msaa-rays';Experience='ORIGINAL';Preroll=1000;Options=@{Stage='LEVEL1_1';RenderScale=2;Ground=5;Sky=1;AaType=6;AaQuality=3;RayTracing=1;Reflections=3}},
 @{Name='no-rays';Experience='ORIGINAL';Preroll=1000;NoRays=$true;Options=@{Stage='LEVEL1_1';RenderScale=1;GroundEnabled=0;RayTracing=0;Reflections=0}}
)
if($GpuUploads) {
 $cases+=@(
  @{Name='parallel-rays';Experience='ORIGINAL';Preroll=1000;Options=@{Stage='LEVEL1_1';RenderScale=2;Ground=5;Sky=1;Bloom=2;RayTracing=1;Reflections=3;ParallelStereoEncoding=$true}},
  @{Name='joined-rays';Experience='ORIGINAL';Preroll=1000;Options=@{Stage='LEVEL1_1';RenderScale=2;Ground=5;Sky=1;Bloom=2;RayTracing=1;Reflections=3;JoinedStereoSubmissions=$true}}
 )
}
if($CaseNames.Count) {
 foreach($name in $CaseNames){if($name -notin $cases.Name){throw "Unknown ray source case: $name"}}
 $cases=@($cases | Where-Object {$_.Name -in $CaseNames})
}
$results=@()
foreach($case in $cases) {
 $options=@{Binary=$Binary;GpuBackend=$GpuBackend;Frames=32;CaptureFirst=8;CaptureInterval=8;
  FixedTemporalClock=$true;PrerollTicks=$case.Preroll;Stereo=2;Display='4_3';GodMode=1;
  TraceStereoSources=$true;Experience=$case.Experience}
 foreach($entry in $case.Options.GetEnumerator()){$options[$entry.Key]=$entry.Value}
 $reference=Join-Path $output ($case.Name+'-duplicated')
 $candidate=Join-Path $output ($case.Name+'-shared')
 if($GpuUploads){& (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $reference -DuplicateStereoRayUploads}
 else {& (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $reference -DuplicateStereoRays}
 & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $candidate
 $sources=@()
 $referencePolicy=if($GpuUploads){'pair-shared'}else{'duplicated'}
 $uploadTotals=@()
 foreach($route in @(@{Directory=$reference;Policy=$referencePolicy;Reference=$true},@{Directory=$candidate;Policy='pair-shared';Reference=$false})) {
  $log=Join-Path $route.Directory 'runtime.log'
  $markers=@(Select-String -LiteralPath $log -Pattern ("^stereo-ray-source-policy: "+$route.Policy+" sources=(\d+) prepared=(\d+)$"))
  if($markers.Count -ne 32){throw "Missing complete pair topology accounting: $($case.Name)"}
  foreach($marker in $markers) {
   if($route.Policy -eq 'duplicated' -or $case.NoRays -or $case.NoSources) {
    if($marker.Matches[0].Groups[1].Value -ne '0' -or $marker.Matches[0].Groups[2].Value -ne '0'){throw 'Unexpected unused ray preparation'}
   }
  }
  if($route.Policy -eq 'pair-shared' -and !$case.NoRays -and !$case.NoSources) {
   $positive=@($markers | Where-Object {[int]$_.Matches[0].Groups[2].Value -gt 0})
   if(!$positive.Count){throw 'No ordinary ray connectivity was consumed'}
   $sources=@($positive | ForEach-Object {$_.Line})
  }
  if(Select-String -LiteralPath $log -Pattern 'stereo failure:|replaying complete frame' -Quiet){throw 'Partial or fallback pair'}
  foreach($eye in 0,1){if(!(Select-String -LiteralPath $log -SimpleMatch "stereo-eye: direct eye=$eye" -Quiet)){throw 'Missing independently retained eye'}}
  if($GpuUploads) {
   $rayUploads=@(Select-String -LiteralPath $log -Pattern '^stereo-ray-uploads: shared=(\d+) buffers=(\d+)$')
   if($rayUploads.Count -ne 32){throw 'Incomplete per-pair ray upload accounting'}
   if($route.Reference -or $case.NoRays -or $case.NoSources) {
    foreach($marker in $rayUploads){if([long]$marker.Matches[0].Groups[1].Value -ne 0 -or [int]$marker.Matches[0].Groups[2].Value -ne 0){throw 'Unexpected ray GPU borrowing'}}
   } elseif(!@($rayUploads | Where-Object {[long]$_.Matches[0].Groups[1].Value -gt 0}).Count){throw 'No immutable GPU ray input consumed'}
   $uploads=@(Select-String -LiteralPath $log -Pattern '^stereo-model-uploads: input=(\d+) uploaded=(\d+) shared=(\d+) copies=(\d+) storage=(\d+)$')
   if($uploads.Count -ne 32){throw 'Incomplete total upload accounting'}
   $inputTotal=0L;$uploadedTotal=0L;$copyTotal=0L
   foreach($marker in $uploads) {
    $inputTotal += [long]$marker.Matches[0].Groups[1].Value
    $uploadedTotal += [long]$marker.Matches[0].Groups[2].Value
    $copyTotal += [long]$marker.Matches[0].Groups[4].Value
    if([long]$marker.Matches[0].Groups[5].Value -gt 16MB){throw 'Stereo source GPU/transfer storage exceeds budget'}
   }
   $uploadTotals+=@{input=$inputTotal;uploaded=$uploadedTotal;copies=$copyTotal}
  }
 }
 $names=@('lava.bmp','lava.bmp.frame-8.bmp','lava.bmp.frame-16.bmp','lava.bmp.frame-24.bmp','lava.bmp.frame-32.bmp')
 foreach($directory in @($reference,$candidate)) {
  $actual=@(Get-ChildItem -LiteralPath $directory -Filter '*.bmp' | ForEach-Object {$_.Name})
  if(@(Compare-Object $actual $names).Count){throw 'Incomplete paired image capture'}
 }
 foreach($name in $names) {
  if((Get-FileHash -LiteralPath (Join-Path $reference $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $candidate $name)).Hash) {
   throw "Ray source preparation changed output: $($case.Name)/$name"
  }
 }
 if($GpuUploads) {
  if($uploadTotals[0].input -ne $uploadTotals[1].input){throw 'Ray source demand differs between policies'}
  if(!$case.NoRays -and !$case.NoSources -and ($uploadTotals[1].uploaded -ge $uploadTotals[0].uploaded -or $uploadTotals[1].copies -ge $uploadTotals[0].copies)){throw 'No ray source upload reduction'}
 }
 $results+=@{case=$case.Name;images=$names.Count;completed_pairs_per_policy=32;exact=$true;ray_sources=$sources;uploads=$uploadTotals}
 Write-Output "PASS $($case.Name): $($names.Count) exact paired images; actual ray-source policy verified"
}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed'}
if((Get-FileHash -LiteralPath $preferences).Hash -ne $preferencesHash){throw 'Saved preferences changed'}
@{complete=$true;sha256=$binaryHash;backend=$GpuBackend;gpu_uploads=[bool]$GpuUploads;results=$results;
 scope='Same-binary full-quality immutable ray-connectivity CPU/GPU comparison. Per-eye geometry/material expansion and ray results remain independent; not sustained FPS acceptance.'} |
 ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $output 'results.json')
