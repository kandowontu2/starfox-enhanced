param([string]$Binary='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/colour-span-trace',
 [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12',
 [switch]$Cooperative)
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
 @{Name='parallel';Options=@{Stage='LEVEL1_1';RenderScale=2;GroundEnabled=0;ParallelStereoEncoding=$true;RayTracing=0;Reflections=0}},
 @{Name='joined';Options=@{Stage='LEVEL1_1';RenderScale=2;GroundEnabled=0;JoinedStereoSubmissions=$true;RayTracing=0;Reflections=0}},
 @{Name='ex-menu';NoModels=$true;Options=@{Experience='EX';ExMenu=1;PrerollTicks=120;RenderScale=1;GroundEnabled=0;RayTracing=0;Reflections=0}}
)
$results=@()
foreach($case in $cases) {
 $options=@{Binary=$Binary;GpuBackend=$GpuBackend;Frames=32;CaptureFirst=8;CaptureInterval=8;
  FixedTemporalClock=$true;PrerollTicks=1000;Stereo=2;Display='4_3';GodMode=1;Experience='ORIGINAL'}
 foreach($entry in $case.Options.GetEnumerator()){$options[$entry.Key]=$entry.Value}
 $paths=@()
 foreach($route in 'reference','candidate','override') {
  $directory=Join-Path $output ($case.Name+'-'+$route)
  $selection=@{}
  if($route -ne 'reference') {
   if($Cooperative){$selection.CooperativeSpanTrace=$true}else{$selection.ColourSpanTrace=$true}
  }
  if($route -ne 'candidate'){$selection.FullSpanTrace=$true}
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options @selection -OutputDirectory $directory
  $log=Join-Path $directory 'runtime.log'
  if(Select-String -LiteralPath $log -Pattern 'stereo failure:|replaying complete frame' -Quiet){throw "Incomplete native pair: $($case.Name)/$route"}
  if(@(Select-String -LiteralPath $log -Pattern '^stereo presented:').Count -ne $options.Frames){throw 'Missing complete native presentations'}
  foreach($eye in 0,1){if(!(Select-String -LiteralPath $log -SimpleMatch "stereo-eye: direct eye=$eye" -Quiet)){throw 'Eye output was not retained directly'}}
  $marker=if($Cooperative){'span-tracing: cooperative rows'}else{'span-tracing: colour XY-only'}
  $colour=Select-String -LiteralPath $log -SimpleMatch $marker -Quiet
  if($colour -ne ($route -eq 'candidate' -and !$case.NoModels)){throw "Wrong actual span selection: $($case.Name)/$route"}
  if(!$case.NoModels -and $route -ne 'candidate' -and
     !(Select-String -LiteralPath $log -SimpleMatch 'span-tracing: full UV' -Quiet)){throw 'Full span override did not execute'}
  $paths+=,$directory
 }
 $names=@('lava.bmp','lava.bmp.frame-8.bmp','lava.bmp.frame-16.bmp','lava.bmp.frame-24.bmp','lava.bmp.frame-32.bmp')
 foreach($directory in $paths) {
  $actualNames=@(Get-ChildItem -LiteralPath $directory -Filter '*.bmp' | ForEach-Object {$_.Name})
  if(@(Compare-Object $actualNames $names).Count){throw 'Incomplete capture set'}
 }
 foreach($name in $names) {
  $expected=(Get-FileHash -LiteralPath (Join-Path $paths[0] $name)).Hash
  foreach($directory in $paths[1..2]) {
   if((Get-FileHash -LiteralPath (Join-Path $directory $name)).Hash -ne $expected) {
    throw "Colour span presentation differs: $($case.Name)/$name"
   }
  }
 }
 $results+=@{case=$case.Name;comparisons=$names.Count*2;exact=$true;sha256=$binaryHash;backend=$GpuBackend}
 Write-Output "PASS $($case.Name): 10 byte-identical SBS candidate/override comparisons"
}
if((Get-FileHash -LiteralPath $preferences).Hash -ne $preferencesHash){throw 'Saved preferences changed'}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed'}
@{complete=$true;results=$results;timing='Correctness only; other jobs left untouched, no isolated FPS acceptance'} |
 ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output 'Colour span tracing parity passed; saved preferences and executable unchanged.'
