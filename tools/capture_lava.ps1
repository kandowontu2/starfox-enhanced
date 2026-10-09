param([string]$Binary='build/release/starfox_pc.exe',
    [string]$OutputDirectory='tmp/lava-review',
    [ValidateRange(0,1)][int]$RayTracing=1,
    [ValidateRange(0,3)][int]$Reflections=3,
    [ValidateRange(1,600)][int]$Frames=120,
    [ValidateRange(1,600)][int]$CaptureFirst=1,
    [ValidateRange(0,600)][int]$CaptureLast=0,
    [ValidateRange(1,600)][int]$CaptureInterval=4,
    [ValidateRange(1,480)][int]$PresentationFps=60,
    [ValidateRange(0,100000)][int]$PrerollTicks=120,
    [string]$Stage='LEVEL6_6', [int]$Ground=9, [int]$Sky=0,
    [ValidateSet('','Auto','Grass','Dirt','Sand','Snow','Water','Mirror','Gold Metal','Red Sand','Bubbling Lava')][string]$GroundMaterial='',
    [int]$ModelMaterial=0, [string]$Presses='', [int]$GroundEnabled=1,
    [ValidateRange(1,240)][int]$PressFrames=3,
    [int]$Fsr1=0, [ValidateRange(0,4)][int]$Dlss=0, [ValidateRange(0,4)][int]$Dlss45=0, [ValidateRange(0,9)][int]$Stereo=0,
    [ValidateRange(1,512)][int]$StereoSeparation=16,
    [ValidateRange(16,65535)][int]$StereoConvergence=1024,
    [ValidateRange(0,65535)][int]$StereoReticleDepth=0,
    [string]$DlssAdapter='', [string]$DlssBinaries='',
    [ValidateRange(-1,1)][int]$DlssJitter=-1,
    [switch]$CheckDlssStationary,
    [switch]$DlssProxyPresentation,
    [switch]$CancelDlssEvaluationOnce,
    [switch]$Fullscreen,
    [switch]$GpuValidation,
    [switch]$DlssValidation,
    [switch]$CaptureDrawable,
    [switch]$HideFps,
    [switch]$ExpectSrUnavailable,
    [ValidateRange(0,3)][int]$Bloom=0, [string]$Display='16_9',
    [int]$ExMenu=-1, [switch]$D3d11, [switch]$Taa, [switch]$SeparatedModels, [int]$AaType=0, [int]$AaQuality=0,
    [ValidateRange(-1,3)][int]$ExModelWobble=-1,
    [ValidateSet('GPU','SOFTWARE')][string]$Renderer='GPU', [ValidateRange(1,10)][int]$RenderScale=2,
    [ValidateSet('','vulkan','direct3d12','metal')][string]$GpuBackend='',
    [ValidateSet('both','composite','effects')][string]$OrderedStereoOwners='both',
    [switch]$Profile, [switch]$SerialStereoLayers, [switch]$OrderedStereoQueue, [switch]$StereoSnapshots, [switch]$DuplicateCpuInputs, [switch]$DuplicateFixedLayers, [switch]$CheckFixedLayers, [switch]$SeparateModelMotion, [switch]$SeparateWorldModelMerge, [switch]$CopyModelBackgrounds, [switch]$GenericDedither, [switch]$CopyEffectsInput, [switch]$SeparateEffectsCapture, [switch]$StereoWait, [switch]$Paced, [switch]$MenuPreview, [int]$ModelFx=0, [int]$WorldFx=0, [int]$WorldDistortion=0,
    [uint32]$GlobalEnhancements=0,
    [switch]$FullTileDispatch,
    [switch]$TraceSceneGpuDraws,
    [switch]$OccupiedTiles,
    [switch]$MaskTileRaster,
    [switch]$SeparateMaskTileRaster,
    [switch]$BinnedSingleFace,
    [switch]$DirectSingleFace,
    [switch]$SeparateEnvironmentReflection,
    [switch]$FusedEnvironmentReflection,
    [switch]$JoinedStereoSubmissions,
    [switch]$SplitStereoSubmissions,
    [switch]$ParallelStereoEncoding,
    [switch]$SerialStereoEncoding,
    [switch]$FusedSmallModel,
    [switch]$SeparateSmallModel,
    [switch]$ModelUploadReuse,
    [switch]$DuplicateModelUploads,
    [switch]$BufferedModelPoses,
    [switch]$InlineModelPoses,
    [switch]$TraceModelUploads,
 [switch]$SpanBoundsClear,
 [switch]$ParallelSpanClear,
 [switch]$DefaultSpanClear,
 [switch]$FullSpanClear,
 [switch]$ColourSpanTrace,
 [switch]$CooperativeSpanTrace,
 [switch]$FullSpanTrace,
 [switch]$DefaultSpanTrace,
 [switch]$SmallClip,
 [switch]$IdentityClip,
 [switch]$CachedProjectionClip,
 [switch]$Radix16Clip,
 [switch]$InteriorClip,
 [switch]$FullClip,
 [switch]$SplitCompositePipelines,
    [switch]$UnifiedCompositePipeline,
    [switch]$LowPowerGpu,
    [byte]$SceneEnhancements=0,
    [byte]$DepthEnhancements=0,
    [byte]$ParticleEnhancements=0,
    [ValidateRange(0,3)][byte]$Phosphor=0,
    [ValidateRange(0,3)][byte]$Exposure=0,
    [ValidateRange(0,3)][byte]$Caustics=0,
    [ValidateRange(0,3)][byte]$ShadowSoftness=2,
    [double]$CameraBank=0,
    [ValidateRange(0,63)][byte]$CameraResponse=0,
    [int]$CameraOffFrame=-1, [int]$CameraOnFrame=-1,
    [ValidateRange(-1,1)][int]$GodMode=-1,
    [switch]$TraceObjects,
    [switch]$TraceRayInputs,
    [switch]$TraceRayGpuInputs,
    [switch]$TraceRayOutputDump,
    [switch]$TracePipelineKeys,
    [switch]$SharedRayPipelines,
    [switch]$DisableRayPrebuildCache,
    [switch]$CachedRayPrebuildSizes,
    [switch]$Visible,
    [switch]$CheckResidentRayCasters,
    [ValidateRange(0,5)][int]$RayHitDiagnostic=0,
    [switch]$CanonicalRayNormals,
    [switch]$DeterministicRayHits,
    [switch]$MeasureProcessResources,
    [switch]$VolumetricFog,
    [ValidateRange(-1,3)][int]$FogQuality=-1,
    [switch]$DuplicateFogScenes,
    [switch]$DuplicateStereoTopology,
    [switch]$DuplicateStereoProjection,
    [switch]$DuplicateStereoFaces,
    [switch]$DuplicateStereoRays,
    [switch]$DuplicateStereoRayUploads,
    [switch]$TraceStereoSources,
    [switch]$DuplicateStereoSourceUploads,
    [switch]$MotionBlur,
    [switch]$LiveMotionBlur,
    [switch]$FixedTemporalClock,
    [ValidateRange(0,3)][byte]$MotionBlurQuality=0,
    [switch]$CheckMotionPause,
    [switch]$CheckParticleShutter,
    [switch]$CheckGroundShadow,
    [switch]$CheckGroundReflection,
    [Alias('CheckTemporalLighting')][switch]$CheckTemporalSurfaces,
    [switch]$FailStereo,
    [switch]$FailStereoAfterLeft,
    [switch]$FailStereoMsaaResolve,
    [ValidateSet('EX','ORIGINAL')][string]$Experience='EX')
