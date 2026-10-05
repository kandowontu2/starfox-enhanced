param([string]$Executable='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/native-defaults-profile',
 [ValidateRange(120,10000)][int]$Frames=360,
 [ValidateSet(60,120,240)][int]$Fps=60,
 [switch]$Capture,
 [switch]$Visible,
 [switch]$Paced,
 [switch]$PresentPacing,
 [switch]$Vsync,
 [ValidateSet('default','direct3d12','vulkan')][string[]]$GpuDriver=@('default'),
 [switch]$DisableDlssRuntime,
 [ValidateSet('ORIGINAL','EX')][string[]]$Experiences=@('ORIGINAL','EX'),
 # GPU (GPU_ACCURATE) is the original path, shown as "GPU" in the menu.
 [ValidateSet('SOFTWARE','GPU','GPU_ACCURATE','GPU_FAST')][string[]]$Renderers=@('SOFTWARE','GPU'),
 # Any stage label from the experience's symbol map, e.g. LEVEL1_2.
 [string[]]$Levels=@('LEVEL1_1','LEVEL2_3'),
 # 5-10 require GPU_FAST; other renderers are clamped to 4x.
 [ValidateRange(1,10)][int[]]$RenderScale=@(1),
 [ValidateSet('4_3','16_9','32_9')][string[]]$DisplayMode=@('4_3'),
 # Forwarded as STARFOX_TEST_ASTEROID_MODELS; inert in builds without the
 # 3D asteroid option.
 [string]$AsteroidModels='',
 # Adds STARFOX_TRACE_SCENE_COST work counters. Tracing writes a line per
 # frame, so keep traced runs separate from timing comparisons.
 [switch]$TraceSceneCost,
 [switch]$RealAudio,
 [switch]$EnhancedGround,
 [switch]$EnhancedSky,
 [switch]$UnbatchedTerrain,
 # GPU FAST A/B: restore the per-model full-frame raster pass.
 [switch]$FullFrameModelRaster,
 # Prefer the low-power adapter (an integrated GPU on hybrid systems).
 [switch]$LowPowerGpu,
 [switch]$GodMode,
 [ValidateRange(0,1000000)][int]$SlowFrameUs=0,
 # Source ticks run before measuring. The default skips LEVEL1_1's opening
 # launch tunnel; 0 measures it (about 1,030 frames at 60 Hz).
 [ValidateRange(0,8000)][int]$PrerollTicks=1000,
 # Window (swapchain) size as WIDTHxHEIGHT, e.g. 3840x2160; empty keeps 1024x896.
 [ValidatePattern('^(\d+x\d+)?$')][string]$WindowSize='')
$ErrorActionPreference='Stop'
if($Vsync -and !$Visible){throw 'VSync measurements require Visible; hidden swapchains can be heavily throttled.'}
foreach($experience in $Experiences) {
 $symbols=if($experience -eq 'EX'){'assets/symbols/starfox-ex.txt'}else{'upstream-ultrastarfox/SYMBOLS.TXT'}
 $available=@(Get-Content -LiteralPath $symbols | ForEach-Object {
  if($_ -match '^(LEVEL(?:[1-7]_[1-9]|_BLACKHOLE|_SPECIAL|_COMET))\s'){$Matches[1]}
 } | Sort-Object -Unique)
 foreach($level in $Levels) {if($level -notin $available){throw "Unknown stage: $experience $level"}}
}
$proof=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $proof -Force | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
 $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$rows=New-Object System.Collections.Generic.List[object]
