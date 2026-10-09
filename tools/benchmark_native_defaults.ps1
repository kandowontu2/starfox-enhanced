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
 [switch]$LowPowerGpu,
 [switch]$TraceSceneCost,
 [switch]$TraceSceneGpuTimestamps,
 [switch]$TraceSceneGpuDraws,
 [switch]$TraceEffectsGpuTimestamps,
 [switch]$FixedTemporalClock,
 [switch]$TraceVulkanSubmitTimestamps,
 [switch]$TraceNativeCost,
 [switch]$TraceNativeAll,
 [switch]$CopyEffectsInput,
 [switch]$BorrowEffectsInput,
 [switch]$TraceEffectsCopy,
 [switch]$SeparateEffectsCapture,
 [switch]$JoinedStereoSubmissions,
 [switch]$SplitStereoSubmissions,
 [switch]$ParallelStereoEncoding,
 [switch]$SerialStereoEncoding,
 [switch]$SplitCompositePipelines,
 [switch]$UnifiedCompositePipeline,
 [switch]$SpanBoundsClear,
 [switch]$ParallelSpanClear,
 [switch]$DefaultSpanClear,
 [switch]$FullSpanClear,
 [switch]$ColourSpanTrace,
 [switch]$CooperativeSpanTrace,
 [switch]$FullSpanTrace,
 [switch]$AllowBillboardOnlyScene,
 [switch]$SmallClip,
 [switch]$IdentityClip,
 [switch]$CachedProjectionClip,
 [switch]$Radix16Clip,
 [switch]$InteriorClip,
 [switch]$FullClip,
 [ValidateRange(0,8)][int]$Stereo=0,
 [ValidateRange(0,1)][int]$RayTracing=0,
 [ValidateRange(0,3)][int]$Reflections=0,
 [ValidateRange(0,3)][int]$Bloom=0,
 [ValidateRange(-1,3)][int]$ExModelWobble=-1,
 [switch]$DeterministicRayHits,
 [switch]$SharedRayPipelines,
 [switch]$DisableRayPrebuildCache,
 [switch]$CachedRayPrebuildSizes,
 [switch]$SerialStereoLayers,
 [switch]$StereoSnapshots,
 [switch]$DuplicateCpuInputs,
 [switch]$DuplicateFixedLayers,
 [switch]$ModelUploadReuse,
 [switch]$DuplicateModelUploads,
 [switch]$BufferedModelPoses,
 [switch]$InlineModelPoses,
 [switch]$TraceModelUploads,
 [switch]$TraceStereoInputUploads,
 [switch]$TraceModelCost,
 [switch]$TraceModelThreadCost,
 [switch]$DuplicateStereoFaces,
 [switch]$DuplicateStereoRays,
 [switch]$DuplicateStereoRayUploads,
 [switch]$SeparateModelMotion,
 [switch]$FusedVisibilityBsp,
 [switch]$SeparateVisibilityBsp,
 [switch]$FusedSmallModel,
 [switch]$SeparateSmallModel,
 [switch]$SeparateWorldModelMerge,
 [switch]$CopyModelBackgrounds,
 [switch]$GenericDedither,
 [switch]$FullTileDispatch,
 [switch]$OccupiedTiles,
 [switch]$RowTileRaster,
 [switch]$MaskTileRaster,
 [switch]$SeparateMaskTileRaster,
 [switch]$PixelOnlyRaster,
 [switch]$GenericRaster,
 [switch]$SeparateRowTileRaster,
 [switch]$BinnedSingleFace,
 [switch]$DirectSingleFace,
 [switch]$SeparateEnvironmentReflection,
 [switch]$FusedEnvironmentReflection,
 [switch]$DefaultEnvironmentReflection,
 [switch]$Taa,
 [ValidateRange(0,3)][int]$MotionBlurQuality=0,
 [switch]$OrderedStereoQueue,
 [switch]$StereoWait,
 [switch]$DisableDlssRuntime,
 [ValidateSet('ORIGINAL','EX')][string[]]$Experiences=@('ORIGINAL','EX'),
 [ValidateSet('SOFTWARE','GPU','GPU_ACCURATE','GPU_FAST')][string[]]$Renderers=@('SOFTWARE','GPU'),
 [string[]]$Levels=@('LEVEL1_1','LEVEL2_3'),
 [ValidateRange(1,10)][int[]]$RenderScale=@(1),
 [ValidateSet('4_3','16_9','32_9')][string[]]$DisplayMode=@('4_3'),
 [string]$AsteroidModels='',
 [switch]$RealAudio,
 [switch]$EnhancedGround,
 [ValidateRange(0,9)][int]$GroundMaterial=0,
 [switch]$EnhancedSky,
 [switch]$UnbatchedTerrain,
 # GPU FAST A/B: restore the per-model full-frame raster pass.
 [switch]$FullFrameModelRaster,
 [switch]$GodMode,
 [ValidateRange(0,1000000)][int]$PrerollTicks=1000,
 [ValidateRange(0,1000000)][int]$SlowFrameUs=0,
 [ValidatePattern('^(\d+x\d+)?$')][string]$WindowSize='')
