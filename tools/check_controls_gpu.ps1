param([string]$OutputDirectory='tmp/controls-gpu-proof',
    [ValidateSet('4_3','16_9','32_9')][string]$DisplayMode='16_9',
    [ValidateRange(1,4)][int]$RenderScale=2)
$ErrorActionPreference='Stop'
$proofPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proofPath | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy'; STARFOX_TEST_HIDDEN='1'; STARFOX_TEST_FRAMES='6'
        STARFOX_TEST_SKIP_PREROLL='1'; STARFOX_TEST_DISPLAY_MODE=$DisplayMode
        STARFOX_TEST_PRESENTATION_FPS='60'; STARFOX_TEST_TIMING_MODE='ORIGINAL'
        STARFOX_TEST_UNPACED='1'; STARFOX_TEST_VSYNC='0'; STARFOX_TEST_MSU1='0'
        STARFOX_TEST_RENDERER='GPU'; STARFOX_TEST_RENDER_SCALE="$RenderScale"
        STARFOX_TEST_RAY_TRACING='0'; STARFOX_TEST_BLOOM='0'; STARFOX_TEST_BLOOM_2D='0'
        STARFOX_TEST_EFFECT='0'; STARFOX_TEST_WORLD_EFFECT='0'
        STARFOX_TEST_HDR_EFFECT='0'; STARFOX_TEST_CHROMATIC_ABERRATION='0'
        STARFOX_TRACE_GPU='1'; STARFOX_TEST_MODEL_SMOOTHING='0'
    }
    foreach($key in $settings.Keys) {[Environment]::SetEnvironmentVariable($key,$settings[$key],'Process')}
    foreach($experience in @('ORIGINAL','EX')) {
        $env:STARFOX_TEST_EXPERIENCE=$experience
        $arguments=if($experience -eq 'EX') {
            'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt CONTMAP'
        } else {'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT CONTMAP'}
        foreach($tick in @(60,300,600)) {
            $env:STARFOX_TEST_PREROLL_TICKS="$tick"
            $hashes=@{}
            foreach($mode in @('gpu','cpu')) {
                if($mode -eq 'cpu') {$env:STARFOX_DISABLE_GPU_GEOMETRY='1'}
                else {Remove-Item Env:STARFOX_DISABLE_GPU_GEOMETRY -ErrorAction SilentlyContinue}
                $name="$experience-$tick-$mode"
                $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proofPath "$name.bmp"
                $log=Join-Path $proofPath "$name.log"
                $process=Start-Process build/current/starfox_pc.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError $log
                $processHandle=$process.Handle
                if(!$process.WaitForExit(60000)) {throw "Capture still running: PID $($process.Id), $log"}
                if($process.ExitCode -ne 0) {throw "Capture failed: $log"}
                if($mode -eq 'gpu' -and !(Select-String -LiteralPath $log -SimpleMatch 'GPU Controls player layer recorded' -Quiet)) {
                    throw "Controls model was not GPU recorded: $log"
                }
                if($mode -eq 'gpu' -and !(Select-String -LiteralPath $log -SimpleMatch 'native-raster: GPU resident' -Quiet)) {
                    throw "Recorded Controls models did not remain GPU resident: $log"
                }
                $hashes[$mode]=(Get-FileHash -LiteralPath $env:STARFOX_CAPTURE_PRESENTATION_PATH -Algorithm SHA256).Hash
            }
            if($hashes.gpu -ne $hashes.cpu) {throw "Controls CPU/GPU presentation differs: $experience tick $tick"}
            Write-Output "$experience Controls tick ${tick}: CPU/GPU presentation byte-identical"
        }
    }
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys) {[Environment]::SetEnvironmentVariable($key,$saved[$key],'Process')}
}
