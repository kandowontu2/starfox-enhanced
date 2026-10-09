param([ValidateRange(120,3600)][int]$Frames=600,
    [ValidateRange(1,10)][int]$Repeats=3,
    [string]$OutputDirectory='tmp/scene-queue-proof',
    [switch]$CompareMerge,
    [switch]$CompareGeometry,
    [switch]$TraceSceneCost,
    [switch]$CompareTiles,
    [switch]$CompareRayGeometry)
$ErrorActionPreference='Stop'
if($CompareMerge -and $CompareGeometry){throw 'Choose one comparison'}
if($CompareTiles -and ($CompareMerge -or $CompareGeometry)){throw 'Choose one comparison'}
if($CompareRayGeometry -and ($CompareMerge -or $CompareGeometry -or $CompareTiles)){throw 'Choose one comparison'}
$proofRoot=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proofRoot | Out-Null
$savedEnvironment=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_)'} | ForEach-Object {
    $savedEnvironment[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $env:SDL_AUDIODRIVER='dummy'
    $env:STARFOX_TEST_HIDDEN='1'
    $env:STARFOX_TEST_SKIP_PREROLL='1'
    $env:STARFOX_TEST_FRAMES="$Frames"
    $env:STARFOX_TEST_UNPACED='1'
    $env:STARFOX_TEST_PRESENTATION_FPS='240'
    $env:STARFOX_TEST_PROFILE_WARMUP='60'
    $env:STARFOX_TEST_PREROLL_TICKS='200'
    $env:STARFOX_TEST_EXPERIENCE='ORIGINAL'
    $env:STARFOX_TEST_RENDERER='GPU'
    $env:STARFOX_TEST_GPU_GEOMETRY='1'
    $env:STARFOX_TEST_STEREO_OUTPUT='0'
    $env:STARFOX_TEST_VSYNC='0'
    $env:STARFOX_TEST_RENDER_SCALE='2'
    $env:STARFOX_TEST_DISPLAY_MODE='16_9'
    $env:STARFOX_TEST_RAY_TRACING=if($CompareRayGeometry){'1'}else{'0'}
    $env:STARFOX_TEST_RTX_LIGHTING='0'
    $env:STARFOX_TEST_BLOOM='0'
    $env:STARFOX_TEST_EFFECT='0'
    $env:STARFOX_TEST_WORLD_EFFECT='0'
    $env:STARFOX_TEST_HDR_EFFECT='0'
    $env:STARFOX_TEST_CHROMATIC_ABERRATION='0'
    $env:STARFOX_TRACE_PROFILE='1'
    $env:STARFOX_TRACE_PROFILE_DISTRIBUTION='1'
    $env:STARFOX_TRACE_GPU='1'
    $env:STARFOX_TRACE_GPU_RAYS=if($CompareRayGeometry){'1'}else{$null}
    $env:STARFOX_TRACE_SCENE_COST=if($TraceSceneCost){'1'}else{$null}
    for($run=0;$run -lt $Repeats;++$run) {
        $order=if($run%2){@('queued','serial')}else{@('serial','queued')}
        foreach($mode in $order) {
            $comparisonVariable=if($CompareMerge){'STARFOX_TEST_SEPARATE_SCENE_MERGE'}else{'STARFOX_TEST_SERIAL_SCENE'}
            if($CompareTiles){$comparisonVariable='STARFOX_TEST_DISABLE_TILED_SPANS'}
            if($CompareRayGeometry){$comparisonVariable='STARFOX_DISABLE_GPU_RAY_GEOMETRY'}
            if($mode -eq 'serial') {Set-Item -LiteralPath "Env:$comparisonVariable" -Value '1'}
            else {Remove-Item -LiteralPath "Env:$comparisonVariable" -ErrorAction SilentlyContinue}
            if($CompareTiles) {
                # Preserve prior pair labels: serial=tiled, queued=scan.
                $env:STARFOX_TEST_DISABLE_TILED_SPANS=if($mode -eq 'queued'){'1'}else{$null}
            }
            if($CompareGeometry) {
                Remove-Item -LiteralPath "Env:$comparisonVariable" -ErrorAction SilentlyContinue
                $env:STARFOX_TEST_GPU_GEOMETRY=if($mode -eq 'queued'){'1'}else{$null}
                $env:STARFOX_DISABLE_GPU_GEOMETRY=if($mode -eq 'queued'){$null}else{'1'}
            }
            $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proofRoot "$mode-$run.bmp"
            $log=Join-Path $proofRoot "$mode-$run.log"
            $process=Start-Process -FilePath (Resolve-Path 'build/current/starfox_pc.exe').Path -WindowStyle Hidden -PassThru `
                -ArgumentList 'upstream-ultrastarfox/SF.SFC','upstream-ultrastarfox/SYMBOLS.TXT','LEVEL1_1' -RedirectStandardError $log
            $processHandle=$process.Handle
            if(!$process.WaitForExit(60000)){throw "Scene benchmark still running: PID $($process.Id), $mode/$run"}
            if($process.ExitCode -ne 0){throw "Scene benchmark failed: $mode/$run, exit $($process.ExitCode)"}
            $expectsGeometry=!$CompareGeometry -or $mode -eq 'queued'
            if(($expectsGeometry -and !(Select-String -LiteralPath $log -Pattern 'native-geometry: GPU model batch resident' -Quiet)) -or
                !(Select-String -LiteralPath $log -Pattern 'native-pipeline: resident raster' -Quiet) -or
                (Select-String -LiteralPath $log -Pattern 'replaying complete frame' -Quiet)) {
                throw "Resident geometry path not maintained: $mode/$run"
            }
            if($CompareGeometry -and !$expectsGeometry -and (Select-String -LiteralPath $log -Pattern 'native-geometry: GPU model batch resident' -Quiet)) {
                throw 'CPU geometry baseline unexpectedly used GPU geometry'
            }
            if($CompareRayGeometry) {
                $gpuCasters=Select-String -LiteralPath $log -SimpleMatch 'GPU caster geometry' -Quiet
                if($gpuCasters -ne ($mode -eq 'queued')) {throw "Wrong shadow geometry backend: $mode/$run"}
            }
            Write-Output "$mode/$run $(Get-Content -LiteralPath $log | Select-String 'render-(profile|distribution)-us')"
        }
        if((Get-FileHash (Join-Path $proofRoot "serial-$run.bmp")).Hash -ne (Get-FileHash (Join-Path $proofRoot "queued-$run.bmp")).Hash) {
            throw "Final presentation mismatch in pair $run"
        }
    }
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $savedEnvironment.GetEnumerator()){Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
