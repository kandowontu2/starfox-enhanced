param([string]$Executable='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/span-clear-timing',
 [ValidateSet('span-clear','clip-scratch','stereo-encoding','fp64-multiply','ray-hit-stability','sbs-ray-casters','ray-pipeline-sharing','ray-prebuild-cache','stereo-ray-submissions','single-face-spans','environment-reflection')][string]$Comparison='span-clear',
 [string]$ReferenceExecutable='',
 [ValidateSet('direct3d12','vulkan')][string]$HighPowerBackend='direct3d12',
 [switch]$DedicatedOnly,
 # Hidden/occluded D3D12 swapchains can stall independently of scene work.
 # Record visibility explicitly; never mix the two modes in one comparison.
 [switch]$Visible,
 [ValidateSet(0,2)][int]$Stereo=2,
 [ValidateRange(1,10)][int]$RenderScale=1,
 [ValidateRange(120,10000)][int]$DefaultFrames=360,
 [ValidateRange(120,10000)][int]$IntegratedFrames=240)
$ErrorActionPreference='Stop'
if(($Comparison -in @('fp64-multiply','sbs-ray-casters')) -ne [bool]$ReferenceExecutable){throw 'fp64-multiply and sbs-ray-casters require a preserved ReferenceExecutable; other comparisons use one executable'}
if($Comparison -eq 'stereo-encoding' -and $Stereo -ne 2){throw 'Stereo encoding comparison requires full SBS'}
if($Comparison -eq 'sbs-ray-casters' -and $Stereo -ne 2){throw 'Active ray caster comparison requires full SBS'}
if($Comparison -eq 'stereo-ray-submissions' -and $Stereo -ne 2){throw 'Ray-traced submission comparison requires full SBS'}
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a fresh timing directory'}
if(Get-Process starfox_pc -ErrorAction SilentlyContinue){throw 'Close other game instances before quiet timing'}
New-Item -ItemType Directory -Path $output | Out-Null
$exe=[IO.Path]::GetFullPath($Executable)
$hash=(Get-FileHash -LiteralPath $exe).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName($exe)) 'pregame.cfg'
$preferencesHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
$reference=if($ReferenceExecutable){[IO.Path]::GetFullPath($ReferenceExecutable)}else{$exe}
$referenceHash=(Get-FileHash -LiteralPath $reference).Hash
$referencePreferences=Join-Path ([IO.Path]::GetDirectoryName($reference)) 'pregame.cfg'
$referencePreferencesHash=if(Test-Path -LiteralPath $referencePreferences){(Get-FileHash -LiteralPath $referencePreferences).Hash}else{''}
$records=@()
$complete=$false
$defaultBackend=if($Comparison -eq 'clip-scratch'){'vulkan'}else{$HighPowerBackend}
$adapters=@(@{Name="default-$defaultBackend";Backend=$defaultBackend;LowPower=$false;Frames=$DefaultFrames})
if(!$DedicatedOnly){$adapters+=@{Name='intel-vulkan';Backend='vulkan';LowPower=$true;Frames=$IntegratedFrames}}
$expectedRuns=3*$adapters.Count*2*2
try {
 foreach($trial in 1,2,3) {
  foreach($adapter in $adapters) {
   foreach($level in @('LEVEL1_1','LEVEL3_5')) {
    $candidate=if($Comparison -eq 'environment-reflection'){'fused-env'}elseif($Comparison -eq 'single-face-spans'){'direct-face'}elseif($Comparison -eq 'fp64-multiply'){'fixed-limbs'}elseif($Comparison -eq 'sbs-ray-casters'){'active-casters'}elseif($Comparison -eq 'ray-pipeline-sharing'){'shared-pipelines'}elseif($Comparison -eq 'ray-prebuild-cache'){'cached-sizes'}elseif($Comparison -eq 'stereo-ray-submissions'){'joined'}elseif($Comparison -eq 'clip-scratch'){'small'}elseif($Comparison -eq 'stereo-encoding'){'parallel'}elseif($Comparison -eq 'ray-hit-stability'){'stable-hits'}else{'bounds'}
    $modes=if($trial -eq 2){@($candidate,'full')}else{@('full',$candidate)}
    foreach($mode in $modes) {
     $directory=Join-Path $output "$($adapter.Name)-$level-$mode-t$trial"
     $options=@{Executable=$exe;OutputDirectory=$directory;Frames=$adapter.Frames;Fps=120;
      GpuDriver=$adapter.Backend;LowPowerGpu=$adapter.LowPower;Stereo=$Stereo;RenderScale=$RenderScale;Visible=[bool]$Visible;
      Experiences=@('ORIGINAL');Renderers=@('GPU');Levels=@($level);GodMode=$true;PrerollTicks=1000}
     if($Comparison -in @('ray-hit-stability','sbs-ray-casters','ray-pipeline-sharing','ray-prebuild-cache','stereo-ray-submissions','single-face-spans','environment-reflection')) {
      $options.EnhancedGround=$true;$options.GroundMaterial=5;$options.EnhancedSky=$true
      $options.RayTracing=1;$options.Reflections=3;$options.Bloom=2
     }
     if($Comparison -in @('fp64-multiply','sbs-ray-casters')) {
      if($mode -eq 'full'){$options.Executable=$reference}
     } elseif($Comparison -eq 'environment-reflection') {
      if($mode -eq 'fused-env'){$options.FusedEnvironmentReflection=$true}else{$options.SeparateEnvironmentReflection=$true}
     } elseif($Comparison -eq 'single-face-spans') {
      if($mode -eq 'direct-face'){$options.DirectSingleFace=$true}else{$options.BinnedSingleFace=$true}
     } elseif($Comparison -eq 'ray-hit-stability') {
      $options.DeterministicRayHits=$mode -eq 'stable-hits'
     } elseif($Comparison -eq 'ray-pipeline-sharing') {
      $options.SharedRayPipelines=$mode -eq 'shared-pipelines'
     } elseif($Comparison -eq 'ray-prebuild-cache') {
      $options.DisableRayPrebuildCache=$mode -eq 'full'
      $options.CachedRayPrebuildSizes=$mode -eq 'cached-sizes'
     } elseif($Comparison -eq 'stereo-ray-submissions') {
      if($mode -eq 'joined'){$options.JoinedStereoSubmissions=$true}else{$options.SplitStereoSubmissions=$true}
     } elseif($Comparison -eq 'stereo-encoding') {
      if($mode -eq 'parallel'){$options.ParallelStereoEncoding=$true}else{$options.SerialStereoEncoding=$true}
     } elseif($Comparison -eq 'clip-scratch') {
      if($mode -eq 'small'){$options.SmallClip=$true}else{$options.FullClip=$true}
     } else {
      if($mode -eq 'bounds'){$options.SpanBoundsClear=$true}else{$options.FullSpanClear=$true}
     }
     & (Join-Path $PSScriptRoot 'benchmark_native_defaults.ps1') @options
     $log=Join-Path $directory "ORIGINAL-$level-GPU.log"
     $text=Get-Content -LiteralPath $log -Raw
     if($text -notmatch '(?m)^frame-work-distribution-us median=(\d+) p95=(\d+) p99=(\d+) max=(\d+)\r?$') {
      throw "Missing frame-work distribution: $log"
     }
     $median=[int]$Matches[1];$p95=[int]$Matches[2];$p99=[int]$Matches[3];$maximum=[int]$Matches[4]
     if($text -notmatch "(?m)^test-gpu-adapter: ([^\r\n]+) driver=$($adapter.Backend)\r?`$") {
      throw "Timing did not record its actual adapter/backend: $log"
     }
     $actualAdapter=$Matches[1]
     if($adapter.LowPower) {
      if($text -notmatch '(?m)^test-gpu-adapter: (Intel[^\r\n]*) driver=vulkan\r?$') {
       throw "Integrated timing did not use an actual Intel Vulkan device: $log"
      }
      $actualAdapter=$Matches[1]
     }
     $records+=@{adapter=$adapter.Name;actual_adapter=$actualAdapter;backend=$adapter.Backend;
      comparison=$Comparison;trial=$trial;mode=$mode;scale=$RenderScale;visible=[bool]$Visible;median_us=$median;p95_us=$p95;p99_us=$p99;max_us=$maximum;
      frames=$adapter.Frames;warmup=60;fps=120;stereo=$Stereo;preroll=1000;level=$level;
      sha256=$(if($Comparison -in @('fp64-multiply','sbs-ray-casters') -and $mode -eq 'full'){$referenceHash}else{$hash});log=$log}
     @{complete=$false;sha256=$hash;visible=[bool]$Visible;results=$records} | ConvertTo-Json -Depth 6 |
      Set-Content -LiteralPath (Join-Path $output 'results.json')
    }
   }
  }
  Write-Output "Completed quiet trial ${trial}: $($records.Count) records"
 }
 if($records.Count -ne $expectedRuns){throw 'Incomplete three-trial timing matrix'}
 if((Get-FileHash -LiteralPath $exe).Hash -ne $hash){throw 'Executable changed during timing'}
 if((Get-FileHash -LiteralPath $reference).Hash -ne $referenceHash){throw 'Reference executable changed during timing'}
 $afterPreferencesHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
 if($afterPreferencesHash -ne $preferencesHash){throw 'Saved preferences changed during timing'}
 $afterReferencePreferencesHash=if(Test-Path -LiteralPath $referencePreferences){(Get-FileHash -LiteralPath $referencePreferences).Hash}else{''}
 if($afterReferencePreferencesHash -ne $referencePreferencesHash){throw 'Reference preferences changed during timing'}
 $complete=$true
} finally {
 @{complete=$complete;sha256=$hash;visible=[bool]$Visible;results=$records} | ConvertTo-Json -Depth 6 |
  Set-Content -LiteralPath (Join-Path $output 'results.json')
}
Write-Output "All $expectedRuns quiet timings passed; executable hashes/settings, complete native pairs and actual adapters verified ($Comparison)."
