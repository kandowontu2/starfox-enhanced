param([string]$OutputDirectory='tmp/temporal-gameplay',
    [ValidateSet(60,120,240)][int]$FPS=240,[switch]$StateRoundtrip)
$ErrorActionPreference='Stop'
$proof=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $proof){throw 'Use a new proof directory'}
New-Item -ItemType Directory -Path $proof | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES='16'
        STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS='1000'
        STARFOX_TEST_DISPLAY_MODE='16_9';STARFOX_TEST_RENDER_SCALE='2'
        STARFOX_TEST_PRESENTATION_FPS="$FPS";STARFOX_TEST_TIMING_MODE='ORIGINAL'
        STARFOX_TEST_UNPACED='1';STARFOX_TEST_VSYNC='0';STARFOX_TEST_MSU1='0'
        STARFOX_TEST_RENDERER='GPU';STARFOX_TEST_RAY_TRACING='0';STARFOX_TEST_STEREO_OUTPUT='0'
        STARFOX_TRACE_GPU='1'
    }
    foreach($key in $settings.Keys){[Environment]::SetEnvironmentVariable($key,$settings[$key],'Process')}
    foreach($experience in @('ORIGINAL','EX')) {
        $env:STARFOX_TEST_EXPERIENCE=$experience
        $arguments=if($experience -eq 'EX'){'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL1_1'}
            else{'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_1'}
        $hashes=@{}
        foreach($mode in @('off','on')) {
            if($StateRoundtrip) {
                $env:STARFOX_TEST_STATE_ACTIONS='4:1,10:2'
                $env:STARFOX_TEST_STATE_DIRECTORY=Join-Path $proof "$experience-$mode-states"
            }
            $env:STARFOX_TEST_TEMPORAL_INPUTS=if($mode -eq 'on'){'1'}else{$null}
            $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proof "$experience-$mode.bmp"
            $log=Join-Path $proof "$experience-$mode.log"
            $process=Start-Process build/current/starfox_pc.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError $log
            $handle=$process.Handle
            if(!$process.WaitForExit(60000)){throw "Still running: PID $($process.Id), $log"}
            if($process.ExitCode -ne 0){throw "Runtime failed: $log"}
            if($mode -eq 'on') {
                if(!(Select-String -LiteralPath $log -Pattern 'temporal-inputs: serial=1 previous=0' -Quiet)){throw "First-frame reset missing: $log"}
                if(!(Select-String -LiteralPath $log -Pattern 'temporal-inputs: serial=\d+ previous=[1-9]\d* depth=1 motion=1' -Quiet)){throw "No resident gameplay motion: $log"}
                if($StateRoundtrip -and (!(Select-String -LiteralPath $log -SimpleMatch 'state loaded slot=0' -Quiet) -or
                    !(Select-String -LiteralPath $log -Pattern 'temporal-inputs: serial=11 previous=0' -Quiet))) {
                    throw "Save-state history reset missing: $log"
                }
            }
            $hashes[$mode]=(Get-FileHash -LiteralPath $env:STARFOX_CAPTURE_PRESENTATION_PATH -Algorithm SHA256).Hash
        }
        if($hashes.off -ne $hashes.on){throw "Temporal generation changed gameplay pixels: $experience"}
        Write-Output "$experience at ${FPS}Hz: live temporal buffers, first-frame reset and unchanged pixels verified"
    }
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys){[Environment]::SetEnvironmentVariable($key,$saved[$key],'Process')}
}
