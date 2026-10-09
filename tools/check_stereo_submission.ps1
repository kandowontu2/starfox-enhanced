param([string]$Binary='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/stereo-submission-parity',
 [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12',
 [ValidateRange(24,120)][int]$Frames=32)
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a fresh result directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$binaryHash=(Get-FileHash -LiteralPath $Binary).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$preferencesHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
$cases=@(
 @{Name='corneria';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=1;GroundEnabled=0;RayTracing=0;Reflections=0}},
 @{Name='water-reflections';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=2;Ground=5;Sky=1;Bloom=2;RayTracing=1;Reflections=3}},
 @{Name='banked-upscale';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=3;Ground=1;Sky=1;CameraBank=0.2;RayTracing=0;Reflections=0}},
 @{Name='venom-motion';Options=@{Experience='ORIGINAL';Stage='LEVEL3_5';RenderScale=2;GroundEnabled=0;MotionBlurQuality=2;RayTracing=0;Reflections=0}}
)
$results=@()
foreach($case in $cases) {
 $options=@{Binary=$Binary;GpuBackend=$GpuBackend;Frames=$Frames;CaptureFirst=8;CaptureInterval=8;
  FixedTemporalClock=$true;PrerollTicks=1000;Stereo=2;Display='4_3';GodMode=1}
 foreach($entry in $case.Options.GetEnumerator()){$options[$entry.Key]=$entry.Value}
 $reference=Join-Path $output ($case.Name+'-split')
 $candidate=Join-Path $output ($case.Name+'-joined')
 & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $reference -SplitStereoSubmissions
 & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $candidate -JoinedStereoSubmissions
 foreach($pair in @(@{Directory=$reference;Mode='split'},@{Directory=$candidate;Mode='joined'})) {
  $log=Join-Path $pair.Directory 'runtime.log'
  # capture_lava traces the actual resident producer/presenter and all failures.
  if(!(Select-String -LiteralPath $log -SimpleMatch "stereo-scene-submission: $($pair.Mode)" -Quiet)) {
   throw "Missing $($pair.Mode) producer marker: $($case.Name)"
  }
  if(Select-String -LiteralPath $log -Pattern 'stereo failure:|replaying complete frame' -Quiet){throw "Partial/fallback pair: $($case.Name)"}
  foreach($eye in 0,1){if(!(Select-String -LiteralPath $log -SimpleMatch "stereo-eye: direct eye=$eye" -Quiet)){throw 'Eye output was not retained directly'}}
 }
 $images=@(Get-ChildItem -LiteralPath $reference -Filter '*.bmp')
 # The capturer writes the final image as well as each selected sequence
 # frame. Require the exact names; the final frame can legitimately appear in
 # both forms, so it must not be mistaken for an extra/duplicate capture.
 $expectedNames=@('lava.bmp')
 for($captureFrame=8;$captureFrame -le $Frames;$captureFrame+=8){$expectedNames+="lava.bmp.frame-$captureFrame.bmp"}
 $actualNames=@(Get-ChildItem -LiteralPath $candidate -Filter '*.bmp' | ForEach-Object {$_.Name})
 if($images.Count -ne $expectedNames.Count -or $actualNames.Count -ne $expectedNames.Count -or
  @(Compare-Object $images.Name $expectedNames).Count -or @(Compare-Object $actualNames $expectedNames).Count){throw 'Incomplete pair capture'}
 foreach($image in $images) {
  $actual=Join-Path $candidate $image.Name
  if(!(Test-Path -LiteralPath $actual) -or (Get-FileHash -LiteralPath $image.FullName).Hash -ne (Get-FileHash -LiteralPath $actual).Hash) {
   throw "Joined stereo presentation differs: $($case.Name)/$($image.Name)"
  }
 }
 $results+=@{case=$case.Name;images=$images.Count;exact=$true;sha256=$binaryHash;backend=$GpuBackend}
 Write-Output "PASS $($case.Name): $($images.Count) byte-identical full-SBS presentations"
}
$afterPreferencesHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
if($afterPreferencesHash -ne $preferencesHash){throw 'Saved preferences changed'}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed'}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output 'Joined stereo presentation parity passed; executable and saved preferences unchanged.'
