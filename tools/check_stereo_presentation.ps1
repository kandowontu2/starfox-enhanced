param([string]$Binary='build/release/starfox_pc.exe',
    [string]$OutputDirectory='tmp/stereo-direct-presentation-check',
    [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12',
    [string[]]$CaseNames=@(),
    [switch]$SingleFaceComparison,
    [switch]$MaskTileComparison,
    [switch]$IdentityClipComparison,
    [switch]$CachedProjectionClipComparison,
    [switch]$Radix16ClipComparison,
    [switch]$ParallelSpanClearComparison,
    [switch]$DefaultSpanClearComparison,
    [switch]$SceneDrawTimingComparison,
    [switch]$EnvironmentReflectionComparison,
    [switch]$OrderedQueueComparison,
    [switch]$DefaultOrderedQueueComparison,
    [switch]$FixedLayerComparison,
    [switch]$FogSceneComparison,
    [switch]$TopologyComparison,
    [switch]$ProjectionComparison,
    [ValidateRange(24,120)][int]$Frames=32)
$ErrorActionPreference='Stop'
if(@($MaskTileComparison,$IdentityClipComparison,$CachedProjectionClipComparison,$Radix16ClipComparison,$ParallelSpanClearComparison,$DefaultSpanClearComparison,$SceneDrawTimingComparison,$SingleFaceComparison,$EnvironmentReflectionComparison,$OrderedQueueComparison,$DefaultOrderedQueueComparison,$FixedLayerComparison,$FogSceneComparison,$TopologyComparison,$ProjectionComparison |
      Where-Object {[bool]$_}).Count -gt 1) {
    throw 'Rendering comparisons isolate one change; do not combine comparisons'
}
$isolatedProductionComparison=$MaskTileComparison -or $IdentityClipComparison -or $CachedProjectionClipComparison -or $Radix16ClipComparison -or $ParallelSpanClearComparison -or $DefaultSpanClearComparison -or $SceneDrawTimingComparison -or $OrderedQueueComparison -or $DefaultOrderedQueueComparison -or $FixedLayerComparison -or $FogSceneComparison -or $TopologyComparison -or $ProjectionComparison
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output) {throw 'Choose a new output directory; existing results will not be overwritten'}
New-Item -ItemType Directory -Path $output | Out-Null
$preferences=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$preferencesHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
$binaryHash=(Get-FileHash -LiteralPath $Binary).Hash
$cases=@()
foreach($mode in 1..8) {
    $cases+=@{Name="format-$mode";Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=$mode;
        RenderScale=1;GroundEnabled=0;RayTracing=0;Reflections=0}}
}
$cases+=@{Name='water-bloom';Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=2;
    RenderScale=2;Ground=5;Sky=1;Bloom=2;RayTracing=1;Reflections=3}}
$cases+=@{Name='upscale-only';Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=2;
    RenderScale=4;GroundEnabled=0;RayTracing=0;Reflections=0}}
$cases+=@{Name='upscale-AA';Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=7;
    RenderScale=2;GroundEnabled=0;AaType=5;AaQuality=2;RayTracing=0;Reflections=0}}
foreach($quality in 1..3) {
    $samples=1 -shl $quality
    $cases+=@{Name="msaa-$samples";MsaaSamples=$samples;Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=2;
        RenderScale=1;GroundEnabled=0;AaType=6;AaQuality=$quality;RayTracing=0;Reflections=0}}
}
$cases+=@{Name='lava-exposure';InlineMotion=$true;Settings=@{Experience='EX';Stage='LEVEL6_6';Stereo=1;
    RenderScale=2;Ground=9;Sky=1;Bloom=2;SceneEnhancements=32;LiveMotionBlur=$true;
    CheckParticleShutter=$true;CheckGroundReflection=$true;CheckGroundShadow=$true;RayTracing=1;Reflections=3}}
