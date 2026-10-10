param([string]$Executable='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/gpu-fast-ab',
 [ValidateSet('direct3d12','vulkan')][string]$GpuDriver='direct3d12',
 [ValidateSet('ORIGINAL','EX')][string[]]$Experiences=@('ORIGINAL'),
 [string[]]$Levels=@('LEVEL1_1'),
 [ValidateRange(1,10)][int[]]$RenderScale=@(1),
 [ValidateSet('4_3','16_9','32_9')][string[]]$DisplayMode=@('16_9'),
 [ValidateRange(0,8000)][int]$Ticks=1000,
 [ValidateRange(1,600)][int]$Frames=60,
 # STARFOX_TEST_PRESSES entries, e.g. '10:0x2000' (Select switches the view).
 [string]$Presses='')
# GPU FAST must be pixel-identical to GPU ACCURATE. Run the same scenario
# under both and require byte-identical native and final captures.
$ErrorActionPreference='Stop'
$proof=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $proof -Force | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
 $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$compared=0
try {
 $settings=@{
  SDL_AUDIODRIVER='dummy';SDL_GPU_DRIVER=$GpuDriver;STARFOX_TEST_HIDDEN='1';
  STARFOX_TEST_FRAMES="$Frames";STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS="$Ticks";
  STARFOX_TEST_UNPACED='1';STARFOX_TEST_PRESENTATION_FPS='60';STARFOX_TEST_TIMING_MODE='ORIGINAL';
  STARFOX_TEST_VSYNC='0';STARFOX_TEST_GOD_MODE='1'
 }
 foreach($name in @('MSU1','ENHANCED','SEPARATED_MODELS','ANTI_ALIASING','2D_FILTER',
  'RTX_LIGHTING','BLOOM','BLOOM_2D','EFFECT','WORLD_EFFECT','MODEL_SMOOTHING',
  'HDR_EFFECT','CHROMATIC_ABERRATION','RAY_TRACING','SOFTWARE_SHADOWS','REFLECTIVE_SURFACES',
  'DLSS_SELECTION','FSR1_SELECTION','NEURAL_SELECTION','STEREO_OUTPUT','LANGUAGE',
  'MANIPULATION','MATERIAL','SHOW_FPS')) {$settings["STARFOX_TEST_$name"]='0'}
 for($field=0;$field -lt 6;++$field){$settings["STARFOX_TEST_ENVIRONMENT_$field"]='0'}
 if($Presses){$settings.STARFOX_TEST_PRESSES=$Presses}
 foreach($entry in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
 foreach($experience in $Experiences) {foreach($level in $Levels) {foreach($scale in $RenderScale) {foreach($display in $DisplayMode) {
  $env:STARFOX_TEST_EXPERIENCE=$experience;$env:STARFOX_TEST_RENDER_SCALE="$scale";$env:STARFOX_TEST_DISPLAY_MODE=$display
  $arguments=if($experience -eq 'EX') {"tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt $level"}
   else {"upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT $level"}
  $stem="$experience-$level-$GpuDriver-${scale}x-$display"
  foreach($renderer in 'GPU_ACCURATE','GPU_FAST') {
   $env:STARFOX_TEST_RENDERER=$renderer
   $env:STARFOX_CAPTURE_PATH=Join-Path $proof "$stem-$renderer-native.bmp"
   $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proof "$stem-$renderer-presentation.bmp"
   $log=Join-Path $proof "$stem-$renderer.log"
   $process=Start-Process $Executable -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError $log
   $handle=$process.Handle
   if(!$process.WaitForExit(120000)) {throw "A/B run still running: PID $($process.Id), log $log"}
   if($process.ExitCode -ne 0) {throw "A/B run failed: $stem $renderer (see $log)"}
  }
  foreach($kind in 'native','presentation') {
   $a=Join-Path $proof "$stem-GPU_ACCURATE-$kind.bmp";$b=Join-Path $proof "$stem-GPU_FAST-$kind.bmp"
   if(!(Test-Path -LiteralPath $a) -or !(Test-Path -LiteralPath $b)) {throw "Missing $kind capture: $stem"}
   if((Get-FileHash -LiteralPath $a).Hash -ne (Get-FileHash -LiteralPath $b).Hash) {
    throw "GPU FAST differs from GPU ACCURATE ($kind): $stem"
   }
  }
  ++$compared
  Write-Output "Exact GPU FAST = GPU ACCURATE (native + final): $stem"
 }}}}
 Write-Output "Compared $compared GPU ACCURATE/GPU FAST pairs ($GpuDriver)"
} finally {
 Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
 foreach($entry in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
