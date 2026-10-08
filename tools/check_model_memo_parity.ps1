param([string]$Binary='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/model-memo-parity',
 [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12')
$ErrorActionPreference='Stop'
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
 @{Name='ex-lava';Options=@{Experience='EX';Stage='LEVEL6_6';RenderScale=2;Ground=9;Sky=1;RayTracing=1;Reflections=3}},
 @{Name='asteroids';Options=@{Experience='ORIGINAL';Stage='LEVEL1_2';RenderScale=2;GroundEnabled=0;RayTracing=1;Reflections=3}},
 @{Name='joined';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=2;GroundEnabled=0;JoinedStereoSubmissions=$true;RayTracing=0;Reflections=0}},
 # Workers can interleave per-model stderr records. Validate their main-thread
 # post-join aggregate instead, without logging inside both workers.
 @{Name='parallel';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';RenderScale=2;GroundEnabled=0;ParallelStereoEncoding=$true;RayTracing=0;Reflections=0}},
 @{Name='mono-water';Options=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=0;RenderScale=2;Ground=5;Sky=1;RayTracing=1;Reflections=3}}
)
function Read-Uploads([string]$Directory) {
 $log=Join-Path $Directory 'runtime.log'
 $lines=@(Get-Content -LiteralPath $log | Where-Object {$_.StartsWith('model-upload:')})
 if(!$lines.Count){throw "Missing owned upload accounting: $Directory"}
 $result=@{draws=$lines.Count;input=0L;uploaded=0L;copies=0L;reused=0L}
 foreach($line in $lines) {
  if($line -notmatch '^model-upload: input=(\d+) uploaded=(\d+) copies=(\d+) reused=(\d+)$') {
   throw "Malformed/interleaved upload record: $line"
  }
  foreach($field in @(@('input',1),@('uploaded',2),@('copies',3),@('reused',4))) {
   $result[$field[0]] += [long]$Matches[[int]$field[1]]
  }
 }
 return $result
}
function Read-PairUpload([string]$Directory) {
 $lines=@(Get-Content -LiteralPath (Join-Path $Directory 'runtime.log') |
  Where-Object {$_.StartsWith('stereo-source-upload-policy:')})
 # Only the first complete pair is compared, not extrapolated over the run.
 if(!$lines.Count -or $lines[0] -notmatch '^stereo-source-upload-policy: pair-shared input=(\d+) uploaded=(\d+) shared=(\d+) copies=(\d+) storage=(\d+)$') {
  throw "Missing post-join pair accounting: $Directory"
 }
 return @{input=[long]$Matches[1];uploaded=[long]$Matches[2];shared=[long]$Matches[3];copies=[long]$Matches[4];storage=[long]$Matches[5]}
}
$results=@();$totalAvoided=0L;$totalCopies=0L
foreach($case in $cases) {
 $options=@{Binary=$Binary;GpuBackend=$GpuBackend;Frames=32;CaptureFirst=8;CaptureInterval=8;
  FixedTemporalClock=$true;PrerollTicks=1000;Stereo=2;Display='4_3';GodMode=1;
  TraceModelUploads=($case.Name -ne 'parallel')}
 foreach($entry in $case.Options.GetEnumerator()){$options[$entry.Key]=$entry.Value}
 $reference=Join-Path $output ($case.Name+'-duplicate')
 $candidate=Join-Path $output ($case.Name+'-memo')
 & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $reference -DuplicateModelUploads
 & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $candidate
 foreach($directory in @($reference,$candidate)) {
  $log=Join-Path $directory 'runtime.log'
  if(Select-String -LiteralPath $log -Pattern 'stereo failure:|replaying complete frame|model failure:' -Quiet){throw "Fallback/failure: $($case.Name)"}
  if($options.Stereo) {
   foreach($eye in 0,1){if(!(Select-String -LiteralPath $log -SimpleMatch "stereo-eye: direct eye=$eye" -Quiet)){throw 'Eye output was not retained directly'}}
  }
 }
 $names=@('lava.bmp','lava.bmp.frame-8.bmp','lava.bmp.frame-16.bmp','lava.bmp.frame-24.bmp','lava.bmp.frame-32.bmp')
 foreach($directory in @($reference,$candidate)) {
  $actualNames=@(Get-ChildItem -LiteralPath $directory -Filter '*.bmp' | ForEach-Object {$_.Name})
  if(@(Compare-Object $actualNames $names).Count){throw "Incomplete capture: $($case.Name)"}
 }
 foreach($name in $names) {
  if((Get-FileHash -LiteralPath (Join-Path $reference $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $candidate $name)).Hash) {
   throw "Owned-buffer memo changed presentation: $($case.Name)/$name"
  }
 }
 if($case.Name -in @('parallel','asteroids')) {
  $before=Read-PairUpload $reference;$after=Read-PairUpload $candidate
  foreach($field in 'input','shared','storage'){if($before[$field] -ne $after[$field]){throw 'Pair source workload changed'}}
  $scope='first completed pair only'
  if($case.Name -eq 'asteroids') {
   # This fixture contains only whole-object billboards. They deliberately
   # bypass the ordinary-input memo and its per-draw diagnostic, so prove the
   # actual all-borrowed source policy instead of accepting missing records.
   foreach($directory in @($reference,$candidate)) {
    if(Select-String -LiteralPath (Join-Path $directory 'runtime.log') -SimpleMatch 'model-upload:' -Quiet){throw 'Asteroid fixture unexpectedly used ordinary model uploads'}
   }
   if($before.input -ne $before.shared -or $after.input -ne $after.shared){throw 'Asteroid fixture did not use all-borrowed billboard inputs'}
  }
 } else {
  $before=Read-Uploads $reference;$after=Read-Uploads $candidate
  foreach($field in 'input','draws'){if($before[$field] -ne $after[$field]){throw 'Model workload changed'}}
  $scope='all ordinary model draws (billboard uploads not logged)'
 }
 $bytes=$before.uploaded-$after.uploaded;$copies=$before.copies-$after.copies
 if($bytes -lt 0 -or $copies -lt 0){throw "Owned memo increased transfers: $($case.Name)"}
 if($case.Name -notin @('parallel','asteroids')){$totalAvoided+=$bytes;$totalCopies+=$copies}
 $results+=@{case=$case.Name;images=$names.Count;exact=$true;before=$before;after=$after;
  avoided_bytes=$bytes;avoided_copies=$copies;accounting_scope=$scope;sha256=$binaryHash;backend=$GpuBackend}
 Write-Output "PASS $($case.Name): $($names.Count) exact full images; avoided $bytes bytes / $copies copies ($scope)"
}
if(!$totalAvoided -or !$totalCopies){throw 'No actual owned model upload savings'}
if((Get-FileHash -LiteralPath $preferences).Hash -ne $preferencesHash){throw 'Saved preferences changed'}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed'}
@{complete=$true;results=$results;avoided_bytes=$totalAvoided;avoided_copies=$totalCopies;
 scope='Same-binary full-image parity and transfer counts under concurrent builds; not isolated FPS acceptance'} |
 ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output 'Owned model memo parity passed; saved preferences and executable unchanged.'
