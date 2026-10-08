param([string]$Binary='build/current/starfox_pc.exe',
 [Parameter(Mandatory)][string]$OutputDirectory,
 [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12',
 [switch]$LowPowerGpu,
 [string]$ExpectedAdapter,
 [ValidateRange(120,10000)][int]$Frames=480,
 [ValidateRange(1,10)][int]$RenderScale=2,
 [ValidateSet('radix16','parallel-spans','default-spans','ordered-queue','default-ordered-queue','fixed-layers')][string]$Comparison='radix16',
 [ValidateSet('LEVEL1_1','LEVEL3_5')][string]$Stage='LEVEL1_1',
 [switch]$TraceSceneGpuDraws,
 [switch]$Quiet,
 [switch]$Lava)
$ErrorActionPreference='Stop'
if($Quiet -and $TraceSceneGpuDraws){throw 'Quiet timing must not enable per-draw GPU traces'}
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Use a fresh timing directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$exe=[IO.Path]::GetFullPath($Binary)
$sha=(Get-FileHash -LiteralPath $exe).Hash
$prefs=Join-Path ([IO.Path]::GetDirectoryName($exe)) 'pregame.cfg'
$prefsHash=if(Test-Path -LiteralPath $prefs){(Get-FileHash -LiteralPath $prefs).Hash}else{''}
function CompetingProcesses {
 @(Get-Process | Where-Object {$_.ProcessName -match '^(starfox_|ninja$|cmake$|cc1(?:plus)?$|dxc$|clang|gcc$|g\+\+$|c\+\+$|cl$|link$|msbuild$)'})
}
function DescribeCompeting($observed) {
 foreach($entry in $observed) {
  $record=Get-CimInstance Win32_Process -Filter "ProcessId=$($entry.Id)" -ErrorAction SilentlyContinue
  $parent=if($record){Get-CimInstance Win32_Process -Filter "ProcessId=$($record.ParentProcessId)" -ErrorAction SilentlyContinue}
  $directory='';$sourceName='';$sourceDirectory=''
  # Keep only build/source paths, not raw command lines or possible secrets.
  if($record.CommandLine -cmatch '(?:--build|-B| -C)\s+("[^"]+"|[^\s]+)'){$directory=$Matches[1]}
  if($record.CommandLine -cmatch '(?:^|\s)-c\s+("[^"]+"|[^\s]+)'){
   $sourcePath=$Matches[1].Trim('"');$sourceName=[IO.Path]::GetFileName($sourcePath);$sourceDirectory=[IO.Path]::GetDirectoryName($sourcePath)
  }
  [pscustomobject]@{id=$entry.Id;name=$entry.ProcessName;parent_id=$record.ParentProcessId;
   parent_name=$parent.Name;build_directory=$directory;source_file=$sourceName;source_directory=$sourceDirectory}
 }
}
$results=@()
$adapterName=''
# Both orderings, same executable and no simultaneous GPU/build processes.
$order=@('reference',$Comparison,$Comparison,'reference',$Comparison,'reference','reference',$Comparison)
for($run=0;$run -lt $order.Count;++$run) {
 $name=('{0:D2}-{1}' -f ($run+1),$order[$run])
 $directory=Join-Path $output $name
 $stdout=Join-Path $output "$name-driver.log"
 $stderr=Join-Path $output "$name-driver.err.log"
 $competing=CompetingProcesses
 if($competing.Count){
  @{run=$name;phase='preflight';sha256=$sha;overlap=@($competing.ProcessName | Sort-Object -Unique);
    details=@(DescribeCompeting $competing)} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'rejected.json')
  throw "Timing was not started; competing processes: $($competing.ProcessName -join ', ')"
 }
 $benchmark=Join-Path $PSScriptRoot 'benchmark_native_defaults.ps1'
 $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"{0}"' -f $benchmark),
  '-Executable',('"{0}"' -f $exe),'-OutputDirectory',('"{0}"' -f $directory),
  '-Frames',"$Frames",'-Fps','120','-GpuDriver',$GpuBackend,'-Stereo','2','-RenderScale',"$RenderScale",
  '-Renderers','GPU','-Experiences',$(if($Lava){'EX'}else{'ORIGINAL'}),
  '-Levels',$(if($Lava){'LEVEL6_6'}else{$Stage}),'-GodMode','-PrerollTicks','1000')
 if(!$Quiet){$arguments+=@('-TraceSceneGpuTimestamps','-TraceEffectsGpuTimestamps')}
 if($Lava){$arguments+=@('-EnhancedGround','-GroundMaterial','9','-EnhancedSky')}
 if($TraceSceneGpuDraws){$arguments+='-TraceSceneGpuDraws'}
 if($LowPowerGpu){$arguments+='-LowPowerGpu'}
 if($Comparison -eq 'radix16'){
  $arguments+=if($order[$run] -eq 'radix16'){'-Radix16Clip'}else{'-FullClip'}
 }elseif($Comparison -eq 'fixed-layers'){
  # Same automatic clipping, queueing, resolution and effects in both paths.
  # Only the reference repeats the pair's eligible screen-fixed producers.
  if($order[$run] -eq 'reference'){$arguments+='-DuplicateFixedLayers'}
 }elseif($Comparison -in @('ordered-queue','default-ordered-queue')){
  $arguments+='-FullClip'
  if($order[$run] -eq 'reference'){$arguments+='-StereoWait'}
  elseif($Comparison -eq 'ordered-queue'){$arguments+='-OrderedStereoQueue'}
 }else{
  $arguments+='-FullClip'
  $arguments+=if($order[$run] -eq 'parallel-spans'){'-ParallelSpanClear'}elseif($order[$run] -eq 'default-spans'){'-DefaultSpanClear'}else{'-FullSpanClear'}
 }
 $process=Start-Process powershell -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
 $handle=$process.Handle
 $overlap=@()
 $overlapDetails=@()
 do {
  # The child app itself is expected. Other tests/builds invalidate the run,
  # but are never killed and this same live process is still awaited.
  $other=@(CompetingProcesses | Where-Object {$_.ProcessName -ne 'starfox_pc'})
  $apps=@(Get-Process -Name starfox_pc -ErrorAction SilentlyContinue)
  if($other.Count -or $apps.Count -gt 1){
   $overlap+=@($other.ProcessName)+$(if($apps.Count -gt 1){'multiple-starfox_pc'})
   if(!$overlapDetails.Count){$overlapDetails=@(DescribeCompeting $other)}
  }
  $finished=$process.WaitForExit(500)
 } while(!$finished)
 $process.Refresh()
 if($overlap.Count){@{run=$name;sha256=$sha;overlap=@($overlap | Sort-Object -Unique);details=$overlapDetails} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'rejected.json');throw 'Observed concurrent work: timing rejected'}
 if($process.ExitCode){throw "Benchmark failed: $stdout / $stderr"}
 if((Get-FileHash -LiteralPath $exe).Hash -ne $sha){throw 'Executable changed during timing'}
 $logFiles=@(Get-ChildItem -LiteralPath $directory -Filter '*.log')
 if($logFiles.Count -ne 1){throw 'Missing or ambiguous runtime log'}
 $log=$logFiles[0].FullName
 $content=Get-Content -LiteralPath $log -Raw
 if($content -notmatch "(?m)^stereo-presentation-count: $Frames\r?$"){throw 'Incomplete pair count'}
 if($content -notmatch "(?m)^test-gpu-adapter: [^\r\n]+ driver=$GpuBackend\r?$"){throw 'Wrong actual backend'}
 $adapter=[regex]::Match($content,"(?m)^test-gpu-adapter: ([^\r\n]+) driver=$GpuBackend\r?$").Groups[1].Value
 if(($ExpectedAdapter -and $adapter -ne $ExpectedAdapter) -or ($adapterName -and $adapter -ne $adapterName)){
  throw 'Wrong or changing actual adapter'
 }
 $adapterName=$adapter
 if($content -match 'stereo failure:|replaying complete frame|GPU effects fallback:|VK_ERROR_|device lost|clip-(identity|projection-cache):'){throw 'Fallback or combined experiment'}
 $usedRadix=$content.Contains('clip-radix16: exact binary64 division')
 if($usedRadix -ne ($order[$run] -eq 'radix16')){throw 'Wrong divider route'}
 if($Comparison -in @('ordered-queue','default-ordered-queue')){
  $expectedOwners=if($order[$run] -eq 'reference'){0}else{2}
  foreach($marker in 'composite-ordered-queue: submitted','effects-ordered-queue: submitted'){
   if([regex]::Matches($content,'(?m)^'+[regex]::Escape($marker)+'\r?$').Count -ne $expectedOwners){
    throw 'Wrong per-eye submission retirement route'
   }
  }
  $policy=if($order[$run] -eq 'reference'){'forced synchronous'}elseif($Comparison -eq 'ordered-queue'){'forced bounded'}else{'automatic bounded'}
  if(!$content.Contains("stereo-layer-queue-policy: $policy")){throw 'Wrong automatic/forced per-eye queue policy'}
 }
 if($Comparison -eq 'fixed-layers'){
  $expected=if($order[$run] -eq 'reference'){'duplicated'}else{'pair-shared'}
  if([regex]::Matches($content,'(?m)^stereo-fixed-layer-policy: '+$expected+'\r?$').Count -ne 1 -or
     [regex]::Matches($content,'(?m)^stereo-fixed-layer-policy: ').Count -ne 1){throw 'Wrong fixed-layer producer route'}
 }
 if($Comparison -in @('parallel-spans','default-spans')){
  $clearMode=if($order[$run] -eq 'reference'){'full'}else{'parallel'}
  if(!$content.Contains("span-clear: $clearMode")){throw 'Wrong span clear route'}
  if($order[$run] -eq 'default-spans' -and
     (!$content.Contains('span-clear-policy: automatic ') -or $content.Contains('span-clear-policy: forced '))){throw 'Automatic span clear used an override'}
 }
 $metric=[regex]::Match($content,'(?m)^frame-work-distribution-us median=(\d+) p95=(\d+) ')
 if(!$metric.Success){throw 'Missing frame distribution'}
 $gpu=$null
 if(!$Quiet){
  $gpuJson=Join-Path $directory 'gpu-timing.json'
  & (Join-Path $PSScriptRoot 'summarize_gpu_timestamps.ps1') -Log $log -Output $gpuJson -WarmupFrames 60 | Out-Null
  $gpu=Get-Content -LiteralPath $gpuJson -Raw | ConvertFrom-Json
 }elseif($content -match 'gpu-time:|gpu-timestamps:|stereo-fixed-layer:'){
  throw 'Quiet timing unexpectedly enabled per-frame tracing'
 }
 if($TraceSceneGpuDraws){
  & (Join-Path $PSScriptRoot 'summarize_scene_draws.ps1') -Log $log -Output (Join-Path $directory 'draw-timing.json') -WarmupFrames 60 | Out-Null
 }
 $row=@{run=$name;route=$order[$run];median_us=[int64]$metric.Groups[1].Value;p95_us=[int64]$metric.Groups[2].Value;
  scene_us=$(if($gpu){$gpu.GpuMeanUsPerCompletePair.SceneUs}else{$null});
  effects_us=$(if($gpu){$gpu.GpuMeanUsPerCompletePair.EffectsUs}else{$null})}
 $results+=,$row
 if($Quiet){Write-Output "$name frame median=$($row.median_us) p95=$($row.p95_us) us"}
 else{Write-Output "$name scene=$($row.scene_us) us frame median=$($row.median_us) p95=$($row.p95_us)"}
}
$after=if(Test-Path -LiteralPath $prefs){(Get-FileHash -LiteralPath $prefs).Hash}else{''}
if($after -ne $prefsHash){throw 'Saved preferences changed'}
@{sha256=$sha;backend=$GpuBackend;adapter=$adapterName;low_power_requested=[bool]$LowPowerGpu;
 frames=$Frames;warmup=60;scale=$RenderScale;lava=[bool]$Lava;comparison=$Comparison;stage=$Stage;quiet=[bool]$Quiet;
 preferences_unchanged=$true;no_observed_overlap=$true;results=$results;
 scope=$(if($Quiet){'Unpaced hidden-window complete SBS pairs, no readback or per-frame GPU tracing; not visible/paced display FPS or device acceptance.'}
  else{'Instrumented complete SBS pairs, not sustained uninstrumented FPS or native ray-queue timing.'})} |
 ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'results.json')
