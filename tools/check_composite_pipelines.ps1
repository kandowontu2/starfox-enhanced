param([string]$Binary='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/composite-pipeline-parity',
 [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12',
 [ValidateSet('composite-pipelines','span-clear','clip-scratch','stereo-encoding','fp64-multiply')][string]$Comparison='composite-pipelines',
 [string]$ReferenceBinary='',
 [ValidateSet('corneria','water-reflections','banked-upscale','venom-motion','msaa','ex-menu')]
 [string[]]$CaseNames=@('corneria','water-reflections','banked-upscale','venom-motion','msaa','ex-menu'),
 [switch]$LowPowerGpu,
 [ValidateRange(24,120)][int]$Frames=32)
$ErrorActionPreference='Stop'
if(($Comparison -eq 'fp64-multiply') -ne [bool]$ReferenceBinary){throw 'fp64-multiply requires a preserved ReferenceBinary; other comparisons use one executable'}
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a fresh result directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$binaryHash=(Get-FileHash -LiteralPath $Binary).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$preferencesHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
$referenceHash=if($ReferenceBinary){(Get-FileHash -LiteralPath $ReferenceBinary).Hash}else{$binaryHash}
$referencePreferences=if($ReferenceBinary){Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($ReferenceBinary))) 'pregame.cfg'}else{$preferences}
$referencePreferencesHash=if(Test-Path -LiteralPath $referencePreferences){(Get-FileHash -LiteralPath $referencePreferences).Hash}else{''}
$cases=@(
 @{Name='corneria';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=1;GroundEnabled=0;RayTracing=0;Reflections=0}},
 @{Name='water-reflections';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=2;Ground=5;Sky=1;Bloom=2;RayTracing=1;Reflections=3}},
 @{Name='banked-upscale';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=3;Ground=1;Sky=1;CameraBank=0.2;RayTracing=0;Reflections=0}},
 @{Name='venom-motion';Options=@{Experience='ORIGINAL';Stage='LEVEL3_5';RenderScale=2;GroundEnabled=0;MotionBlurQuality=2;RayTracing=0;Reflections=0}},
 @{Name='msaa';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=2;GroundEnabled=0;AaType=6;AaQuality=3;RayTracing=0;Reflections=0}},
 # Start at the cartridge title, without gameplay preroll or the host menu.
 # Advancing TITLEMAP 1000 ticks first enters the attract demo instead.
 @{Name='ex-menu';Options=@{Experience='EX';Stage='TITLEMAP';ExMenu=1;PrerollTicks=0;GroundEnabled=0;RayTracing=0;Reflections=0}}
)
$cases=@($cases | Where-Object {$_.Name -in $CaseNames})
if(!$cases.Count){throw 'Select at least one presentation case'}
$results=@()
foreach($case in $cases) {
 $options=@{Binary=$Binary;GpuBackend=$GpuBackend;Frames=$Frames;CaptureFirst=8;CaptureInterval=8;
  FixedTemporalClock=$true;PrerollTicks=1000;Stereo=2;Display='16_9';GodMode=1}
 if($LowPowerGpu){$options.LowPowerGpu=$true}
 foreach($entry in $case.Options.GetEnumerator()){$options[$entry.Key]=$entry.Value}
 $referenceMode=if($Comparison -eq 'fp64-multiply'){'reference'}elseif($Comparison -eq 'composite-pipelines'){'unified'}elseif($Comparison -eq 'stereo-encoding'){'split'}else{'full'}
 $candidateMode=if($Comparison -eq 'fp64-multiply'){'fixed-limbs'}elseif($Comparison -eq 'span-clear'){'bounds'}elseif($Comparison -eq 'clip-scratch'){'small'}elseif($Comparison -eq 'stereo-encoding'){'parallel'}else{'split'}
 $reference=Join-Path $output ($case.Name+'-'+$referenceMode)
 $candidate=Join-Path $output ($case.Name+'-'+$candidateMode)
 if($Comparison -eq 'fp64-multiply') {
  $options.Binary=$ReferenceBinary
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $reference
  $options.Binary=$Binary
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $candidate
 } elseif($Comparison -eq 'stereo-encoding') {
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $reference -SerialStereoEncoding
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $candidate -ParallelStereoEncoding
 } elseif($Comparison -eq 'clip-scratch') {
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $reference -FullClip
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $candidate -SmallClip
 } elseif($Comparison -eq 'span-clear') {
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $reference -FullSpanClear
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $candidate -SpanBoundsClear
 } else {
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $reference -UnifiedCompositePipeline
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $candidate -SplitCompositePipelines
 }
 $actualAdapters=@()
 foreach($pair in @(@{Directory=$reference;Mode=$referenceMode},@{Directory=$candidate;Mode=$candidateMode})) {
  $log=Join-Path $pair.Directory 'runtime.log'
  # This native menu capture has no model-span emitter; it remains a control
  # for unchanged background/HUD presentation, not evidence of span execution.
  $exercisesComponent=$Comparison -in @('composite-pipelines','stereo-encoding','fp64-multiply') -or $case.Name -ne 'ex-menu'
  if(!$exercisesComponent -and (Select-String -LiteralPath $log -Pattern '^(span-clear|clip-scratch):' -Quiet)) {
   throw 'No-span EX menu control unexpectedly emitted model spans'
  }
  $marker=if($Comparison -eq 'stereo-encoding'){'stereo-scene-submission'}else{$Comparison}
  if($Comparison -ne 'fp64-multiply' -and $exercisesComponent -and !(Select-String -LiteralPath $log -SimpleMatch "${marker}: $($pair.Mode)" -Quiet)) {
   throw "Missing $($pair.Mode) $Comparison marker: $($case.Name)"
  }
  if(Select-String -LiteralPath $log -Pattern 'stereo failure:|replaying complete frame|MSAA scene fallback:' -Quiet){throw "Partial/fallback pair: $($case.Name)"}
  foreach($eye in 0,1){if(!(Select-String -LiteralPath $log -SimpleMatch "stereo-eye: direct eye=$eye" -Quiet)){throw 'Eye output was not retained directly'}}
  $logText=Get-Content -LiteralPath $log -Raw
  if($logText -notmatch "(?m)^test-gpu-adapter: ([^\r\n]+) driver=$GpuBackend\r?`$"){throw 'Actual capture adapter/backend was not recorded'}
  $actualAdapters+=$Matches[1]
 }
 if($actualAdapters[0] -ne $actualAdapters[1]){throw 'Reference and candidate used different GPUs'}
 $images=@(Get-ChildItem -LiteralPath $reference -Filter '*.bmp')
 $expectedNames=@('lava.bmp')
 for($captureFrame=8;$captureFrame -le $Frames;$captureFrame+=8){$expectedNames+="lava.bmp.frame-$captureFrame.bmp"}
 $actualNames=@(Get-ChildItem -LiteralPath $candidate -Filter '*.bmp' | ForEach-Object {$_.Name})
 if($images.Count -ne $expectedNames.Count -or $actualNames.Count -ne $expectedNames.Count -or
  @(Compare-Object $images.Name $expectedNames).Count -or @(Compare-Object $actualNames $expectedNames).Count){throw 'Incomplete pair capture'}
 foreach($image in $images) {
  $actual=Join-Path $candidate $image.Name
  if(!(Test-Path -LiteralPath $actual) -or (Get-FileHash -LiteralPath $image.FullName).Hash -ne (Get-FileHash -LiteralPath $actual).Hash) {
   throw "$Comparison presentation differs: $($case.Name)/$($image.Name)"
  }
 }
 $results+=@{case=$case.Name;comparison=$Comparison;component_exercised=$exercisesComponent;images=$images.Count;exact=$true;sha256=$binaryHash;reference_sha256=$referenceHash;backend=$GpuBackend;actual_adapter=$actualAdapters[0]}
 Write-Output "PASS $($case.Name): $($images.Count) byte-identical full-SBS presentations"
}
$afterPreferencesHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
if($afterPreferencesHash -ne $preferencesHash){throw 'Saved preferences changed'}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed'}
if($ReferenceBinary -and (Get-FileHash -LiteralPath $ReferenceBinary).Hash -ne $referenceHash){throw 'Reference executable changed'}
$afterReferencePreferencesHash=if(Test-Path -LiteralPath $referencePreferences){(Get-FileHash -LiteralPath $referencePreferences).Hash}else{''}
if($afterReferencePreferencesHash -ne $referencePreferencesHash){throw 'Reference preferences changed'}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output "$Comparison presentation parity passed; executable and saved preferences unchanged."
