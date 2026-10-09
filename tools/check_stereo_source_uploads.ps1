param([string]$Binary='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/stereo-source-uploads',
 [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12',
 [Alias('Cases')][string[]]$SelectedCases=@())
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a fresh result directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$binaryHash=(Get-FileHash -LiteralPath $Binary).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$preferencesHash=(Get-FileHash -LiteralPath $preferences).Hash
$cases=@(
 @{Name='corneria';Options=@{Stage='LEVEL1_1';RenderScale=1;GroundEnabled=0;RayTracing=0;Reflections=0}},
 @{Name='water-rays';Options=@{Stage='LEVEL1_1';RenderScale=2;Ground=5;Sky=1;Bloom=2;RayTracing=1;Reflections=3}},
 @{Name='banked-upscale';Options=@{Stage='LEVEL1_1';RenderScale=3;Ground=1;Sky=1;CameraBank=0.2;RayTracing=0;Reflections=0}},
 @{Name='venom-motion';Options=@{Stage='LEVEL3_5';RenderScale=2;GroundEnabled=0;MotionBlurQuality=2;RayTracing=0;Reflections=0}},
 @{Name='corneria-msaa';Options=@{Stage='LEVEL1_1';RenderScale=2;GroundEnabled=0;AaType=6;AaQuality=3;RayTracing=0;Reflections=0}},
 # Immutable source pooling includes billboard texels now, not only helper
 # polygon vertices. Asteroids must prove positive sprite-source reduction
 # against the explicitly duplicated path, rather than expecting zero uploads.
 @{Name='asteroids';Options=@{Stage='LEVEL1_2';RenderScale=2;GroundEnabled=0;RayTracing=1;Reflections=3}},
 @{Name='parallel';Options=@{Stage='LEVEL1_1';RenderScale=2;GroundEnabled=0;ParallelStereoEncoding=$true;RayTracing=0;Reflections=0}},
 @{Name='joined';Options=@{Stage='LEVEL1_1';RenderScale=2;GroundEnabled=0;JoinedStereoSubmissions=$true;RayTracing=0;Reflections=0}}
)
$results=@()
if($SelectedCases.Count) {
 foreach($requested in $SelectedCases){if($requested -notin $cases.Name){throw "Unknown source-upload case: $requested"}}
 $cases=@($cases | Where-Object {$_.Name -in $SelectedCases})
}
foreach($case in $cases) {
 $options=@{Binary=$Binary;GpuBackend=$GpuBackend;Frames=32;CaptureFirst=8;CaptureInterval=8;
  FixedTemporalClock=$true;PrerollTicks=1000;Stereo=2;Display='4_3';GodMode=1;Experience='ORIGINAL'}
 foreach($entry in $case.Options.GetEnumerator()){$options[$entry.Key]=$entry.Value}
 $reference=Join-Path $output ($case.Name+'-duplicated')
 $candidate=Join-Path $output ($case.Name+'-shared')
 & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $reference -DuplicateStereoSourceUploads
 & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $candidate
 $candidateMode=if($case.ZeroSources){'duplicated'}else{'pair-shared'}
 foreach($pair in @(@{Directory=$reference;Mode='duplicated'},@{Directory=$candidate;Mode=$candidateMode})) {
  $log=Join-Path $pair.Directory 'runtime.log'
  if(!(Select-String -LiteralPath $log -SimpleMatch "stereo-source-upload-policy: $($pair.Mode)" -Quiet)) {
   throw "Missing actual $($pair.Mode) upload marker: $($case.Name)"
  }
  if(Select-String -LiteralPath $log -Pattern 'stereo failure:|replaying complete frame' -Quiet){throw "Partial/fallback pair: $($case.Name)"}
  foreach($eye in 0,1){if(!(Select-String -LiteralPath $log -SimpleMatch "stereo-eye: direct eye=$eye" -Quiet)){throw 'Eye output was not retained directly'}}
 }
 $names=@('lava.bmp','lava.bmp.frame-8.bmp','lava.bmp.frame-16.bmp','lava.bmp.frame-24.bmp','lava.bmp.frame-32.bmp')
 foreach($directory in @($reference,$candidate)) {
  $actualNames=@(Get-ChildItem -LiteralPath $directory -Filter '*.bmp' | ForEach-Object {$_.Name})
  if(@(Compare-Object $actualNames $names).Count){throw 'Incomplete native pair capture'}
 }
 foreach($name in $names) {
  if((Get-FileHash -LiteralPath (Join-Path $reference $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $candidate $name)).Hash) {
   throw "Immutable source upload presentation differs: $($case.Name)/$name"
  }
 }
 $marker=Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -Pattern "stereo-source-upload-policy: $candidateMode input=(\d+) uploaded=(\d+) shared=(\d+) copies=(\d+) storage=(\d+)"
 if(!$marker){throw 'Missing source upload accounting'}
 if($case.ZeroSources) {
  if([long]$marker.Matches[0].Groups[2].Value -ne [long]$marker.Matches[0].Groups[1].Value -or
   [long]$marker.Matches[0].Groups[3].Value -ne 0 -or [long]$marker.Matches[0].Groups[5].Value -ne 0){throw 'Unused helper sources uploaded or retained'}
 } elseif([long]$marker.Matches[0].Groups[2].Value -ge [long]$marker.Matches[0].Groups[1].Value){throw 'No model upload reduction'}
 $results+=@{case=$case.Name;images=$names.Count;exact=$true;upload=$marker.Line;sha256=$binaryHash;backend=$GpuBackend}
 Write-Output "PASS $($case.Name): $($names.Count) byte-identical full-SBS images; $(if($case.ZeroSources){'zero unused source uploads'}else{'fewer source uploads'})"
}
if((Get-FileHash -LiteralPath $preferences).Hash -ne $preferencesHash){throw 'Saved preferences changed'}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed'}
@{complete=$true;results=$results;timing='Correctness only under concurrent builds; no isolated FPS acceptance'} |
 ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output 'Stereo immutable GPU source upload parity passed; saved preferences and executable unchanged.'
