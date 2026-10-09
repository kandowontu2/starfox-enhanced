param([string]$Binary='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/model-pose-constants',
 [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12',
 [string[]]$CaseNames=@(),[switch]$CollectOnly,[string]$CapturedSha256='')
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
$binaryHash=(Get-FileHash -LiteralPath $Binary).Hash
if($CollectOnly) {
 if(!(Test-Path -LiteralPath $output) -or !$CapturedSha256 -or $CapturedSha256 -ne $binaryHash){throw 'CollectOnly requires existing captures and their independently observed executable hash'}
} else {
 if(Test-Path -LiteralPath $output){throw 'Choose a fresh result directory'}
 New-Item -ItemType Directory -Path $output | Out-Null
}
$preferences=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$preferencesHash=(Get-FileHash -LiteralPath $preferences).Hash
$cases=@(
 @{Name='native';Experience='ORIGINAL';Options=@{Stage='LEVEL1_1';RenderScale=1;GroundEnabled=0;RayTracing=0;Reflections=0}},
 @{Name='water-rays';Experience='ORIGINAL';Options=@{Stage='LEVEL1_1';RenderScale=2;Ground=5;Sky=1;Bloom=2;RayTracing=1;Reflections=3}},
 @{Name='msaa-rays';Experience='ORIGINAL';Options=@{Stage='LEVEL1_1';RenderScale=2;Ground=5;Sky=1;AaType=6;AaQuality=3;RayTracing=1;Reflections=3}},
 @{Name='taa';Experience='ORIGINAL';Options=@{Stage='LEVEL1_1';RenderScale=2;GroundEnabled=0;Taa=$true;RayTracing=0;Reflections=0}},
 @{Name='lava-rays';Experience='EX';Options=@{Stage='LEVEL6_6';RenderScale=2;Ground=9;Sky=1;Bloom=2;RayTracing=1;Reflections=3}},
 @{Name='parallel';Experience='ORIGINAL';Options=@{Stage='LEVEL1_1';RenderScale=2;Ground=5;Sky=1;RayTracing=1;Reflections=3;ParallelStereoEncoding=$true}},
 @{Name='joined';Experience='ORIGINAL';Options=@{Stage='LEVEL1_1';RenderScale=2;Ground=5;Sky=1;RayTracing=1;Reflections=3;JoinedStereoSubmissions=$true}},
 @{Name='snapshot';Experience='ORIGINAL';Options=@{Stage='LEVEL1_1';RenderScale=2;GroundEnabled=0;RayTracing=0;Reflections=0;StereoSnapshots=$true}}
)
if($CaseNames.Count) {
 foreach($name in $CaseNames){if($name -notin $cases.Name){throw "Unknown pose case: $name"}}
 $cases=@($cases | Where-Object {$_.Name -in $CaseNames})
}
$results=@()
foreach($case in $cases) {
 $options=@{Binary=$Binary;GpuBackend=$GpuBackend;Frames=32;CaptureFirst=8;CaptureInterval=8;
  FixedTemporalClock=$true;PrerollTicks=1000;Stereo=2;Display='4_3';GodMode=1;
  TraceStereoSources=$true;Experience=$case.Experience}
 if($case.Experience -eq 'EX'){$options.PrerollTicks=120}
 foreach($entry in $case.Options.GetEnumerator()){$options[$entry.Key]=$entry.Value}
 $reference=Join-Path $output ($case.Name+'-buffered')
 $candidate=Join-Path $output ($case.Name+'-constants')
 if(!$CollectOnly) {
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $reference -BufferedModelPoses
  & (Join-Path $PSScriptRoot 'capture_lava.ps1') @options -OutputDirectory $candidate -InlineModelPoses
 }
 $traffic=@()
 foreach($route in @(@{Directory=$reference;Reference=$true},@{Directory=$candidate;Reference=$false})) {
  $log=Join-Path $route.Directory 'runtime.log'
  if(Select-String -LiteralPath $log -Pattern 'stereo failure:|replaying complete frame' -Quiet){throw 'Partial or fallback pair'}
  $eyePath=if($case.Options.StereoSnapshots){'snapshot'}else{'direct'}
  foreach($eye in 0,1){if(!(Select-String -LiteralPath $log -SimpleMatch "stereo-eye: $eyePath eye=$eye" -Quiet)){throw 'Missing actual independent eye'}}
  $uploads=@(Select-String -LiteralPath $log -Pattern '^stereo-model-uploads: input=(\d+) uploaded=(\d+) shared=(\d+) copies=(\d+) storage=(\d+)$')
  $uniforms=@(Select-String -LiteralPath $log -Pattern '^stereo-pose-uniforms: bytes=(\d+) pushes=(\d+)$')
  if($uploads.Count -ne 32 -or $uniforms.Count -ne 32){throw 'Incomplete actual completed-pair accounting'}
  $records=@(for($i=0;$i -lt 32;++$i) {
   $up=$uploads[$i].Matches[0].Groups;$un=$uniforms[$i].Matches[0].Groups
   if([long]$up[5].Value -gt 16MB){throw 'Source storage exceeded budget'}
   if($route.Reference -and ([long]$un[1].Value -or [int]$un[2].Value)){throw 'Buffered policy still used inline constants'}
   @{input=[long]$up[1].Value;storage_upload=[long]$up[2].Value;copies=[long]$up[4].Value;
    uniform_bytes=[long]$un[1].Value;uniform_pushes=[int]$un[2].Value}
  })
  if(!$route.Reference -and !@($records | Where-Object {$_.uniform_pushes -gt 0}).Count){throw 'No command-owned pose constants consumed'}
  $traffic+=,@($records)
 }
 for($i=0;$i -lt 32;++$i){if($traffic[0][$i].input -ne $traffic[1][$i].input){throw 'Changed authored model input demand'}}
 $copyTotals=@(foreach($records in $traffic){($records | Measure-Object copies -Sum).Sum})
 if($copyTotals[1] -ge $copyTotals[0]){throw 'No explicit storage-copy reduction'}
 $names=@('lava.bmp','lava.bmp.frame-8.bmp','lava.bmp.frame-16.bmp','lava.bmp.frame-24.bmp','lava.bmp.frame-32.bmp')
 foreach($directory in @($reference,$candidate)) {
  $actual=@(Get-ChildItem -LiteralPath $directory -Filter '*.bmp' | ForEach-Object {$_.Name})
  if(@(Compare-Object $actual $names).Count){throw 'Incomplete paired capture'}
 }
 foreach($name in $names) {
  if((Get-FileHash -LiteralPath (Join-Path $reference $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $candidate $name)).Hash){throw "Pose transport changed output: $($case.Name)/$name"}
 }
 $results+=@{case=$case.Name;exact_images=$names.Count;completed_pairs_per_policy=32;traffic=$traffic;copies=$copyTotals}
 Write-Output "PASS $($case.Name): five exact SBS images; actual constants and reduced storage copies"
}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed'}
if((Get-FileHash -LiteralPath $preferences).Hash -ne $preferencesHash){throw 'Saved preferences changed'}
@{complete=$true;sha256=$binaryHash;backend=$GpuBackend;results=$results;collected_existing=[bool]$CollectOnly;
 scope='Same-binary buffered versus command-owned model poses. Shader arithmetic and quality unchanged; constants carry their own GPU transport, not zero upload. Not sustained speed or physical-device acceptance.'} |
 ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'results.json')
