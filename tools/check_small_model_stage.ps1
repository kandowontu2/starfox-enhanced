param([string]$Binary='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/small-model-stage-parity',
 [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12',
 [string]$BaselineDirectory='',
 [switch]$ReferenceOnly)
$ErrorActionPreference='Stop'
if($ReferenceOnly -and !$BaselineDirectory){throw 'ReferenceOnly requires a completed prior checkpoint'}
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a fresh result directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$binaryHash=(Get-FileHash -LiteralPath $Binary).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$preferencesHash=(Get-FileHash -LiteralPath $preferences).Hash
$cases=@(
 @{Name='corneria';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=1;GroundEnabled=0;RayTracing=0;Reflections=0}},
 @{Name='water-rays';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=2;Ground=5;Sky=1;Bloom=2;RayTracing=1;Reflections=3}},
 @{Name='banked-upscale';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=3;Ground=1;Sky=1;CameraBank=0.2;RayTracing=0;Reflections=0}},
 @{Name='venom-motion';Options=@{Experience='ORIGINAL';Stage='LEVEL3_5';RenderScale=2;GroundEnabled=0;MotionBlurQuality=2;RayTracing=0;Reflections=0}},
 @{Name='corneria-msaa';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=2;GroundEnabled=0;AaType=6;AaQuality=3;RayTracing=0;Reflections=0}},
 @{Name='ex-lava';Options=@{Experience='EX';Stage='LEVEL6_6';RenderScale=2;Ground=9;Sky=1;RayTracing=1;Reflections=3}}
)
$baseline=$null
if($BaselineDirectory) {
 $BaselineDirectory=[IO.Path]::GetFullPath($BaselineDirectory)
 $baseline=Get-Content -Raw -LiteralPath (Join-Path $BaselineDirectory 'results.json') | ConvertFrom-Json
 if(!$baseline.complete -or $baseline.results.Count -ne $cases.Count){throw 'Incomplete prior checkpoint'}
 foreach($case in $cases) {
  $previous=@($baseline.results | Where-Object case -eq $case.Name)
  if($previous.Count -ne 1 -or !$previous[0].exact -or $previous[0].backend -ne $GpuBackend -or $previous[0].images -ne 5) {
   throw 'Prior checkpoint scenario/backend does not match'
  }
 }
 if(@($baseline.results.sha256 | Sort-Object -Unique).Count -ne 1){throw 'Mixed prior executables'}
}
$results=@()
foreach($case in $cases) {
 $options=@{Binary=$Binary;GpuBackend=$GpuBackend;Frames=32;CaptureFirst=8;CaptureInterval=8;
  FixedTemporalClock=$true;PrerollTicks=1000;Stereo=2;Display='4_3';GodMode=1}
 foreach($entry in $case.Options.GetEnumerator()){$options[$entry.Key]=$entry.Value}
 $reference=Join-Path $output ($case.Name+'-separate')
 $candidate=Join-Path $output ($case.Name+'-single-pass')
 & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $reference -SeparateSmallModel
 $directories=@($reference)
 if(!$ReferenceOnly) {
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $candidate -FusedSmallModel
  $marker=Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch 'model-small-stage: projection visibility painter in one resident pass'
  if(!$marker){throw "No actual small-model producer: $($case.Name)"}
  $directories+=,$candidate
 }
 foreach($directory in $directories) {
  $log=Join-Path $directory 'runtime.log'
  if(Select-String -LiteralPath $log -Pattern 'stereo failure:|replaying complete frame|model failure:' -Quiet){throw "Fallback/failure: $($case.Name)"}
  foreach($eye in 0,1){if(!(Select-String -LiteralPath $log -SimpleMatch "stereo-eye: direct eye=$eye" -Quiet)){throw 'Eye output was not retained directly'}}
 }
 $names=@('lava.bmp','lava.bmp.frame-8.bmp','lava.bmp.frame-16.bmp','lava.bmp.frame-24.bmp','lava.bmp.frame-32.bmp')
 foreach($directory in $directories) {
  $actualNames=@(Get-ChildItem -LiteralPath $directory -Filter '*.bmp' | ForEach-Object {$_.Name})
  if(@(Compare-Object $actualNames $names).Count){throw "Incomplete native pair capture: $($case.Name)"}
 }
 if(!$ReferenceOnly){foreach($name in $names) {
  if((Get-FileHash -LiteralPath (Join-Path $reference $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $candidate $name)).Hash) {
   throw "Small-model producer presentation differs: $($case.Name)/$name"
  }
 }}
 if($baseline) {
  $suffixes=if($ReferenceOnly){@('separate')}else{@('separate','single-pass')}
  foreach($suffix in $suffixes) {foreach($name in $names) {
   $oldImage=Join-Path $BaselineDirectory "$($case.Name)-$suffix/$name"
   $newImage=Join-Path $output "$($case.Name)-$suffix/$name"
   if((Get-FileHash -LiteralPath $oldImage).Hash -ne (Get-FileHash -LiteralPath $newImage).Hash) {
    throw "Projection initialization changed prior checkpoint: $($case.Name)-$suffix/$name"
   }
  }}
 }
 $results+=@{case=$case.Name;images=$names.Count;exact=$true;sha256=$binaryHash;backend=$GpuBackend}
 if($ReferenceOnly){Write-Output "PASS $($case.Name): $($names.Count) byte-identical prior-checkpoint full-SBS images, ordinary separate path"}
 else{Write-Output "PASS $($case.Name): $($names.Count) byte-identical full-SBS images, actual single-pass small-model marker"}
}
if((Get-FileHash -LiteralPath $preferences).Hash -ne $preferencesHash){throw 'Saved preferences changed'}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed'}
@{complete=$true;results=$results;baseline_sha256=if($baseline){$baseline.results[0].sha256}else{''};
 baseline_images=if($baseline){$cases.Count*5*$directories.Count}else{0};reference_only=$ReferenceOnly.IsPresent;
 scope='Opt-in bounded producer correctness and optional exact prior-checkpoint image comparison under concurrent builds; no isolated FPS/default acceptance'} |
 ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'results.json')
if($ReferenceOnly){Write-Output 'Default-path prior-checkpoint image comparison passed; saved preferences and executable unchanged.'}
else{Write-Output 'Small-model stage parity passed; saved preferences and executable unchanged.'}