try {
 $settings=@{
  SDL_AUDIODRIVER='dummy';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES="$Frames";
  STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS="$PrerollTicks";STARFOX_TEST_UNPACED='1';
  STARFOX_TEST_PRESENTATION_FPS="$Fps";STARFOX_TEST_TIMING_MODE='ORIGINAL';
  STARFOX_TEST_VSYNC='0';
  STARFOX_TRACE_PROFILE='1';STARFOX_TRACE_PROFILE_DISTRIBUTION='1';STARFOX_TEST_PROFILE_WARMUP='60'
 }
 foreach($name in @('MSU1','ENHANCED','SEPARATED_MODELS','ANTI_ALIASING','2D_FILTER',
  'RTX_LIGHTING','BLOOM','BLOOM_2D','EFFECT','WORLD_EFFECT','MODEL_SMOOTHING',
  'HDR_EFFECT','CHROMATIC_ABERRATION','RAY_TRACING','SOFTWARE_SHADOWS','REFLECTIVE_SURFACES',
  'DLSS_SELECTION','FSR1_SELECTION','NEURAL_SELECTION','STEREO_OUTPUT','LANGUAGE',
  'MANIPULATION','MATERIAL')) {$settings["STARFOX_TEST_$name"]='0'}
 # Baseline runs must not inherit persisted terrain/sky upgrades from the menu.
 for($field=0;$field -lt 6;++$field){$settings["STARFOX_TEST_ENVIRONMENT_$field"]='0'}
 if($EnhancedGround){$settings.STARFOX_TEST_ENVIRONMENT_0='1'}
 if($EnhancedSky){$settings.STARFOX_TEST_ENVIRONMENT_3='1'}
 if($UnbatchedTerrain){$settings.STARFOX_TEST_UNBATCHED_TERRAIN='1'}
 if($FullFrameModelRaster){$settings.STARFOX_TEST_FULL_FRAME_MODEL_RASTER='1'}
 if($LowPowerGpu){$settings.STARFOX_TEST_LOW_POWER_GPU='1'}
 if($AsteroidModels){$settings.STARFOX_TEST_ASTEROID_MODELS=$AsteroidModels}
 if($TraceSceneCost){$settings.STARFOX_TRACE_SCENE_COST='1'}
 if($SlowFrameUs){$settings.STARFOX_TRACE_SLOW_FRAME_US=[string]$SlowFrameUs}
 if($WindowSize){$settings.STARFOX_TEST_WINDOW_SIZE=$WindowSize}
 $settings.STARFOX_TEST_GOD_MODE=if($GodMode){'1'}else{'0'}
 if($Visible) {$settings.Remove('STARFOX_TEST_HIDDEN')}
 if($DisableDlssRuntime) {
  # Exercise the optional-runtime failure path without renaming installed DLLs.
  $settings.STARFOX_DLSS_ADAPTER=Join-Path $proof 'intentionally-unavailable-adapter.dll'
  $settings.STARFOX_DLSS_BINARIES=$proof
 }
 if($Vsync){$settings.STARFOX_TEST_VSYNC='1'}
 if($Paced) {$settings.Remove('STARFOX_TEST_UNPACED')}
 if($PresentPacing) {
  if(!$Paced){throw 'PresentPacing requires Paced'}
  $settings.STARFOX_TEST_PRESENT_PACING='1'
 }
 if($RealAudio) {$settings.Remove('SDL_AUDIODRIVER')}
 foreach($entry in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
 foreach($driver in $GpuDriver) {
  if($driver -ne 'default'){$env:SDL_GPU_DRIVER=$driver}else{Remove-Item -LiteralPath Env:SDL_GPU_DRIVER -ErrorAction SilentlyContinue}
  foreach($experience in $Experiences) {foreach($level in $Levels) {foreach($renderer in $Renderers) {
  foreach($scale in $RenderScale) {foreach($display in $DisplayMode) {
   $env:STARFOX_TEST_EXPERIENCE=$experience;$env:STARFOX_TEST_RENDERER=$renderer
   $env:STARFOX_TEST_RENDER_SCALE="$scale";$env:STARFOX_TEST_DISPLAY_MODE=$display
   $name="$experience-$level-$renderer-$driver-${scale}x-$display";$log=Join-Path $proof "$name.log"
   # Readback/BMP encoding stalls the last frame; keep visual checks separate
   # from timing runs unless explicitly requested.
   if($Capture) {$env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proof "$name.bmp"}
   $arguments=if($experience -eq 'EX') {"tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt $level"}
    else {"upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT $level"}
   $process=Start-Process $Executable -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError $log
   $handle=$process.Handle
   $timeoutMs=[Math]::Max(60000,[int](1000*$Frames/$Fps)+60000)
   if(!$process.WaitForExit($timeoutMs)) {throw "Benchmark still running: PID $($process.Id), log $log"}
   if($process.ExitCode -ne 0) {throw "Benchmark failed: $name"}
   $lines=Get-Content -LiteralPath $log
   $summary=@($lines | Where-Object {$_ -match '^(logic/audio|frame-work|present-interval|input-to-present|render)-distribution-us|^render-profile-us|^presentation-pacing:'})
   if($summary.Count -ne 7) {throw "Incomplete profiling metrics: $name"}
   $counters=@($lines | Where-Object {$_ -match '^scene-counters-distribution '})
   if($TraceSceneCost -and $counters.Count -ne 8) {throw "Incomplete scene counters: $name"}
   Write-Output $name;Write-Output $summary;Write-Output $counters
   $row=[ordered]@{experience=$experience;level=$level;renderer=$renderer;driver=$driver;scale=$scale;display=$display;traced=[bool]$TraceSceneCost}
   foreach($line in $summary) {
    if($line -match '^(\S+)-distribution-us median=(\d+) p95=(\d+) p99=(\d+) max=(\d+)') {
     $metric=$Matches[1] -replace '/','-'
     $row["$metric-median-us"]=[int64]$Matches[2];$row["$metric-p95-us"]=[int64]$Matches[3]
     $row["$metric-p99-us"]=[int64]$Matches[4];$row["$metric-max-us"]=[int64]$Matches[5]
    }
   }
   foreach($line in $counters) {
    if($line -match '^scene-counters-distribution (\S+) median=(\d+) p95=(\d+) p99=(\d+) max=(\d+)') {
     $row["$($Matches[1])-median"]=[int64]$Matches[2];$row["$($Matches[1])-p95"]=[int64]$Matches[3]
     $row["$($Matches[1])-max"]=[int64]$Matches[5]
    }
   }
   $rows.Add([pscustomobject]$row)
  }}}}}
 }
} finally {
 # Keep partial results: a failed run late in a long matrix should not
 # discard the rows already measured.
 if($rows.Count) {$rows | Export-Csv -LiteralPath (Join-Path $proof 'summary.csv') -NoTypeInformation}
 Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
 foreach($entry in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