$ErrorActionPreference='Stop'
if($ExpectSrUnavailable -and ($Stereo -ne 9 -or $Renderer -ne 'GPU' -or $FailStereo -or $FailStereoAfterLeft -or $FailStereoMsaaResolve)) {
    throw '-ExpectSrUnavailable requires SR Platform GPU output without injected stereo faults'
}
if($ExModelWobble -ge 0 -and $Experience -ne 'EX'){throw 'EX model wobble requires EX'}
if(($DlssProxyPresentation -or $CancelDlssEvaluationOnce) -and
    ((!$Dlss -and !$Dlss45) -or $Renderer -ne 'GPU' -or $Stereo -or $Fsr1)) {
    throw 'DLSS presentation diagnostics require an actual mono GPU DLSS selection without FSR'
}
if($OrderedStereoQueue -and (!$Stereo -or $Renderer -ne 'GPU' -or $SerialStereoLayers -or $StereoWait -or $SeparatedModels)) {
    throw '-OrderedStereoQueue requires independent GPU stereo owners, without forced waits or separated-model fallback'
}
if($OrderedStereoOwners -ne 'both' -and !$OrderedStereoQueue) {throw '-OrderedStereoOwners requires -OrderedStereoQueue'}
if($FailStereoMsaaResolve -and (!$Stereo -or $AaType -ne 6 -or !$AaQuality -or $Fsr1 -or $Dlss -or $Dlss45 -or
    $LiveMotionBlur -or $MotionBlurQuality -or $CheckResidentRayCasters)) {
    throw '-FailStereoMsaaResolve requires stereo MSAA without neural/spatial upscaling or live motion checks'
}
if($DefaultSpanClear -and ($FullSpanClear -or $ParallelSpanClear -or $SpanBoundsClear)) {
    throw 'Automatic span clearing cannot be combined with a forced clear mode'
}
if($CaptureFirst -gt $Frames -or ($CaptureLast -and ($CaptureLast -lt $CaptureFirst -or $CaptureLast -gt $Frames))) {
    throw 'Capture window must be within 1..Frames, with Last >= First (or Last=0 for all remaining frames)'
}
# A production quality must exercise the saved-setting caller, not the old
# diagnostic enable switch. Existing guards apply to either path.
$productionMotionBlur=$MotionBlurQuality -gt 0
if($productionMotionBlur) {$LiveMotionBlur=$true}
if($GroundMaterial) {
    $Ground=@{'Auto'=0;'Grass'=1;'Dirt'=2;'Sand'=3;'Snow'=4;'Water'=5;'Mirror'=6;'Gold Metal'=7;'Red Sand'=8;'Bubbling Lava'=9}[$GroundMaterial]
}
if($CheckMotionPause -and !$LiveMotionBlur) {throw '-CheckMotionPause requires -LiveMotionBlur and scripted pause/resume presses'}
if($CheckParticleShutter -and !$LiveMotionBlur) {throw '-CheckParticleShutter requires -LiveMotionBlur'}
if($CheckGroundShadow -and (!$LiveMotionBlur -or !$RayTracing)) {
    throw '-CheckGroundShadow requires -LiveMotionBlur with -RayTracing 1'
}
if($CheckGroundReflection -and (!$LiveMotionBlur -or !$RayTracing)) {
    throw '-CheckGroundReflection requires -LiveMotionBlur with -RayTracing 1'
}
if($CheckTemporalSurfaces -and (!$LiveMotionBlur -or !$Taa)) {throw '-CheckTemporalSurfaces requires -LiveMotionBlur and -Taa'}
if($CheckResidentRayCasters -and ($Renderer -ne 'GPU' -or !$RayTracing -or !$Stereo -or $FailStereo -or $FailStereoAfterLeft)) {
    throw '-CheckResidentRayCasters requires an uninterrupted GPU ray-traced stereo capture'
}
$output=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    if($GpuBackend) { $env:SDL_GPU_DRIVER=$GpuBackend }
    elseif($saved.ContainsKey('SDL_GPU_DRIVER')) { $env:SDL_GPU_DRIVER=$saved['SDL_GPU_DRIVER'] }
    $settings=@{
        SDL_AUDIODRIVER='dummy';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES="$Frames";
        STARFOX_TEST_REQUIRE_CLEAN_RUNTIME='1';
        DISABLE_VK_LAYER_reshade_1='1'; # Installed Vulkan manifest's test-only opt-out.
        STARFOX_TEST_PREROLL_TICKS="$PrerollTicks";STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_EXPERIENCE=$Experience;
        STARFOX_TEST_RENDERER=$Renderer;STARFOX_TEST_DISPLAY_MODE=$Display;STARFOX_TEST_TIMING_MODE='ORIGINAL';
        STARFOX_TEST_PRESENTATION_FPS="$PresentationFps";STARFOX_TEST_UNPACED='1';STARFOX_TEST_VSYNC='0';
        STARFOX_TEST_RENDER_SCALE="$RenderScale";STARFOX_TEST_DLSS_SELECTION="$Dlss";STARFOX_TEST_STEREO_OUTPUT="$Stereo";
        STARFOX_TEST_DLSS45_SELECTION="$Dlss45";STARFOX_TEST_CAMERA_RESPONSE="$CameraResponse";
        STARFOX_TEST_STEREO_SEPARATION="$StereoSeparation";STARFOX_TEST_STEREO_CONVERGENCE="$StereoConvergence";
        STARFOX_TEST_ANTI_ALIASING="$AaQuality";STARFOX_TEST_AA_TYPE="$AaType";STARFOX_TEST_RTX_LIGHTING='0';STARFOX_TEST_2D_FILTER='0';
        STARFOX_TEST_BLOOM="$Bloom";STARFOX_TEST_BLOOM_2D='0';STARFOX_TEST_EFFECT='0';STARFOX_TEST_WORLD_EFFECT='0';
        STARFOX_TEST_MATERIAL="$ModelMaterial";STARFOX_TEST_MANIPULATION='0';STARFOX_TEST_MODEL_SMOOTHING='0';
        STARFOX_TEST_SEPARATED_MODELS=$(if($SeparatedModels){'1'}else{'0'});
        STARFOX_TEST_MODEL_FX="$ModelFx";STARFOX_TEST_WORLD_FX="$WorldFx";STARFOX_TEST_WORLD_DISTORTION="$WorldDistortion";
        STARFOX_TEST_GLOBAL_ENHANCEMENTS="$GlobalEnhancements";
        STARFOX_TEST_SCENE_ENHANCEMENTS="$SceneEnhancements";
        STARFOX_TEST_DEPTH_ENHANCEMENTS="$DepthEnhancements";
        STARFOX_TEST_PARTICLE_ENHANCEMENTS="$ParticleEnhancements";
        STARFOX_TEST_PHOSPHOR_PERSISTENCE="$Phosphor";
        STARFOX_TEST_ADAPTIVE_EXPOSURE="$Exposure";
        STARFOX_TEST_WATER_CAUSTICS="$Caustics";
        STARFOX_TEST_VOLUMETRIC_FOG='0';
        STARFOX_TEST_SHADOW_SOFTNESS="$ShadowSoftness";
        STARFOX_TRACE_SCENE_FX='1';
        STARFOX_TEST_HDR_EFFECT='0';STARFOX_TEST_CHROMATIC_ABERRATION='0';
        STARFOX_TEST_RAY_TRACING="$RayTracing";STARFOX_TEST_REFLECTIVE_SURFACES="$Reflections";
        STARFOX_TEST_ENVIRONMENT_0="$GroundEnabled";STARFOX_TEST_ENVIRONMENT_1="$Ground";STARFOX_TEST_ENVIRONMENT_2='0';
        STARFOX_TEST_ENVIRONMENT_3="$Sky";STARFOX_TRACE_GPU_RAYS='1';
        STARFOX_TEST_PRESSES=$Presses;
        STARFOX_TEST_PRESS_FRAMES="$PressFrames";
        STARFOX_CAPTURE_PRESENTATION_SEQUENCE='1';STARFOX_CAPTURE_PRESENTATION_INTERVAL="$CaptureInterval";
        STARFOX_CAPTURE_PRESENTATION_FIRST="$CaptureFirst";
        STARFOX_CAPTURE_PRESENTATION_LAST=$(if($CaptureLast){"$CaptureLast"}else{"$Frames"});
        STARFOX_CAPTURE_PRESENTATION_PATH=(Join-Path $output 'lava.bmp')
    }
    # A brief visible launch tests the actual drawable size instead of a
    # fixed hidden-window fixture. It still exits after the requested frames.
    if($Fullscreen){$settings.Remove('STARFOX_TEST_HIDDEN')}
    if($HideFps){$settings.STARFOX_TEST_SHOW_FPS='0'}
    if($CaptureDrawable){$settings.STARFOX_CAPTURE_PRESENTATION_DRAWABLE='1'}
    if($StereoReticleDepth -gt 0) { $settings.STARFOX_TEST_STEREO_RETICLE_DEPTH="$StereoReticleDepth" }
    if($D3d11){$settings.STARFOX_TEST_D3D11_GPU='1'}
    if($DlssAdapter){$settings.STARFOX_DLSS_ADAPTER=[IO.Path]::GetFullPath($DlssAdapter)}
    if($DlssBinaries){$settings.STARFOX_DLSS_BINARIES=[IO.Path]::GetFullPath($DlssBinaries)}
    if($DlssJitter -ge 0){$settings.STARFOX_TEST_DLSS_JITTER="$DlssJitter"}
    if($DlssProxyPresentation){$settings.STARFOX_TEST_DLSS_PRESENT_PROXY='1'}
    if($CancelDlssEvaluationOnce){$settings.STARFOX_TEST_DLSS_CANCEL_AFTER_EVALUATION='1'}
    if($CheckDlssStationary) {
        if(!$MenuPreview -or (!$Dlss -and !$Dlss45)) {throw '-CheckDlssStationary requires a DLSS menu preview'}
        $settings.STARFOX_TEST_DLSS_AUDIT_STATIONARY='1'
    }
    if($CameraBank -ne 0){$settings.STARFOX_TEST_CAMERA_BANK="$CameraBank";$settings.STARFOX_TRACE_GPU='1'}
    if($CameraResponse){$settings.STARFOX_TEST_CAMERA_RESPONSE="$CameraResponse";$settings.STARFOX_TRACE_GPU='1'}
    if($CameraOffFrame -ge 0){$settings.STARFOX_TEST_CAMERA_OFF_FRAME="$CameraOffFrame"}
    if($CameraOnFrame -ge 0){$settings.STARFOX_TEST_CAMERA_ON_FRAME="$CameraOnFrame"}
    if($GodMode -ge 0){$settings.STARFOX_TEST_GOD_MODE="$GodMode"}
    if($TraceObjects){$settings.STARFOX_TRACE_OBJECTS='1';$settings.STARFOX_TRACE_RENDER_STATE='1'}
    if($TraceRayInputs -or $TraceRayGpuInputs -or $TraceRayOutputDump){$settings.STARFOX_TRACE_DXR_INPUTS='1'}
    if($TraceRayGpuInputs -or $TraceRayOutputDump){$settings.STARFOX_TRACE_DXR_GPU_INPUTS='1'}
    if($TraceRayOutputDump){$settings.STARFOX_TRACE_DXR_OUTPUT_PREFIX=(Join-Path $output 'dxr-output-')}
    if($TracePipelineKeys){$settings.STARFOX_TRACE_DXR_PIPELINE_KEYS='1'}
    if($SharedRayPipelines){$settings.STARFOX_TEST_SHARE_DXR_PIPELINES='1'}
    if($DisableRayPrebuildCache){$settings.STARFOX_TEST_DISABLE_DXR_PREBUILD_CACHE='1'}
    if($CachedRayPrebuildSizes){$settings.STARFOX_TEST_DXR_PREBUILD_CACHE='1'}
    if($Visible){$settings.Remove('STARFOX_TEST_HIDDEN')}
    if($RayHitDiagnostic) {
        if(!$TraceRayOutputDump){throw '-RayHitDiagnostic requires -TraceRayOutputDump'}
        $settings.STARFOX_TEST_DXR_HIT_DIAGNOSTIC="$RayHitDiagnostic"
    }
    if($CanonicalRayNormals){$settings.STARFOX_TEST_DXR_CANONICAL_NORMALS='1'}
    if($DeterministicRayHits){$settings.STARFOX_TEST_DXR_STABLE_HITS='1'}
    if($VolumetricFog){$settings.STARFOX_TEST_VOLUMETRIC_FOG='1';$settings.STARFOX_TRACE_GPU='1'}
    if($FogQuality -ge 0){$settings.STARFOX_TEST_VOLUMETRIC_FOG_QUALITY="$FogQuality";$settings.STARFOX_TRACE_GPU='1'}
    if($DuplicateFogScenes){$settings.STARFOX_TEST_STEREO_DUPLICATE_FOG_SCENES='1'}
    if($DuplicateStereoTopology){$settings.STARFOX_TEST_DUPLICATE_STEREO_TOPOLOGY='1'}
    if($DuplicateStereoProjection){$settings.STARFOX_TEST_DUPLICATE_STEREO_PROJECTION='1'}
    if($DuplicateStereoFaces){$settings.STARFOX_TEST_DUPLICATE_STEREO_FACES='1'}
    if($DuplicateStereoRays){$settings.STARFOX_TEST_DUPLICATE_STEREO_RAYS='1'}
    if($DuplicateStereoRayUploads){$settings.STARFOX_TEST_DUPLICATE_STEREO_RAY_UPLOADS='1'}
    if($TraceStereoSources){$settings.STARFOX_TRACE_STEREO_INPUT_UPLOADS='1'}
    if($DuplicateStereoSourceUploads){$settings.STARFOX_TEST_DUPLICATE_STEREO_SOURCE_UPLOADS='1'}
    if($MotionBlur){$settings.STARFOX_TEST_MOTION_BLUR_CAPTURE=(Join-Path $output 'motion-blur.bmp');$settings.STARFOX_TRACE_GPU='1'}
    if($productionMotionBlur){$settings.STARFOX_TEST_MOTION_BLUR_QUALITY="$MotionBlurQuality";$settings.STARFOX_TRACE_GPU='1'}
    elseif($LiveMotionBlur){$settings.STARFOX_TEST_MOTION_BLUR_LIVE='1';$settings.STARFOX_TRACE_GPU='1'}
    if($FixedTemporalClock){$settings.STARFOX_TEST_TEMPORAL_FPS="$PresentationFps"}
    if($FailStereo){$settings.STARFOX_TEST_FAIL_STEREO_PRESENT='1';$settings.STARFOX_TRACE_GPU='1'}
    if($StereoSnapshots){$settings.STARFOX_TEST_STEREO_SNAPSHOT='1'}
    if($DuplicateCpuInputs){$settings.STARFOX_TEST_STEREO_DUPLICATE_CPU_INPUTS='1'}
    if($DuplicateFixedLayers){$settings.STARFOX_TEST_STEREO_DUPLICATE_FIXED_LAYERS='1'}
    if($CheckFixedLayers){$settings.STARFOX_TEST_STEREO_FIXED_LAYER_RESULT='1'}
    if($Stereo){$settings.STARFOX_TEST_STEREO_RESULT='1'}
    if($SeparateModelMotion){$settings.STARFOX_TEST_SEPARATE_MODEL_MOTION='1'}
    if($SeparateWorldModelMerge){$settings.STARFOX_TEST_SEPARATE_WORLD_MODEL_MERGE='1'}
    if($CopyModelBackgrounds){$settings.STARFOX_TEST_DISABLE_INPLACE_SPANS='1'}
    if($GenericDedither){$settings.STARFOX_TEST_GENERIC_DEDITHER='1'}
    if($CopyEffectsInput){$settings.STARFOX_TEST_COPY_EFFECTS_INPUT='1'}
    if($SeparateEffectsCapture){$settings.STARFOX_TEST_SEPARATE_EFFECTS_CAPTURE='1'}
    if($FullTileDispatch){$settings.STARFOX_TEST_DISABLE_OCCUPIED_TILES='1'}
    if($TraceSceneGpuDraws){
        $settings.STARFOX_TRACE_SCENE_GPU_DRAWS='1';$settings.STARFOX_TRACE_SCENE_GPU_TIMESTAMPS='1'
        if($Stereo){$settings.STARFOX_TEST_STEREO_RESULT='1'}
    }
    if($OccupiedTiles){$settings.STARFOX_TEST_OCCUPIED_TILES='1'}
    if($MaskTileRaster){$settings.STARFOX_TEST_MASK_TILE_RASTER='1';$settings.STARFOX_TEST_MASK_TILE_RESULT='1'}
    if($SeparateMaskTileRaster){$settings.STARFOX_TEST_DISABLE_MASK_TILE_RASTER='1'}
    if($BinnedSingleFace){$settings.STARFOX_TEST_SINGLE_FACE_TILED_SPANS='1'}
    if($DirectSingleFace){$settings.STARFOX_TEST_SINGLE_FACE_RASTER='1'}
    if($BinnedSingleFace -or $DirectSingleFace){$settings.STARFOX_TEST_SINGLE_FACE_RESULT='1'}
    if($SeparateEnvironmentReflection){$settings.STARFOX_TEST_SEPARATE_ENVIRONMENT_REFLECTION='1'}
    if($FusedEnvironmentReflection){$settings.STARFOX_TEST_FUSE_ENVIRONMENT_REFLECTION='1'}
    if($SeparateEnvironmentReflection -or $FusedEnvironmentReflection){$settings.STARFOX_TEST_ENVIRONMENT_REFLECTION_RESULT='1'}
    if($FailStereoAfterLeft){$settings.STARFOX_TEST_FAIL_STEREO_AFTER_LEFT='1';$settings.STARFOX_TRACE_GPU='1'}
    if($FailStereoMsaaResolve){$settings.STARFOX_TEST_FAIL_STEREO_MSAA_RESOLVE_RIGHT='1'}
    if($Paced){$settings.Remove('STARFOX_TEST_UNPACED')}
    if($Taa){$settings.STARFOX_TEST_TAA='1';$settings.STARFOX_TRACE_GPU='1'}
    if($ExMenu -ge 0){$settings.STARFOX_TEST_EX_MENU_BACKGROUND="$ExMenu"}
    if($MenuPreview){$settings.STARFOX_TEST_MENU_PREVIEW='1'}
    if($Profile) {
        $settings.STARFOX_TRACE_GPU_PASS_COST='1'
        $settings.STARFOX_TRACE_GPU_PASS_COST_ALL='1'
        $settings.STARFOX_TRACE_SCENE_COST='1'
        $settings.STARFOX_CAPTURE_PRESENTATION_INTERVAL="$Frames"
    }
    if($SerialStereoLayers){$settings.STARFOX_TEST_SERIAL_STEREO_LAYERS='1'}
    if($OrderedStereoQueue){$settings.STARFOX_TEST_STEREO_ORDERED_QUEUE='1';$settings.STARFOX_TEST_STEREO_ORDERED_OWNER=$OrderedStereoOwners}
    if($StereoWait){$settings.STARFOX_TEST_STEREO_WAIT='1'}
    if($JoinedStereoSubmissions){$settings.STARFOX_TEST_JOINED_STEREO_SUBMISSIONS='1';$settings.STARFOX_TEST_STEREO_RESULT='1'}
    if($SplitStereoSubmissions){$settings.STARFOX_TEST_SPLIT_STEREO_SUBMISSIONS='1';$settings.STARFOX_TEST_STEREO_RESULT='1'}
    if($ParallelStereoEncoding){$settings.STARFOX_TEST_PARALLEL_STEREO_ENCODING='1';$settings.STARFOX_TEST_STEREO_RESULT='1'}
    if($SerialStereoEncoding){$settings.STARFOX_TEST_SERIAL_STEREO_ENCODING='1';$settings.STARFOX_TEST_STEREO_RESULT='1'}
    if($FusedSmallModel){$settings.STARFOX_TEST_FUSED_SMALL_MODEL='1';$settings.STARFOX_TEST_SMALL_MODEL_RESULT='1'}
    if($SeparateSmallModel){$settings.STARFOX_TEST_SEPARATE_SMALL_MODEL='1'}
    if($ModelUploadReuse){$settings.STARFOX_TEST_MODEL_UPLOAD_REUSE='1'}
    if($DuplicateModelUploads){$settings.STARFOX_TEST_DUPLICATE_MODEL_UPLOADS='1'}
    if($BufferedModelPoses){$settings.STARFOX_TEST_BUFFERED_MODEL_POSES='1'}
    if($InlineModelPoses){$settings.STARFOX_TEST_INLINE_MODEL_POSES='1'}
    if($TraceModelUploads){$settings.STARFOX_TRACE_MODEL_UPLOADS='1'}
    if($SplitCompositePipelines){$settings.STARFOX_TEST_SPLIT_COMPOSITE_PIPELINES='1'}
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
    if($DefaultSpanTrace){$settings.STARFOX_TEST_SPAN_TRACE_RESULT='1'}
    if($ColourSpanTrace -or $CooperativeSpanTrace -or $FullSpanTrace -or $DefaultSpanTrace){$settings.STARFOX_TEST_MODEL_PATH_RESULT='1'}
    if($FullClip){$settings.STARFOX_TEST_FULL_CLIP='1'}
    if($SmallClip -or $FullClip){$settings.STARFOX_TEST_CLIP_SCRATCH_RESULT='1'}
    if($UnifiedCompositePipeline){$settings.STARFOX_TEST_UNIFIED_COMPOSITE_PIPELINE='1'}
    if($SplitCompositePipelines -or $UnifiedCompositePipeline){$settings.STARFOX_TEST_COMPOSITE_RESULT='1'}
    if($LowPowerGpu){$settings.STARFOX_TEST_LOW_POWER_GPU='1'}
    if($ExModelWobble -ge 0){$settings.STARFOX_TEST_EX_MODEL_WOBBLE="$ExModelWobble";$settings.STARFOX_TEST_MODEL_STYLE_RESULT='1'}
    if($GpuValidation){$settings.STARFOX_TEST_GPU_VALIDATION='1'}
    if($DlssValidation) {
        if(!$GpuValidation -or $GpuBackend -ne 'direct3d12' -or (!$Dlss -and !$Dlss45)) {
            throw 'DLSS validation requires real DLSS, D3D12 and GPU validation'
        }
        $settings.STARFOX_TEST_DLSS_VALIDATION='1'
    }
    $settings.STARFOX_TEST_FSR1_SELECTION="$Fsr1"
    if($GpuBackend -or $Fsr1 -or $Dlss -or $Dlss45 -or $AaQuality -or $Stereo){$settings.STARFOX_TRACE_GPU='1'}
    foreach($entry in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
    $log=Join-Path $output 'runtime.log'
    $runtimeArguments=if($Experience -eq 'EX') {"tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt $Stage"} else {"upstream-ultrastarfox/SF.sfc assets/symbols/ultrastarfox.txt $Stage"}
    if($Fullscreen){$runtimeArguments="--fullscreen $runtimeArguments"}
    if($MeasureProcessResources) {
        # Whole Windows process working set/CPU, not GPU memory or per-frame
        # timing. Keep this sampling out of the quiet timing harness.
        $resourceHash=(Get-FileHash -LiteralPath $Binary -Algorithm SHA256).Hash
        $resourcePeak=0L;$resourceCpu=0.0;$resourceSamples=0;$resourceError=''
        $resourceClock=[Diagnostics.Stopwatch]::StartNew();$resourceLastReport=0L
    }
    $process=Start-Process $Binary -ArgumentList $runtimeArguments -WindowStyle Hidden -PassThru -RedirectStandardError $log
    $handle=$process.Handle
    if($MeasureProcessResources) {
        while(!$process.WaitForExit(1000)) {
            try {
                $process.Refresh()
                $resourcePeak=[Math]::Max($resourcePeak,$process.PeakWorkingSet64)
                $resourceCpu=$process.TotalProcessorTime.TotalSeconds
                ++$resourceSamples
            } catch {
                # A normal exit can race a sample. In every case retain and
                # wait on this same process; never restart or abandon it.
                if(!$process.HasExited){$resourceError=$_.Exception.Message}
            }
            if($resourceClock.ElapsedMilliseconds-$resourceLastReport -ge 30000) {
                Write-Output "Capture still running: PID $($process.Id), $log"
                $resourceLastReport=$resourceClock.ElapsedMilliseconds
            }
        }
        $resourceClock.Stop()
        @{terminal=$true;exit_code=$process.ExitCode;binary=$Binary;sha256=$resourceHash;
            wall_seconds=$resourceClock.Elapsed.TotalSeconds;sampled_cpu_seconds=$resourceCpu;
            sampled_peak_working_set_bytes=$resourcePeak;samples=$resourceSamples;sample_error=$resourceError;
            requested_backend=$GpuBackend;low_power=[bool]$LowPowerGpu;stable_hits=[bool]$DeterministicRayHits;
            stage=$Stage;frames=$Frames;scale=$RenderScale;stereo=$Stereo} |
            ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'process-resources.json')
        if($resourceError -or !$resourceSamples){throw 'Process-resource measurement incomplete; inspect process-resources.json'}
    } else {
        while(!$process.WaitForExit(30000)) {
            Write-Output "Capture still running: PID $($process.Id), $log"
        }
    }
    if($process.ExitCode -ne 0) {throw "Capture failed: $log"}
    if(!(Select-String -LiteralPath $log -SimpleMatch 'test-graphics-runtime: no-legacy-injector' -Quiet)) {
        throw 'Capture did not validate the loaded graphics runtime; inspect runtime.log'
    }
    if(!(Test-Path -LiteralPath (Join-Path $output 'lava.bmp'))) {throw 'Missing final lava capture'}
    if($ExpectSrUnavailable) {
        $declined=@(Select-String -LiteralPath $log -SimpleMatch 'stereo failure: SR Platform unavailable').Count
        if($declined -ne $Frames -or (Select-String -LiteralPath $log -SimpleMatch 'stereo presented:' -Quiet)) {
            throw 'SR Platform must decline every native frame without publishing fake native output; inspect runtime.log'
        }
    }
    if($Stereo -and !$ExpectSrUnavailable -and !$FailStereo -and !$FailStereoAfterLeft -and !$FailStereoMsaaResolve -and !(Select-String -LiteralPath $log -SimpleMatch 'stereo presented:' -Quiet)) {
        throw 'Requested stereo output did not present; inspect runtime.log'
    }
    if($DeterministicRayHits -and !(Select-String -LiteralPath $log -Pattern '^DXR reflection pipeline creating: .* stable-depth$' -Quiet)) {
        throw 'Requested precise ray-hit pipeline did not execute; inspect runtime.log'
    }
    if($TracePipelineKeys -and !(Select-String -LiteralPath $log -Pattern '^dxr-pipeline-key: device=.+ root=.+ root_hash=\d+ root_bytes=\d+$' -Quiet)) {
        throw 'Requested DXR pipeline identity diagnostic did not execute; inspect runtime.log'
    }
    if($SharedRayPipelines -and $TracePipelineKeys -and !(Select-String -LiteralPath $log -Pattern '^dxr-pipeline-prepare: .* shared=1 reused=1 ' -Quiet)) {
        throw 'Requested shared DXR pipeline was not reused; inspect runtime.log'
    }
    if($Stereo -and ($JoinedStereoSubmissions -or $SplitStereoSubmissions)) {
        $submissionMode=if($SplitStereoSubmissions){'split'}else{'joined'}
        if(!(Select-String -LiteralPath $log -Pattern "^stereo-scene-submission: $submissionMode`$" -Quiet)) {
            throw "Requested $submissionMode stereo scene submission did not execute; inspect runtime.log"
        }
    }
    if($CheckResidentRayCasters) {
        if(Select-String -LiteralPath $log -Pattern '^ray-scene CPU caster fallback$|stereo failure:|replaying complete frame' -Quiet) {
            throw 'Stereo ray casters fell back to CPU or incomplete presentation; inspect runtime.log'
        }
        $completePairs=@(Select-String -LiteralPath $log -Pattern '^stereo presented:').Count
        if($completePairs -ne $Frames -or !(Select-String -LiteralPath $log -Pattern '^shadow-backend: .*stereo.*GPU caster' -Quiet)) {
            throw 'Not all requested frames used complete stereo GPU caster/presentation pairs; inspect runtime.log'
        }
    }
    if($FailStereoAfterLeft -and !(Select-String -LiteralPath $log -SimpleMatch 'injected failure after left eye' -Quiet)) {
        throw 'Stereo did not reach the left-eye failure injection; inspect runtime.log'
    }
    if($FailStereoMsaaResolve -and
       (@(Select-String -LiteralPath $log -SimpleMatch 'stereo failure: MSAA palette resolve:').Count -ne $Frames -or
        (Select-String -LiteralPath $log -SimpleMatch 'stereo presented:' -Quiet))) {
        throw 'Every requested pair must reach the MSAA resolve fault and decline publication'
    }
    if($MotionBlur -and (!(Test-Path -LiteralPath (Join-Path $output 'motion-blur.bmp')) -or
        !(Select-String -LiteralPath $log -Pattern 'motion-blur-reference: moving=[1-9][0-9]* .*history=1 saved=1 changed=[1-9][0-9]* protected-changed=0' -Quiet))) {
        throw 'Motion-blur reference did not capture valid moving history; inspect runtime.log'
    }
    if($MotionBlur -and !(Select-String -LiteralPath $log -Pattern 'motion-blur-gpu: max-error=[012] protected-changed=0 saved=1' -Quiet)) {
        throw 'GPU motion-blur capture differs from reference or changed protected pixels; inspect runtime.log'
    }
    if($LiveMotionBlur -and !$FailStereo -and !$FailStereoAfterLeft -and !(Select-String -LiteralPath $log -SimpleMatch 'motion-blur-live: applied=1 history=1' -Quiet)) {
        throw 'Live motion-blur pipeline did not execute with valid history; inspect runtime.log'
    }
    if($LiveMotionBlur -and $Fsr1 -and !(Select-String -LiteralPath $log -SimpleMatch 'fsr1: scene=' -Quiet)) {
        throw 'FSR1 did not execute with live motion blur; inspect runtime.log'
    }
    if($LiveMotionBlur -and $Taa -and !(Select-String -LiteralPath $log -SimpleMatch 'taa: resolved' -Quiet)) {
        throw 'TAA did not execute with live motion blur; inspect runtime.log'
    }
    if($LiveMotionBlur -and $DepthEnhancements -and
        !(Select-String -LiteralPath $log -SimpleMatch "depth-fx: modes=$DepthEnhancements " -Quiet)) {
        throw 'Requested depth effects did not execute with live motion blur; inspect runtime.log'
    }
    if($CheckMotionPause) {
        $motionEyeCount=if($Stereo) {2} else {1}
        & (Join-Path $PSScriptRoot 'check_motion_blur_pause.ps1') -Log $log -EyeCount $motionEyeCount
    }
    if($CheckGroundShadow) {
        if($Stereo) {foreach($eye in 0,1) {
            if(!(Select-String -LiteralPath $log -Pattern "motion-ground-shadow: ready=1 hardware=1 resident_geometry=[01] eye=$eye" -Quiet)) {
                throw "Missing hardware ground-only shadow for stereo eye $eye"
            }
        }}
        if(!$Stereo -and !(Select-String -LiteralPath $log -Pattern 'motion-ground-shadow: ready=1 hardware=1 resident_geometry=1' -Quiet)) {
            throw 'No hardware ground-only mask used resident caster geometry'
        }
        if(Select-String -LiteralPath $log -Pattern 'motion-ground-shadow: ready=0|motion-blur-live: unavailable|motion-blur-live: applied=0' -Quiet) {
            throw 'Ground-shadow exposure failed or declined a frame; inspect runtime.log'
        }
    }
    if($CheckGroundReflection) {
        if($Stereo) {foreach($eye in 0,1) {
            if(!(Select-String -LiteralPath $log -SimpleMatch "motion-ground-reflection: ready=1 eye=$eye" -Quiet)) {
                throw "Missing independent ground reflection for stereo eye $eye"
            }
        }}
        if(!(Select-String -LiteralPath $log -SimpleMatch 'motion-ground-reflection: ready=1' -Quiet)) {
            throw 'No independent ground reflection reached the exposure path'
        }
        if(Select-String -LiteralPath $log -Pattern 'motion-ground-reflection: ready=0|motion-blur-live: unavailable|motion-blur-live: applied=0' -Quiet) {
            throw 'Ground-reflection exposure failed or declined a frame; inspect runtime.log'
        }
    }
    if($CheckParticleShutter) {
        if(!(Select-String -LiteralPath $log -Pattern 'particle-shutter-live: count=[1-9][0-9]* history=1' -Quiet)) {
            throw 'No live particles reached valid-history joint exposure'
        }
        $unexpectedStereoFailures=@(Select-String -LiteralPath $log -SimpleMatch 'stereo failure:' | Where-Object {
            !($FailStereoAfterLeft -and $_.Line.StartsWith('stereo failure: injected failure after left eye:')) -and
            !($FailStereo -and $_.Line.StartsWith('stereo failure: injected presentation failure:'))
        })
        if($unexpectedStereoFailures.Count -or (Select-String -LiteralPath $log -Pattern 'motion-blur-live: unavailable|motion-blur-live: applied=0' -Quiet)) {
            throw 'Particle exposure declined or failed a frame; inspect runtime.log'
        }
    }
    if($LiveMotionBlur -and $Stereo -and !$FailStereo -and !$FailStereoAfterLeft) {
        foreach($eyeSlot in 1,2) {
            if(!(Select-String -LiteralPath $log -SimpleMatch "motion-blur-live: applied=1 history=1 slot=$eyeSlot" -Quiet)) {
                throw "Live motion blur did not execute for stereo eye slot $eyeSlot; inspect runtime.log"
            }
        }
    }
    if($LiveMotionBlur -and $FailStereoAfterLeft) {
        $failedPairs=@(Select-String -LiteralPath $log -SimpleMatch 'injected failure after left eye').Count
        if($RayTracing -and $Reflections) {
            $monoReflections=@(Select-String -LiteralPath $log -SimpleMatch 'stereo-fallback-reflections: ready=1').Count
            if($failedPairs -ne $monoReflections -or !$monoReflections) {
                throw 'Failed stereo pair did not regenerate every mono reflection frame'
            }
        }
        $identityFallbacks=@(Select-String -LiteralPath $log -SimpleMatch 'motion-blur-live: applied=1 history=0 slot=0').Count
        if($failedPairs -ne $identityFallbacks -or
            (Select-String -LiteralPath $log -SimpleMatch 'motion-blur-live: applied=1 history=1 slot=0' -Quiet)) {
            throw 'Failed stereo pair leaked motion history into the mono fallback'
        }
    }
    if($LiveMotionBlur -and $CameraBank -ne 0 -and
        !(Select-String -LiteralPath $log -SimpleMatch 'camera-response: resident world/HUD=1' -Quiet)) {
        throw 'Camera response did not execute with live motion blur; inspect runtime.log'
    }
    if($LiveMotionBlur -and $VolumetricFog -and
        !(Select-String -LiteralPath $log -SimpleMatch 'motion-blur-fog-underlay: ready=1' -Quiet)) {
        throw 'Background fog did not execute with live motion blur; inspect runtime.log'
    }
    if($Dlss -or $Dlss45) {
        if($MenuPreview) {
            if(!(Select-String -LiteralPath $log -SimpleMatch 'dlss-gameplay: evaluated' -Quiet) -or
                !(Select-String -LiteralPath $log -SimpleMatch 'frozen=1' -Quiet) -or
                ($Frames -gt 32 -and !(Select-String -LiteralPath $log -SimpleMatch 'dlss-preview: reused reconstructed frame=' -Quiet))) {
                throw 'Frozen DLSS preview did not reconstruct and retain its neural result; inspect runtime.log'
            }
        } elseif(!(Select-String -LiteralPath $log -SimpleMatch 'dlss-gameplay: evaluated' -Quiet)) {
            throw 'DLSS did not evaluate; inspect runtime.log'
        }
    }
    if($DlssValidation) {
        $validationText=Get-Content -LiteralPath $log -Raw
        foreach($stage in 'sdk-teardown','renderer-teardown') {
            if($validationText -notmatch ('(?m)^dlss-validation: stage='+$stage+' critical=0 discarded=0\r?$')) {
                throw "Missing lossless zero-critical DLSS validation at $stage; inspect runtime.log"
            }
        }
        if($validationText -match 'dlss-validation: FAILED|dlss-validation: ID |critical=[1-9]|discarded=[1-9]') {
            throw 'DLSS emitted critical/overflow validation errors; inspect runtime.log'
        }
    }
    # ray-water is emitted by the GPU liquid dispatcher, not the software
    # environment/reflection path. Do not require GPU evidence in a CPU capture.
    if($Renderer -eq 'GPU' -and $RayTracing -and $GroundEnabled -and $Ground -ge 5 -and !(Select-String -LiteralPath $log -SimpleMatch 'ray-water=1' -Quiet)) {
        throw 'Ray-traced liquid did not execute; inspect runtime.log'
    }
    if($CheckTemporalSurfaces -and !(Select-String -LiteralPath $log -SimpleMatch 'taa-surfaces: aligned lighting/depth' -Quiet)) {
        throw 'TAA lighting/depth surface alignment did not execute; inspect runtime.log'
    }
    if($GpuBackend -and !(Select-String -LiteralPath $log -Pattern ("(?:driver=|SDL GPU compute: )"+[regex]::Escape($GpuBackend)+"(?:\s|$)") -Quiet)) {
        throw "Requested GPU backend $GpuBackend was not confirmed; inspect runtime.log"
    }
    if(($TraceRayInputs -or $TraceRayGpuInputs -or $TraceRayOutputDump) -and !(Select-String -LiteralPath $log -SimpleMatch 'dxr-inputs: call=' -Quiet)) {
        throw 'Requested DXR input diagnostic did not execute; inspect runtime.log'
    }
    if(($TraceRayGpuInputs -or $TraceRayOutputDump) -and !(Select-String -LiteralPath $log -SimpleMatch 'dxr-resident-inputs: call=' -Quiet)) {
        throw 'Requested resident DXR input diagnostic did not execute; inspect runtime.log'
    }
    if($TraceRayOutputDump -and (!(Test-Path -LiteralPath (Join-Path $output 'dxr-output-1.bin')) -or
        !(Select-String -LiteralPath $log -SimpleMatch 'dxr-output-dump: index=1 ' -Quiet))) {
        throw 'Requested DXR output diagnostic was not saved; inspect runtime.log'
    }
    Write-Output "Lava capture: $output"
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