$ErrorActionPreference='Stop'
if($FusedSmallModel -and $SeparateSmallModel){throw 'Choose single-pass or separate small-model stages, not both'}
if($OrderedStereoQueue -and (!$Stereo -or $SerialStereoLayers -or $StereoWait -or 'SOFTWARE' -in $Renderers)) {
 throw 'OrderedStereoQueue requires independent GPU stereo owners without forced waits'
}
if($DefaultSpanClear -and ($FullSpanClear -or $ParallelSpanClear -or $SpanBoundsClear)) {
 throw 'Automatic span clearing cannot be combined with a forced clear mode'
}
if($TraceModelThreadCost){$TraceModelCost=$true;$TraceSceneGpuDraws=$true}
if($TraceSceneGpuDraws){$TraceSceneGpuTimestamps=$true}
if($Vsync -and !$Visible){throw 'VSync measurements require Visible; hidden swapchains can be heavily throttled.'}
if($CopyEffectsInput -and $BorrowEffectsInput){throw 'Choose copied or borrowed effects input, not both'}
if($GroundMaterial -and !$EnhancedGround){throw 'GroundMaterial requires EnhancedGround'}
if($AllowBillboardOnlyScene -and !($ColourSpanTrace -or $CooperativeSpanTrace -or $FullSpanTrace)) {
 throw 'Billboard-only control requires an explicitly selected span tracer and actual model path counts'
}
if($TraceVulkanSubmitTimestamps -and ($GpuDriver.Count -ne 1 -or $GpuDriver[0] -ne 'vulkan')) {
 throw 'Native Vulkan timestamp diagnostics require GpuDriver vulkan'
}
if(($TraceSceneGpuTimestamps -or $TraceEffectsGpuTimestamps) -and ($GpuDriver.Count -ne 1 -or $GpuDriver[0] -notin @('direct3d12','vulkan'))) {
 throw 'GPU timestamp diagnostics require an explicit direct3d12 or vulkan backend'
}
if($Stereo -and 'SOFTWARE' -in $Renderers){throw 'Native stereo-pair profiling requires Renderers GPU; software output does not use this pair counter'}
if($DuplicateFixedLayers -and (!$Stereo -or $SerialStereoLayers)){throw 'DuplicateFixedLayers requires independent GPU stereo owners'}
if(('LEVEL5_1' -in $Levels -or 'LEVEL6_6' -in $Levels) -and 'ORIGINAL' -in $Experiences){throw 'Courses 5/6 require EX'}
if($ExModelWobble -ge 0 -and 'ORIGINAL' -in $Experiences){throw 'EX model wobble requires EX only'}
# Reject known loose injectors before launching or changing the environment.
# The app also checks loaded module exports after the renderer/SDK are bound.
$binaryDirectory=[IO.Path]::GetDirectoryName((Resolve-Path -LiteralPath $Executable).Path)
foreach($name in @('renodx-dlss5.addon64','nvngx_dlssnr.dll')) {
 if(Test-Path -LiteralPath (Join-Path $binaryDirectory $name)) {
  throw "Benchmark rejected: legacy DLSS5 sidecar $name; use a clean runtime directory."
 }
}
foreach($name in @('dxgi.dll','d3d11.dll','d3d12.dll','opengl32.dll','vulkan-1.dll','ReShade64.dll','ReShade.dll')) {
 $sidecar=Join-Path $binaryDirectory $name
 if((Test-Path -LiteralPath $sidecar -PathType Leaf) -and
    [Diagnostics.FileVersionInfo]::GetVersionInfo($sidecar).ProductName -match 'ReShade') {
  throw "Benchmark rejected: ReShade injector $name; use a clean runtime directory."
 }
}
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
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {
 $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$rows=New-Object System.Collections.Generic.List[object]
try {
 $settings=@{
  SDL_AUDIODRIVER='dummy';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES="$Frames";
  STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS=[string]$PrerollTicks;STARFOX_TEST_UNPACED='1';
  STARFOX_TEST_PRESENTATION_FPS="$Fps";STARFOX_TEST_TIMING_MODE='ORIGINAL';
  STARFOX_TEST_DISPLAY_MODE='4_3';STARFOX_TEST_RENDER_SCALE='1';STARFOX_TEST_VSYNC='0';
  STARFOX_TEST_REQUIRE_CLEAN_RUNTIME='1';
  # The installed ReShade Vulkan manifest's per-process opt-out. Do not edit
  # its registry registration or affect the user's other applications.
  DISABLE_VK_LAYER_reshade_1='1';
  STARFOX_TRACE_PROFILE='1';STARFOX_TRACE_PROFILE_DISTRIBUTION='1';STARFOX_TEST_PROFILE_WARMUP='60'
 }
 foreach($name in @('MSU1','ENHANCED','SEPARATED_MODELS','ANTI_ALIASING','2D_FILTER',
  'RTX_LIGHTING','BLOOM','BLOOM_2D','EFFECT','WORLD_EFFECT','MODEL_SMOOTHING',
  'HDR_EFFECT','CHROMATIC_ABERRATION','RAY_TRACING','SOFTWARE_SHADOWS','REFLECTIVE_SURFACES',
  'DLSS_SELECTION','DLSS45_SELECTION','FSR1_SELECTION','STEREO_OUTPUT','LANGUAGE',
  'MANIPULATION','MATERIAL','CAMERA_RESPONSE','GLOBAL_ENHANCEMENTS','SCENE_ENHANCEMENTS',
  'DEPTH_ENHANCEMENTS','PARTICLE_ENHANCEMENTS','PHOSPHOR_PERSISTENCE','ADAPTIVE_EXPOSURE',
  'WATER_CAUSTICS','VOLUMETRIC_FOG','MOTION_BLUR_QUALITY','MODEL_FX','WORLD_FX','WORLD_DISTORTION')) {$settings["STARFOX_TEST_$name"]='0'}
 # Baseline runs must not inherit persisted terrain/sky upgrades from the menu.
 for($field=0;$field -lt 6;++$field){$settings["STARFOX_TEST_ENVIRONMENT_$field"]='0'}
 $settings.STARFOX_TEST_STEREO_OUTPUT=[string]$Stereo
 $settings.STARFOX_TEST_RENDER_SCALE=[string]$RenderScale
 $settings.STARFOX_TEST_RAY_TRACING=[string]$RayTracing
 $settings.STARFOX_TEST_REFLECTIVE_SURFACES=[string]$Reflections
 $settings.STARFOX_TEST_BLOOM=[string]$Bloom
 if($DeterministicRayHits){$settings.STARFOX_TEST_DXR_STABLE_HITS='1'}
 if($Taa){$settings.STARFOX_TEST_TAA='1'}
 $settings.STARFOX_TEST_MOTION_BLUR_QUALITY=[string]$MotionBlurQuality
 if($MotionBlurQuality -gt 0){$settings.STARFOX_TEST_TEMPORAL_FPS=[string]$Fps}
 # Per-pass stderr tracing flushes many times on Windows and materially
 # contaminates timing. Keep a one-time success marker and every failure.
 if($Stereo){$settings.STARFOX_TEST_STEREO_RESULT='1'}
 if($ParallelStereoEncoding){$settings.STARFOX_TEST_PARALLEL_STEREO_ENCODING='1'}
 if($SerialStereoEncoding){$settings.STARFOX_TEST_SERIAL_STEREO_ENCODING='1'}
 if($SerialStereoLayers){$settings.STARFOX_TEST_SERIAL_STEREO_LAYERS='1'}
 if($StereoSnapshots){$settings.STARFOX_TEST_STEREO_SNAPSHOT='1'}
 if($DuplicateCpuInputs){$settings.STARFOX_TEST_STEREO_DUPLICATE_CPU_INPUTS='1'}
 if($DuplicateFixedLayers){$settings.STARFOX_TEST_STEREO_DUPLICATE_FIXED_LAYERS='1'}
 if($ModelUploadReuse){$settings.STARFOX_TEST_MODEL_UPLOAD_REUSE='1'}
 if($DuplicateModelUploads){$settings.STARFOX_TEST_DUPLICATE_MODEL_UPLOADS='1'}
 if($BufferedModelPoses){$settings.STARFOX_TEST_BUFFERED_MODEL_POSES='1'}
 if($InlineModelPoses){$settings.STARFOX_TEST_INLINE_MODEL_POSES='1'}
 if($TraceModelUploads){$settings.STARFOX_TRACE_MODEL_UPLOADS='1'}
 if(($TraceModelUploads -or $TraceStereoInputUploads) -and $Stereo){$settings.STARFOX_TRACE_STEREO_INPUT_UPLOADS='1'}
 if($TraceModelCost){$settings.STARFOX_TRACE_MODEL_COST='1'}
 if($TraceModelThreadCost){$settings.STARFOX_TRACE_MODEL_THREAD_COST='1'}
 if($DuplicateStereoFaces){$settings.STARFOX_TEST_DUPLICATE_STEREO_FACES='1'}
 if($DuplicateStereoRays){$settings.STARFOX_TEST_DUPLICATE_STEREO_RAYS='1'}
 if($DuplicateStereoRayUploads){$settings.STARFOX_TEST_DUPLICATE_STEREO_RAY_UPLOADS='1'}
 if($SeparateModelMotion){$settings.STARFOX_TEST_SEPARATE_MODEL_MOTION='1'}
 if($FusedVisibilityBsp){$settings.STARFOX_TEST_FUSED_VISIBILITY_BSP='1';$settings.STARFOX_TEST_VISIBILITY_BSP_RESULT='1'}
 if($SeparateVisibilityBsp){$settings.STARFOX_TEST_SEPARATE_VISIBILITY_BSP='1'}
 if($FusedSmallModel){$settings.STARFOX_TEST_FUSED_SMALL_MODEL='1';$settings.STARFOX_TEST_SMALL_MODEL_RESULT='1'}
 if($SeparateSmallModel){$settings.STARFOX_TEST_SEPARATE_SMALL_MODEL='1'}
 if($SeparateWorldModelMerge){$settings.STARFOX_TEST_SEPARATE_WORLD_MODEL_MERGE='1'}
 if($CopyModelBackgrounds){$settings.STARFOX_TEST_DISABLE_INPLACE_SPANS='1'}
 if($GenericDedither){$settings.STARFOX_TEST_GENERIC_DEDITHER='1'}
 if($FullTileDispatch){$settings.STARFOX_TEST_DISABLE_OCCUPIED_TILES='1'}
 if($OccupiedTiles){$settings.STARFOX_TEST_OCCUPIED_TILES='1'}
 if($RowTileRaster){$settings.STARFOX_TEST_ROW_TILE_RASTER='1'}
 if($MaskTileRaster){$settings.STARFOX_TEST_MASK_TILE_RASTER='1';$settings.STARFOX_TEST_MASK_TILE_RESULT='1'}
 if($SeparateMaskTileRaster){$settings.STARFOX_TEST_DISABLE_MASK_TILE_RASTER='1'}
 if($PixelOnlyRaster){$settings.STARFOX_TEST_PIXEL_ONLY_RASTER='1'}
 if($GenericRaster){$settings.STARFOX_TEST_GENERIC_RASTER='1'}
 if($SeparateRowTileRaster){$settings.STARFOX_TEST_DISABLE_ROW_TILE_RASTER='1'}
 if($BinnedSingleFace){$settings.STARFOX_TEST_SINGLE_FACE_TILED_SPANS='1'}
 if($DirectSingleFace){$settings.STARFOX_TEST_SINGLE_FACE_RASTER='1'}
 if($BinnedSingleFace -or $DirectSingleFace){$settings.STARFOX_TEST_SINGLE_FACE_RESULT='1'}
 if($SeparateEnvironmentReflection){$settings.STARFOX_TEST_SEPARATE_ENVIRONMENT_REFLECTION='1'}
 if($FusedEnvironmentReflection){$settings.STARFOX_TEST_FUSE_ENVIRONMENT_REFLECTION='1'}
 if($SeparateEnvironmentReflection -or $FusedEnvironmentReflection -or $DefaultEnvironmentReflection){$settings.STARFOX_TEST_ENVIRONMENT_REFLECTION_RESULT='1'}
 if($OrderedStereoQueue){$settings.STARFOX_TEST_STEREO_ORDERED_QUEUE='1'}
 if($StereoWait){$settings.STARFOX_TEST_STEREO_WAIT='1'}
 if($EnhancedGround){$settings.STARFOX_TEST_ENVIRONMENT_0='1'}
 if($EnhancedGround){$settings.STARFOX_TEST_ENVIRONMENT_1=[string]$GroundMaterial}
 if($EnhancedSky){$settings.STARFOX_TEST_ENVIRONMENT_3='1'}
 if($UnbatchedTerrain){$settings.STARFOX_TEST_UNBATCHED_TERRAIN='1'}
 if($FullFrameModelRaster){$settings.STARFOX_TEST_FULL_FRAME_MODEL_RASTER='1'}
 if($LowPowerGpu){$settings.STARFOX_TEST_LOW_POWER_GPU='1'}
 if($WindowSize){$settings.STARFOX_TEST_WINDOW_SIZE=$WindowSize}
 if($AsteroidModels){$settings.STARFOX_TEST_ASTEROID_MODELS=$AsteroidModels}
 if($TraceSceneCost){$settings.STARFOX_TRACE_SCENE_COST='1'}
 if($SlowFrameUs){$settings.STARFOX_TRACE_SLOW_FRAME_US=[string]$SlowFrameUs}
 if($WindowSize){$settings.STARFOX_TEST_WINDOW_SIZE=$WindowSize}
 $settings.STARFOX_TEST_GOD_MODE=if($GodMode){'1'}else{'0'}
 if($Visible) {$settings.Remove('STARFOX_TEST_HIDDEN')}
 if($SharedRayPipelines){$settings.STARFOX_TEST_SHARE_DXR_PIPELINES='1'}
 if($DisableRayPrebuildCache){$settings.STARFOX_TEST_DISABLE_DXR_PREBUILD_CACHE='1'}
 if($CachedRayPrebuildSizes){$settings.STARFOX_TEST_DXR_PREBUILD_CACHE='1'}
 if($FixedTemporalClock){$settings.STARFOX_TEST_TEMPORAL_FPS="$Fps"}
 if($ExModelWobble -ge 0){$settings.STARFOX_TEST_EX_MODEL_WOBBLE="$ExModelWobble";$settings.STARFOX_TEST_MODEL_STYLE_RESULT='1'}
 if($TraceSceneCost){$settings.STARFOX_TRACE_SCENE_COST='1'}
 if($TraceSceneGpuTimestamps){$settings.STARFOX_TRACE_SCENE_GPU_TIMESTAMPS='1'}
 if($TraceSceneGpuDraws){$settings.STARFOX_TRACE_SCENE_GPU_DRAWS='1'}
 if($TraceEffectsGpuTimestamps){$settings.STARFOX_TRACE_EFFECTS_GPU_TIMESTAMPS='1'}
 if($TraceVulkanSubmitTimestamps){$settings.STARFOX_TRACE_VULKAN_SUBMIT_TIMESTAMPS='1'}
 if($TraceNativeCost -or $TraceNativeAll){$settings.STARFOX_TRACE_GPU_PASS_COST='1'}
 if($TraceNativeAll){$settings.STARFOX_TRACE_GPU_PASS_COST_ALL='1'}
 if($CopyEffectsInput){$settings.STARFOX_TEST_COPY_EFFECTS_INPUT='1'}
 if($BorrowEffectsInput){$settings.STARFOX_TEST_BORROW_EFFECTS_INPUT='1'}
 if($TraceEffectsCopy){$settings.STARFOX_TEST_EFFECTS_COPY_RESULT='1'}
 if($SeparateEffectsCapture){$settings.STARFOX_TEST_SEPARATE_EFFECTS_CAPTURE='1'}
 if($JoinedStereoSubmissions){$settings.STARFOX_TEST_JOINED_STEREO_SUBMISSIONS='1'}
 if($SplitStereoSubmissions){$settings.STARFOX_TEST_SPLIT_STEREO_SUBMISSIONS='1'}
 if($SplitCompositePipelines){$settings.STARFOX_TEST_SPLIT_COMPOSITE_PIPELINES='1'}
 if($UnifiedCompositePipeline){$settings.STARFOX_TEST_UNIFIED_COMPOSITE_PIPELINE='1'}
 if($SpanBoundsClear){$settings.STARFOX_TEST_SPAN_BOUNDS_CLEAR='1'}
 if($ParallelSpanClear){$settings.STARFOX_TEST_PARALLEL_SPAN_CLEAR='1'}
 if($FullSpanClear){$settings.STARFOX_TEST_FULL_SPAN_CLEAR='1'}
 if($SpanBoundsClear -or $ParallelSpanClear -or $FullSpanClear -or $DefaultSpanClear){$settings.STARFOX_TEST_SPAN_CLEAR_RESULT='1'}
 if($SmallClip){$settings.STARFOX_TEST_SMALL_CLIP='1'}
 if($IdentityClip){$settings.STARFOX_TEST_IDENTITY_CLIP='1';$settings.STARFOX_TEST_CLIP_IDENTITY_RESULT='1'}
 if($CachedProjectionClip){$settings.STARFOX_TEST_CACHED_PROJECTION_CLIP='1';$settings.STARFOX_TEST_CLIP_PROJECTION_CACHE_RESULT='1'}
 if($Radix16Clip){$settings.STARFOX_TEST_RADIX16_CLIP='1';$settings.STARFOX_TEST_CLIP_RADIX16_RESULT='1'}
 if($InteriorClip){$settings.STARFOX_TEST_INTERIOR_CLIP='1';$settings.STARFOX_TEST_CLIP_INTERIOR_RESULT='1'}
 if($ColourSpanTrace){$settings.STARFOX_TEST_COLOUR_SPAN_TRACE='1';$settings.STARFOX_TEST_SPAN_TRACE_RESULT='1'}
 if($CooperativeSpanTrace){$settings.STARFOX_TEST_COOPERATIVE_SPAN_TRACE='1';$settings.STARFOX_TEST_SPAN_TRACE_RESULT='1'}
 if($FullSpanTrace){$settings.STARFOX_TEST_FULL_SPAN_TRACE='1';$settings.STARFOX_TEST_SPAN_TRACE_RESULT='1'}
 if($ColourSpanTrace -or $CooperativeSpanTrace -or $FullSpanTrace){$settings.STARFOX_TEST_MODEL_PATH_RESULT='1'}
 if($FullClip){$settings.STARFOX_TEST_FULL_CLIP='1'}
 if($SmallClip -or $FullClip){$settings.STARFOX_TEST_CLIP_SCRATCH_RESULT='1'}
 if($SplitCompositePipelines -or $UnifiedCompositePipeline){$settings.STARFOX_TEST_COMPOSITE_RESULT='1'}
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
  # Cold integrated-GPU pipeline compilation can exceed one minute. Keep the
  # actual process handle and wait in bounded chunks; an observation timeout
  # must never restart/kill a live benchmark or masquerade as completion.
  $timeoutMs=[Math]::Max(180000,[int](1000*$Frames/$Fps)+60000)
  $observation=[Diagnostics.Stopwatch]::StartNew()
  while(!$process.WaitForExit(30000)) {
   if($observation.ElapsedMilliseconds -ge $timeoutMs) {
    Write-Output "Benchmark still running: PID $($process.Id), log $log (continuing on the same live handle)"
    $observation.Restart()
   }
  }
  if($process.ExitCode -ne 0) {throw "Benchmark failed: $name"}
  if(!(Select-String -LiteralPath $log -SimpleMatch 'test-graphics-runtime: no-legacy-injector' -Quiet)) {
   throw "Benchmark did not validate the loaded graphics runtime: $name"
  }
  if($renderer -eq 'GPU' -and ($TraceSceneGpuTimestamps -or $TraceEffectsGpuTimestamps -or $TraceVulkanSubmitTimestamps)) {
   $timestampLog=Get-Content -LiteralPath $log -Raw
   foreach($phase in @('scene','effects')) {
    if(($phase -eq 'scene' -and !$TraceSceneGpuTimestamps) -or ($phase -eq 'effects' -and !$TraceEffectsGpuTimestamps)){continue}
    $summaries=[regex]::Matches($timestampLog,"(?m)^$phase-gpu-timestamps: samples=(\d+) dropped=(\d+) cancelled=(\d+) errors=(\d+)[^\r\n]*\r?$")
    if(!$summaries.Count -or $timestampLog -match "$phase-gpu-timestamps:[^\r\n]*unavailable=") {
     throw "$phase GPU timestamp queries unavailable: $name"
    }
    foreach($summary in $summaries) {
     if($summary.Value -notmatch " backend=$driver\r?$"){throw "Wrong actual timestamp backend: $name"}
    }
    $expectedSamples=0
    foreach($summary in $summaries) {
     if([int]$summary.Groups[2].Value -or [int]$summary.Groups[3].Value -or [int]$summary.Groups[4].Value) {
      throw "$phase GPU timestamp queries dropped, cancelled, or failed: $name"
     }
     $expectedSamples+=[int]$summary.Groups[1].Value
    }
    $tail=if($phase -eq 'effects'){' input-us=[\d.e+]+ effects-us=[\d.e+]+ output-us=[\d.e+]+'}else{''}
    $actualSamples=[regex]::Matches($timestampLog,"(?m)^$phase-gpu-time: serial=\d+ draws=\d+ ticks=\d+ us=[\d.e+]+$tail\r?$").Count
    if(!$expectedSamples -or $actualSamples -ne $expectedSamples) {throw "Incomplete $phase GPU timestamp samples: $name"}
   }
   if($TraceVulkanSubmitTimestamps) {
    $summary=[regex]::Match($timestampLog,'(?m)^vulkan-submit-timestamps: samples=(\d+) dropped=(\d+)\r?$')
    $actualSamples=[regex]::Matches($timestampLog,'(?m)^vulkan-submit-time: serial=\d+ present=[01] lock-us=[\d.]+ prepare-us=[\d.]+ end-us=[\d.]+ fence-us=[\d.]+ queue-us=[\d.]+ tail-us=[\d.]+\r?$').Count
    if(!$summary.Success -or [int]$summary.Groups[2].Value -or !$actualSamples -or
       $actualSamples -ne [int]$summary.Groups[1].Value) {throw "Incomplete Vulkan host submission timestamps: $name"}
   }
  }
  if($renderer -eq 'GPU' -and $FusedVisibilityBsp -and !$SeparateVisibilityBsp -and !(Select-String -LiteralPath $log -SimpleMatch 'model-visibility-bsp: fused resident pass' -Quiet)) {
   throw "Benchmark did not exercise fused visibility/BSP: $name"
  }
  if($renderer -eq 'GPU' -and $MaskTileRaster -and !$SeparateMaskTileRaster -and
     !(Select-String -LiteralPath $log -SimpleMatch 'raster-mask-tile: compact ordered masks' -Quiet)) {
   throw "Benchmark did not exercise compact mask tiles: $name"
  }
  if($renderer -eq 'GPU' -and $TraceSceneGpuDraws) {
   $drawSummaries=[regex]::Matches((Get-Content -LiteralPath $log -Raw),'(?m)^scene-draw-gpu-timestamps: samples=(\d+) dropped=(\d+) cancelled=(\d+) errors=(\d+)[^\r\n]*\r?$')
   if(!$drawSummaries.Count){throw "No per-draw GPU timestamps: $name"}
   foreach($drawSummary in $drawSummaries) {
    if(![int]$drawSummary.Groups[1].Value -or [int]$drawSummary.Groups[2].Value -or [int]$drawSummary.Groups[3].Value -or [int]$drawSummary.Groups[4].Value -or
       $drawSummary.Value -notmatch "boundary=bottom-bottom backend=$GpuDriver\r?$") {
     throw "Incomplete per-draw GPU timestamps: $name"
    }
   }
  }
  if(Select-String -LiteralPath $log -SimpleMatch 'replaying complete frame' -Quiet){throw "Benchmark replayed the CPU scene: $name"}
  if($renderer -eq 'GPU' -and $Reflections -gt 0 -and $EnhancedGround -and
     !(Select-String -LiteralPath $log -SimpleMatch 'reflection-scene: GPU resident,' -Quiet)) {
   throw "Benchmark did not retain native reflected output: $name"
  }
  if($Stereo -and (!(Select-String -LiteralPath $log -SimpleMatch 'stereo presented:' -Quiet) -or
      (Select-String -LiteralPath $log -Pattern 'stereo failure:|replaying complete frame' -Quiet))) {
   throw "Stereo benchmark fell back or failed: $name"
  }
  if($Stereo -and !(Select-String -LiteralPath $log -Pattern "^stereo-presentation-count: $Frames`$" -Quiet)) {
   throw "Stereo benchmark did not present all $Frames complete eye pairs: $name"
  }
  if($DuplicateFixedLayers -and !(Select-String -LiteralPath $log -SimpleMatch 'stereo-fixed-layer-policy: duplicated' -Quiet)) {
   throw "Duplicated fixed-layer producers did not execute: $name"
  }
  if($Stereo -and ($JoinedStereoSubmissions -or $SplitStereoSubmissions)) {
   $submissionMode=if($SplitStereoSubmissions){'split'}else{'joined'}
   if(!(Select-String -LiteralPath $log -SimpleMatch "stereo-scene-submission: $submissionMode" -Quiet)) {
    throw "Requested $submissionMode stereo submission did not execute: $name"
   }
  }
  if($Stereo -and ($ParallelStereoEncoding -or $SerialStereoEncoding)) {
   $encodingMode=if($SerialStereoEncoding){'split'}else{'parallel'}
   if(!(Select-String -LiteralPath $log -SimpleMatch "stereo-scene-submission: $encodingMode" -Quiet)) {
    throw "Requested $encodingMode stereo encoding did not execute: $name"
   }
  }
  if($renderer -eq 'GPU' -and ($SplitCompositePipelines -or $UnifiedCompositePipeline)) {
   $compositeMode=if($UnifiedCompositePipeline){'unified'}else{'split'}
   if(!(Select-String -LiteralPath $log -SimpleMatch "composite-pipelines: $compositeMode" -Quiet)) {
    throw "Requested $compositeMode composition did not execute: $name"
   }
  }
  $billboardOnly=$false
  if($renderer -eq 'GPU' -and $AllowBillboardOnlyScene) {
   $pathText=Get-Content -LiteralPath $log -Raw
   $pathCounts=[regex]::Matches($pathText,'(?m)^model-path-counts: spans=(\d+) billboards=(\d+) empty=(\d+)\r?$')
   $spanDraws=0L;$billboardDraws=0L
   foreach($count in $pathCounts){$spanDraws+=[long]$count.Groups[1].Value;$billboardDraws+=[long]$count.Groups[2].Value}
   $billboardOnly=$pathCounts.Count -gt 0 -and $spanDraws -eq 0 -and $billboardDraws -ge $Frames*2
   if(!$billboardOnly){throw 'Billboard-only control did not submit its expected non-span model workload'}
   if($pathText -match '(?m)^span-tracing:'){throw 'Billboard-only control unexpectedly exercised a polygon tracer'}
  }
  if($renderer -eq 'GPU' -and ($SmallClip -or $FullClip) -and !$billboardOnly) {
   $clipMode=if($FullClip){'full'}else{'small'}
   if(!(Select-String -LiteralPath $log -SimpleMatch "clip-scratch: $clipMode" -Quiet)) {
    throw "Benchmark did not exercise $clipMode clipping: $name"
   }
  }
  if($renderer -eq 'GPU' -and $IdentityClip -and !$FullClip -and
     !(Select-String -LiteralPath $log -SimpleMatch 'clip-identity: exact interior planes' -Quiet)) {
   throw "Identity clipping variant did not execute: $name"
  }
  if($renderer -eq 'GPU' -and $CachedProjectionClip -and !$FullClip -and
     !(Select-String -LiteralPath $log -SimpleMatch 'clip-projection-cache: exact front raw screens' -Quiet)) {
   throw "Cached projection clipping variant did not execute: $name"
  }
  if($renderer -eq 'GPU' -and $Radix16Clip -and !$FullClip -and
     !(Select-String -LiteralPath $log -SimpleMatch 'clip-radix16: exact binary64 division' -Quiet)) {
   throw "Radix16 clipping variant did not execute: $name"
  }
  if($renderer -eq 'GPU' -and $InteriorClip -and !$FullClip -and
     !(Select-String -LiteralPath $log -SimpleMatch 'clip-interior: exact binary64 output shortcut' -Quiet)) {
   throw "Interior clipping variant did not execute: $name"
  }
  if($renderer -eq 'GPU' -and ($ColourSpanTrace -or $CooperativeSpanTrace -or $FullSpanTrace) -and !$billboardOnly) {
   $traceMode=if($FullSpanTrace){'full UV'}elseif($CooperativeSpanTrace){'cooperative rows'}else{'colour XY-only'}
   if(!(Select-String -LiteralPath $log -Pattern ('^span-tracing: '+[regex]::Escape($traceMode)+'$') -Quiet)) {
    throw "Span tracer did not execute the selected mode: $traceMode"
   }
  }
  if($renderer -eq 'GPU' -and $ExModelWobble -ge 0) {
   $styleText=Get-Content -LiteralPath $log -Raw
   if($styleText -notmatch "model-style: actual-wobble=$ExModelWobble ") {
    throw 'Requested EX model style did not reach actual GPU span rendering'
   }
   $counts=[regex]::Matches($styleText,'(?m)^model-style-counts: normal=(\d+) repeated=(\d+) sparse=(\d+) combined=(\d+)\r?$')
   $submitted=0L
   foreach($count in $counts){$submitted+=[long]$count.Groups[$ExModelWobble+1].Value}
   if($submitted -lt $Frames*2){throw 'Requested EX style lacks at least one model span draw per presented eye/frame'}
  }
  if($renderer -eq 'GPU' -and ($SpanBoundsClear -or $ParallelSpanClear -or $FullSpanClear)) {
   $spanMode=if($FullSpanClear){'full'}elseif($ParallelSpanClear){'parallel'}else{'bounds'}
   if(!(Select-String -LiteralPath $log -SimpleMatch "span-clear: $spanMode" -Quiet)) {
    throw "Requested $spanMode span clearing did not execute: $name"
   }
  }
  if($renderer -eq 'GPU' -and $DefaultSpanClear) {
   if(!(Select-String -LiteralPath $log -SimpleMatch 'span-clear-policy: automatic ' -Quiet) -or
      (Select-String -LiteralPath $log -SimpleMatch 'span-clear-policy: forced ' -Quiet)) {
    throw 'Default span clear did not execute without overrides'
   }
  }
  if($renderer -eq 'GPU' -and ($BinnedSingleFace -or $DirectSingleFace)) {
   $faceMode=if($BinnedSingleFace){'binned'}else{'direct'}
   if(!(Select-String -LiteralPath $log -SimpleMatch "raster-single-face: $faceMode row painter" -Quiet)) {
    throw "Benchmark did not exercise $faceMode single-face rows: $name"
   }
  }
  if($renderer -eq 'GPU' -and ($SeparateEnvironmentReflection -or $FusedEnvironmentReflection -or $DefaultEnvironmentReflection)) {
   $environmentMode=if($SeparateEnvironmentReflection){'separate'}elseif($FusedEnvironmentReflection){'fused'}else{
    $deviceText=Get-Content -LiteralPath $log -Raw
    if($deviceText -notmatch '(?m)^test-gpu-adapter: [^\r\n]+ driver=(direct3d12|vulkan)\r?$') {
     throw "Default environment policy did not record its actual backend: $name"
    }
    if($Matches[1] -eq 'direct3d12'){'fused'}else{'separate'}
   }
   if(!(Select-String -LiteralPath $log -SimpleMatch "environment-reflection: $environmentMode" -Quiet)) {
    throw "Benchmark did not exercise $environmentMode environment/ray resolve: $name"
   }
  }
  if($renderer -eq 'GPU' -and $FusedSmallModel -and
     !(Select-String -LiteralPath $log -SimpleMatch 'model-small-stage: projection visibility painter in one resident pass' -Quiet)) {
   throw "Benchmark did not exercise the bounded small-model stage: $name"
  }
  $summary=@(Get-Content -LiteralPath $log | Where-Object {$_ -match '^(logic/audio|frame-work|present-interval|input-to-present|render)-distribution-us|^render-profile-us|^presentation-pacing:'})
  if($summary.Count -ne 7) {throw "Incomplete profiling metrics: $name"}
  Write-Output $name;Write-Output $summary
 }}}}}}
} finally {
 Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
 foreach($entry in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