$cases+=@{Name='banked-ground';Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=2;
    RenderScale=3;Ground=1;Sky=1;CameraBank=0.2;RayTracing=0;Reflections=0}}
$cases+=@{Name='fog';Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=2;
    RenderScale=1;GroundEnabled=0;VolumetricFog=$true;RayTracing=0;Reflections=0}}
$cases+=@{Name='production-motion';InlineMotion=$true;Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=2;
    RenderScale=2;GroundEnabled=0;MotionBlurQuality=2;RayTracing=0;Reflections=0}}
$cases+=@{Name='split-model-fallback';SnapshotOnly=$true;Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=2;
    RenderScale=1;GroundEnabled=0;SeparatedModels=$true;RayTracing=0;Reflections=0}}
if($FogSceneComparison) {
    $cases=@()
    foreach($mode in 1..8) {
        $cases+=@{Name="fog-format-$mode";Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=$mode;
            RenderScale=1;GroundEnabled=0;FogQuality=2;RayTracing=0;Reflections=0}}
    }
    foreach($quality in 1..3) {
        $cases+=@{Name="fog-quality-$quality";Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=2;
            RenderScale=2;Ground=1;Sky=1;FogQuality=$quality;RayTracing=0;Reflections=0}}
    }
    $cases+=@{Name='fog-banked';Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=2;
        RenderScale=2;Ground=1;Sky=1;CameraBank=0.2;FogQuality=2;RayTracing=0;Reflections=0}}
    $cases+=@{Name='fog-water-motion';InlineMotion=$true;Settings=@{Experience='ORIGINAL';Stage='LEVEL1_1';Stereo=2;
        RenderScale=2;Ground=5;Sky=1;FogQuality=2;Bloom=2;MotionBlurQuality=2;SceneEnhancements=32;RayTracing=1;Reflections=3}}
    $cases+=@{Name='fog-lava';Settings=@{Experience='EX';Stage='LEVEL6_6';Stereo=1;
        RenderScale=2;Ground=9;Sky=1;FogQuality=2;Bloom=2;RayTracing=1;Reflections=3}}
}
if($CaseNames.Count) {
    foreach($name in $CaseNames) {if($name -notin $cases.Name){throw "Unknown case: $name"}}
    $cases=@($cases | Where-Object {$_.Name -in $CaseNames})
}
if($OrderedQueueComparison -or $DefaultOrderedQueueComparison) {
    if('split-model-fallback' -in $CaseNames) {throw 'Ordered queue comparison requires independent per-eye owners'}
    $cases=@($cases | Where-Object {!$_.SnapshotOnly})
}
$results=@()
foreach($case in $cases) {
    $common=@{Binary=$Binary;GpuBackend=$GpuBackend;Frames=$Frames;CaptureFirst=8;CaptureInterval=8;
        FixedTemporalClock=$true;
        PrerollTicks=$(if($case.Settings.Experience -eq 'ORIGINAL'){1000}else{120})}
    foreach($entry in $case.Settings.GetEnumerator()) {$common[$entry.Key]=$entry.Value}
    $reference=Join-Path $output ($case.Name+'-snapshot')
    $candidate=Join-Path $output ($case.Name+'-direct')
    if($TopologyComparison -or $ProjectionComparison) {
        $sourceControl=if($ProjectionComparison){@{DuplicateStereoProjection=$true}}else{@{DuplicateStereoTopology=$true}}
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $reference @sourceControl
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $candidate
        foreach($route in @(@{Directory=$reference;Policy='duplicated'},@{Directory=$candidate;Policy='pair-shared'})) {
            $runtime=Join-Path $route.Directory 'runtime.log'
            $sourcePolicy=if($ProjectionComparison){'projection'}else{'topology'}
            $marker=@(Select-String -LiteralPath $runtime -Pattern ('^stereo-'+$sourcePolicy+'-policy: '+$route.Policy+' sources=(\d+) models=(\d+) prepared=(\d+)$'))
            if($marker.Count -ne 1 -or @(Select-String -LiteralPath $runtime -Pattern '^stereo presented:').Count -ne $Frames -or
               (Select-String -LiteralPath $runtime -Pattern 'VK_ERROR_|device lost|GPU effects fallback:|stereo failure:|replaying complete frame' -Quiet)) {
                throw "Source topology route was not exercised cleanly: $($case.Name)/$($route.Policy)"
            }
            $match=$marker[0].Matches[0]
            $packed=[int]$match.Groups[1].Value;$models=[int]$match.Groups[2].Value;$prepared=[int]$match.Groups[3].Value
            if(!$models -or ($route.Policy -eq 'pair-shared' -and (!$packed -or $models -ne $prepared -or 2*$packed -gt $models)) -or
               ($route.Policy -eq 'duplicated' -and ($prepared -ne 0 -or $packed -ne $models))) {
                throw "Topology preparation accounting is inconsistent: $($case.Name)/$($route.Policy)"
            }
        }
        if($ProjectionComparison) {
            $topologyReference=@(Select-String -LiteralPath (Join-Path $reference 'runtime.log') -Pattern '^stereo-topology-policy: pair-shared sources=\d+ models=\d+ prepared=\d+$');
            $topologyCandidate=@(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -Pattern '^stereo-topology-policy: pair-shared sources=\d+ models=\d+ prepared=\d+$');
            if($topologyReference.Count -ne 1 -or $topologyCandidate.Count -ne 1 -or
                $topologyReference[0].Line -ne $topologyCandidate[0].Line) {
                throw "Projection comparison changed the BSP preparation policy: $($case.Name)"
            }
        }
    } elseif($FogSceneComparison) {
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $reference -DuplicateFogScenes
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $candidate
        foreach($route in @(@{Directory=$reference;Policy='duplicated'},@{Directory=$candidate;Policy='pair-shared'})) {
            $runtime=Join-Path $route.Directory 'runtime.log'
            if(@(Select-String -LiteralPath $runtime -Pattern ('^stereo-fog-scene-policy: '+$route.Policy+'$')).Count -ne 1 -or
               @(Select-String -LiteralPath $runtime -Pattern '^stereo presented:').Count -ne $Frames -or
               (Select-String -LiteralPath $runtime -Pattern 'VK_ERROR_|device lost|GPU effects fallback:|stereo failure:|replaying complete frame' -Quiet)) {
                throw "Shared fog comparison did not finish the requested route: $($case.Name)/$($route.Policy)"
            }
        }
        if($case.InlineMotion -and !(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch 'motion-blur-fog-underlay: ready=1' -Quiet)) {
            throw 'Shared fog underlay was not exercised'
        }
        if($case.InlineMotion) {
            foreach($eye in 0..1) {
                if(!(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch "motion-blur-fog-source: resident eye=$eye" -Quiet) -or
                   !(Select-String -LiteralPath (Join-Path $reference 'runtime.log') -SimpleMatch "motion-blur-fog-source: independent eye=$eye" -Quiet)) {
                    throw "Fog motion underlay did not exercise both source policies for eye $eye"
                }
            }
        }
    } elseif($FixedLayerComparison) {
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $reference -CheckFixedLayers -DuplicateFixedLayers
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $candidate -CheckFixedLayers
        $repeated=@(Select-String -LiteralPath (Join-Path $reference 'runtime.log') -Pattern '^stereo-fixed-layer: rendered slot=[0-3] eye=[01]$').Count
        $rendered=@(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -Pattern '^stereo-fixed-layer: rendered slot=[0-3] eye=0$').Count
        $shared=@(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -Pattern '^stereo-fixed-layer: shared slot=[0-3] eye=1$').Count
        if(!$shared -or $rendered -ne $shared -or $repeated -ne 2*$shared -or
            (Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -Pattern '^stereo-fixed-layer: rendered slot=[0-3] eye=1$' -Quiet) -or
            (Select-String -LiteralPath (Join-Path $reference 'runtime.log') -Pattern '^stereo-fixed-layer: shared ' -Quiet)) {
            throw "Fixed-layer comparison did not halve eligible producer submissions: $($case.Name)"
        }
        foreach($directory in @($reference,$candidate)) {
            $runtime=Join-Path $directory 'runtime.log'
            if(@(Select-String -LiteralPath $runtime -Pattern '^stereo presented:').Count -ne $Frames -or
                (Select-String -LiteralPath $runtime -Pattern 'VK_ERROR_|device lost|GPU effects fallback:|stereo failure:|replaying complete frame' -Quiet)) {
                throw "Fixed-layer comparison did not finish clean GPU pairs in $directory"
            }
        }
    } elseif($OrderedQueueComparison -or $DefaultOrderedQueueComparison) {
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $reference -StereoWait
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $candidate -OrderedStereoQueue:$OrderedQueueComparison
        $expectedPolicy=if($DefaultOrderedQueueComparison){'automatic bounded'}else{'forced bounded'}
        if(!(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch "stereo-layer-queue-policy: $expectedPolicy" -Quiet) -or
            !(Select-String -LiteralPath (Join-Path $reference 'runtime.log') -SimpleMatch 'stereo-layer-queue-policy: forced synchronous' -Quiet)) {
            throw "Queued comparison did not exercise the requested production policy: $($case.Name)"
        }
        if($DefaultOrderedQueueComparison -and
            (Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch 'stereo-layer-queue-policy: forced bounded' -Quiet)) {
            throw "Automatic queued policy required a diagnostic override: $($case.Name)"
        }
        foreach($directory in @($reference,$candidate)) {
            $runtime=Join-Path $directory 'runtime.log'
            if(@(Select-String -LiteralPath $runtime -Pattern '^stereo presented:').Count -ne $Frames -or
               (Select-String -LiteralPath $runtime -Pattern 'VK_ERROR_|device lost|GPU effects fallback:|stereo failure:|replaying complete frame' -Quiet)) {
                throw "Queued retirement comparison did not complete clean GPU pairs in $directory"
            }
        }
        foreach($marker in 'composite-ordered-queue: submitted','effects-ordered-queue: submitted') {
            if(@(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch $marker).Count -ne 2) {
                throw "Both per-eye owners did not exercise $marker in $($case.Name)"
            }
            if(Select-String -LiteralPath (Join-Path $reference 'runtime.log') -SimpleMatch $marker -Quiet) {
                throw "Reference unexpectedly used $marker in $($case.Name)"
            }
        }
    } elseif($MaskTileComparison) {
        # Both runs use the production composition/metadata/queue path. Only
        # the compact face-list representation differs, including live motion.
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $reference -SeparateMaskTileRaster
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $candidate -MaskTileRaster
        if(!(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch 'raster-mask-tile: compact ordered masks' -Quiet)) {
            throw "Compact mask raster was not exercised in $($case.Name)"
        }
        if(Select-String -LiteralPath (Join-Path $reference 'runtime.log') -SimpleMatch 'raster-mask-tile:' -Quiet) {
            throw "Binned reference unexpectedly used compact masks in $($case.Name)"
        }
    } elseif($IdentityClipComparison -or $CachedProjectionClipComparison -or $Radix16ClipComparison) {
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $reference -FullClip
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $candidate -IdentityClip:$IdentityClipComparison -CachedProjectionClip:$CachedProjectionClipComparison -Radix16Clip:$Radix16ClipComparison
        $clipMarker=if($Radix16ClipComparison){'clip-radix16: exact binary64 division'}elseif($CachedProjectionClipComparison){'clip-projection-cache: exact front raw screens'}else{'clip-identity: exact interior planes'}
        if(!(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch $clipMarker -Quiet)) {
            throw "Requested clipping variant was not exercised in $($case.Name)"
        }
        if(Select-String -LiteralPath (Join-Path $reference 'runtime.log') -Pattern '^clip-(identity|projection-cache|radix16):' -Quiet) {
            throw "Reference unexpectedly used variant clipping in $($case.Name)"
        }
    } elseif($ParallelSpanClearComparison -or $DefaultSpanClearComparison) {
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $reference -FullSpanClear
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $candidate -ParallelSpanClear:$ParallelSpanClearComparison -DefaultSpanClear:$DefaultSpanClearComparison
        foreach($pair in @(@($reference,'full'),@($candidate,'parallel'))) {
            if(!(Select-String -LiteralPath (Join-Path $pair[0] 'runtime.log') -SimpleMatch "span-clear: $($pair[1])" -Quiet)) {
                throw "Span $($pair[1]) route was not exercised in $($case.Name)"
            }
        }
        if($DefaultSpanClearComparison -and
           (!(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch 'span-clear-policy: automatic ' -Quiet) -or
            (Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch 'span-clear-policy: forced ' -Quiet))) {
            throw "Automatic span clear used an override in $($case.Name)"
        }
    } elseif($SceneDrawTimingComparison) {
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $reference
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $candidate -TraceSceneGpuDraws
        if(!(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -Pattern '^scene-draw-profile: batches=[1-9][0-9]* owners=[1-9][0-9]* oversized=0$' -Quiet)) {
            throw "Draw timing did not execute in $($case.Name)"
        }
        if(Select-String -LiteralPath (Join-Path $reference 'runtime.log') -SimpleMatch 'scene-draw-profile:' -Quiet) {
            throw "Reference unexpectedly used draw timing in $($case.Name)"
        }
        & (Join-Path $PSScriptRoot 'summarize_scene_draws.ps1') -Log (Join-Path $candidate 'runtime.log') `
            -Output (Join-Path $candidate 'draw-summary.json') -WarmupFrames 0 | Out-Null
    } else {
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $reference -StereoSnapshots -DuplicateCpuInputs -SeparateModelMotion -SeparateWorldModelMerge -CopyModelBackgrounds -GenericDedither -CopyEffectsInput -SeparateEffectsCapture -FullTileDispatch -BinnedSingleFace:$SingleFaceComparison -SeparateEnvironmentReflection:$EnvironmentReflectionComparison
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $candidate -OccupiedTiles:(!$SingleFaceComparison) -DirectSingleFace:$SingleFaceComparison -FusedEnvironmentReflection:$EnvironmentReflectionComparison
    }
    if($EnvironmentReflectionComparison) {
        foreach($pair in @(@($reference,'separate'),@($candidate,'fused'))) {
            if(!(Select-String -LiteralPath (Join-Path $pair[0] 'runtime.log') -SimpleMatch "environment-reflection: $($pair[1])" -Quiet)) {
                throw "Environment/ray $($pair[1]) resolve was not exercised in $($case.Name)"
            }
        }
    }
    if($SingleFaceComparison) {
        foreach($pair in @(@($reference,'binned'),@($candidate,'direct'))) {
            if(!(Select-String -LiteralPath (Join-Path $pair[0] 'runtime.log') -SimpleMatch "raster-single-face: $($pair[1]) row painter" -Quiet)) {
                throw "Single-face $($pair[1]) path was not exercised in $($case.Name)"
            }
        }
    }
    $targetKind=if($case.SnapshotOnly){'snapshot'}else{'direct'}
    if($case.MsaaSamples) {
        foreach($directory in @($reference,$candidate)) {
            if(!(Select-String -LiteralPath (Join-Path $directory 'runtime.log') -SimpleMatch "stereo-msaa: samples=$($case.MsaaSamples) late-palette=1" -Quiet)) {
                throw "Real per-eye MSAA/late palette was not exercised in $($case.Name)"
            }
        }
    }
    if(!(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch 'stereo-cpu-inputs: shared right eye' -Quiet)) {
        throw "Shared CPU input path was not exercised in $($case.Name)"
    }
    if($case.InlineMotion -and !(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -Pattern '^scene-motion-merge: models=[1-9][0-9]*$' -Quiet)) {
        throw "Inline model motion was not exercised in $($case.Name)"
    }
    if(!$isolatedProductionComparison -and (Select-String -LiteralPath (Join-Path $reference 'runtime.log') -SimpleMatch 'scene-motion-merge:' -Quiet)) {
        throw "Separate-motion reference unexpectedly used inline motion in $($case.Name)"
    }
    if($case.InlineMotion -and !(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -Pattern '^scene-world-raster-merge: models=[1-9][0-9]*$' -Quiet)) {
        throw "World-model raster merge was not exercised in $($case.Name)"
    }
    if(!$isolatedProductionComparison -and (Select-String -LiteralPath (Join-Path $reference 'runtime.log') -SimpleMatch 'scene-world-raster-merge:' -Quiet)) {
        throw "Separate-world-merge reference unexpectedly used combined raster in $($case.Name)"
    }
    if(!$isolatedProductionComparison -and (Select-String -LiteralPath (Join-Path $reference 'runtime.log') -SimpleMatch 'scene-inplace-raster:' -Quiet)) {
        throw "Copying reference unexpectedly borrowed a sparse target in $($case.Name)"
    }
    if(!$isolatedProductionComparison -and (Select-String -LiteralPath (Join-Path $reference 'runtime.log') -SimpleMatch 'effects-dedither: specialized' -Quiet)) {
        throw "Generic reference unexpectedly used specialized dedither in $($case.Name)"
    }
    if($case.Settings.RenderScale -gt 1 -and
        !(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch 'effects-dedither: specialized' -Quiet)) {
        throw "Specialized dedither was not exercised in $($case.Name)"
    }
    if(($case.Name -like 'format-*' -or $case.Name -eq 'banked-ground') -and
        !(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -Pattern '^scene-inplace-raster: models=[1-9][0-9]*$' -Quiet)) {
        throw "Sparse model painter path was not exercised in $($case.Name)"
    }
    if(!$isolatedProductionComparison -and !$SingleFaceComparison -and ($case.Name -like 'format-*' -or $case.Name -eq 'banked-ground') -and
        !(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch 'raster-occupied-tiles: indirect native pass' -Quiet)) {
        throw "Occupied-tile raster was not exercised in $($case.Name)"
    }
    if(!$isolatedProductionComparison -and (Select-String -LiteralPath (Join-Path $reference 'runtime.log') -SimpleMatch 'raster-occupied-tiles: indirect native pass' -Quiet)) {
        throw "Dense reference unexpectedly used occupied dispatch in $($case.Name)"
    }
    foreach($eye in 0,1) {
        if(!(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -SimpleMatch "stereo-eye: $targetKind eye=$eye" -Quiet)) {
            throw "$targetKind target was not exercised for eye $eye in $($case.Name)"
        }
    }
    $images=@(Get-ChildItem -LiteralPath $reference -Filter '*.bmp')
    foreach($image in $images) {
        $actual=Join-Path $candidate $image.Name
        if(!(Test-Path -LiteralPath $actual) -or (Get-FileHash -LiteralPath $image.FullName).Hash -ne (Get-FileHash -LiteralPath $actual).Hash) {
            throw "Stereo target parity failed: $($case.Name)/$($image.Name)"
        }
    }
    $results+=@{case=$case.Name;images=$images.Count;exact=$true;settings=$case.Settings;sha256=$binaryHash;single_face_comparison=[bool]$SingleFaceComparison;mask_tile_comparison=[bool]$MaskTileComparison;identity_clip_comparison=[bool]$IdentityClipComparison;cached_projection_clip_comparison=[bool]$CachedProjectionClipComparison;radix16_clip_comparison=[bool]$Radix16ClipComparison;scene_draw_timing_comparison=[bool]$SceneDrawTimingComparison;environment_reflection_comparison=[bool]$EnvironmentReflectionComparison}
    $results[-1].parallel_span_clear_comparison=[bool]$ParallelSpanClearComparison
    $results[-1].default_span_clear_comparison=[bool]$DefaultSpanClearComparison
    $results[-1].ordered_queue_comparison=[bool]$OrderedQueueComparison
    $results[-1].default_ordered_queue_comparison=[bool]$DefaultOrderedQueueComparison
    $results[-1].fixed_layer_comparison=[bool]$FixedLayerComparison
    $results[-1].fog_scene_comparison=[bool]$FogSceneComparison
    $results[-1].topology_comparison=[bool]$TopologyComparison
    $results[-1].projection_comparison=[bool]$ProjectionComparison
    if($FixedLayerComparison) {
        $results[-1].eligible_reference_submissions=$repeated
        $results[-1].eligible_candidate_submissions=$rendered
        $results[-1].shared_right_eye_sources=$shared
    }
    Write-Output "PASS $($case.Name): $($images.Count) byte-identical packed images"
}
# A partial eye must never leak into the ordinary mono fallback. With a reset
# interval the diagnostic exposure is identity, including normal exhaust ink.
$fallback=@{Binary=$Binary;GpuBackend=$GpuBackend;Frames=$Frames;CaptureFirst=8;CaptureInterval=8;
    FixedTemporalClock=$true;
    Experience='EX';Stage='LEVEL6_6';RenderScale=2;Ground=9;Sky=1;Bloom=2;SceneEnhancements=32;
    RayTracing=1;Reflections=3}
if($ParallelSpanClearComparison){$fallback.ParallelSpanClear=$true}
if($DefaultSpanClearComparison){$fallback.DefaultSpanClear=$true}
if($FogSceneComparison){$fallback.VolumetricFog=$true}
$mono=Join-Path $output 'fallback-mono'
$rejected=Join-Path $output 'fallback-after-left'
& (Join-Path $PSScriptRoot 'capture_lava.ps1') @fallback -OutputDirectory $mono -MaskTileRaster:$MaskTileComparison -IdentityClip:$IdentityClipComparison -CachedProjectionClip:$CachedProjectionClipComparison -Radix16Clip:$Radix16ClipComparison
& (Join-Path $PSScriptRoot 'capture_lava.ps1') @fallback -OutputDirectory $rejected -Stereo 2 -FailStereoAfterLeft -LiveMotionBlur -CheckParticleShutter -CheckGroundReflection -CheckGroundShadow -MaskTileRaster:$MaskTileComparison -IdentityClip:$IdentityClipComparison -CachedProjectionClip:$CachedProjectionClipComparison -Radix16Clip:$Radix16ClipComparison -TraceSceneGpuDraws:$SceneDrawTimingComparison -OrderedStereoQueue:$OrderedQueueComparison
$images=@(Get-ChildItem -LiteralPath $mono -Filter '*.bmp')
foreach($image in $images) {
    $actual=Join-Path $rejected $image.Name
    if(!(Test-Path -LiteralPath $actual) -or (Get-FileHash -LiteralPath $image.FullName).Hash -ne (Get-FileHash -LiteralPath $actual).Hash) {
        throw "Rejected-eye mono parity failed: $($image.Name)"
    }
}
$results+=@{case='fallback-after-left';images=$images.Count;exact=$true;settings=$fallback;sha256=$binaryHash}
Write-Output "PASS fallback-after-left: $($images.Count) byte-identical mono images"
$afterHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
if($afterHash -ne $preferencesHash) {throw 'Saved preferences changed during stereo tests'}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash) {throw 'Executable changed during stereo tests'}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output 'All stereo presentation comparisons passed; saved preferences unchanged.'
