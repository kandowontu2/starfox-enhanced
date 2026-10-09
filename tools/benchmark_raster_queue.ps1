param([int]$Frames=240,[int]$Repeats=3,[string]$OutputDirectory='tmp/raster-queue-proof')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$queueProofRoot=(Resolve-Path $OutputDirectory).Path
$env:SDL_VIDEODRIVER='windows'
$env:SDL_AUDIODRIVER='dummy'
$env:STARFOX_TEST_HIDDEN='1'
$env:STARFOX_TEST_FRAMES="$Frames"
$env:STARFOX_TEST_UNPACED='1'
$env:STARFOX_TEST_PRESENTATION_FPS='240'
$env:STARFOX_TEST_PROFILE_WARMUP='60'
$env:STARFOX_TEST_PREROLL_TICKS='1000'
$env:STARFOX_TEST_EXPERIENCE='ORIGINAL'
$env:STARFOX_TEST_RENDERER='GPU'
$env:STARFOX_TEST_GPU_NATIVE_PIPELINE='1'
$env:STARFOX_TEST_VSYNC='0'
$env:STARFOX_TEST_RENDER_SCALE='2'
$env:STARFOX_TEST_DISPLAY_MODE='16_9'
$env:STARFOX_TRACE_PROFILE='1'
$env:STARFOX_TRACE_PROFILE_DISTRIBUTION='1'
$env:STARFOX_TRACE_GPU='1'
for($run=0;$run -lt $Repeats;++$run) {
    $order=if($run%2){@('queued','serial')}else{@('serial','queued')}
    foreach($mode in $order) {
        [Environment]::SetEnvironmentVariable('STARFOX_TEST_SERIAL_RASTER',$(if($mode -eq 'serial'){'1'}else{$null}),'Process')
        $env:STARFOX_CAPTURE_PATH=Join-Path $queueProofRoot "$mode-$run.bmp"
        $queueLog=Join-Path $queueProofRoot "$mode-$run.log"
        $queueProcess=Start-Process -FilePath (Resolve-Path 'build/current/starfox_pc.exe').Path -WindowStyle Hidden -PassThru `
            -ArgumentList 'upstream-ultrastarfox/SF.SFC','upstream-ultrastarfox/SYMBOLS.TXT','LEVEL1_1' -RedirectStandardError $queueLog
        $queueProcessHandle=$queueProcess.Handle
        $queueProcess.WaitForExit()
        if($queueProcess.ExitCode -ne 0){throw "Raster queue benchmark failed: $mode/$run, exit $($queueProcess.ExitCode)"}
        Write-Output "$mode/$run $(Get-Content $queueLog | Select-String 'render-(profile|distribution)-us|native-raster:')"
    }
    if((Get-FileHash (Join-Path $queueProofRoot "serial-$run.bmp")).Hash -ne (Get-FileHash (Join-Path $queueProofRoot "queued-$run.bmp")).Hash){throw "Capture mismatch in pair $run"}
}
