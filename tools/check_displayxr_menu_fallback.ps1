# Real front-end button navigation on a machine WITHOUT a usable DisplayXR
# panel/runtime. No fake success, vendor installer, saved-setting write or FPS
# claim. Software presentation keeps this unavailable-menu proof GPU-neutral.
param([Parameter(Mandatory)][string]$Binary,
      [Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$binaryPath = [IO.Path]::GetFullPath($Binary)
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Use a fresh output directory' }
New-Item -ItemType Directory -Path $output | Out-Null
$saved = @{}
Get-ChildItem Env: | Where-Object { $_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_VIDEODRIVER$|SDL_GPU_DRIVER$)' } | ForEach-Object {
    $saved[$_.Name] = $_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
$config = Join-Path (Split-Path -Parent $binaryPath) 'pregame.cfg'
$before = if (Test-Path -LiteralPath $config) { (Get-FileHash -LiteralPath $config).Hash } else { '' }
try {
    $settings = @{
        SDL_AUDIODRIVER='dummy';SDL_VIDEODRIVER='dummy';STARFOX_TEST_HIDDEN='1';
        STARFOX_TEST_FRAMES='100';STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS='0';
        STARFOX_TEST_UNPACED='1';STARFOX_TEST_MENU_PREVIEW='1';STARFOX_TEST_RENDERER='SOFTWARE';
        STARFOX_TEST_RENDER_SCALE='1';STARFOX_TEST_VSYNC='0';STARFOX_TEST_DLSS_SELECTION='0';
        STARFOX_TEST_DLSS45_SELECTION='0';STARFOX_TEST_FSR1_SELECTION='0';STARFOX_TEST_RAY_TRACING='0';
        STARFOX_TEST_REFLECTIVE_SURFACES='0';STARFOX_TEST_BLOOM='0';STARFOX_TEST_BLOOM_2D='0';
        STARFOX_TEST_CAMERA_RESPONSE='0';STARFOX_TEST_PRESS_FRAMES='3';
        # Preview enters on main selection 16: up twice opens OPTIONS.
        # Options enters on legacy selection 0 (Cheats): up once opens Stereo.
        # Stereo: move to DisplayXR, request it, release, retry once.
        STARFOX_TEST_PRESSES='0:2048,6:2048,12:128,18:2048,24:128,30:1024,36:1024,42:1024,48:1024,54:1024,60:128,72:128'
    }
    foreach ($experience in 'ORIGINAL','EX') {
        foreach ($prior in 0,1,9) {
            foreach ($entry in $settings.GetEnumerator()) { Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value }
            $env:STARFOX_TEST_EXPERIENCE = $experience
            $env:STARFOX_TEST_STEREO_OUTPUT = [string]$prior
            $arguments = if ($experience -eq 'EX') {
                'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL1_1'
            } else { 'upstream-ultrastarfox/SF.sfc assets/symbols/ultrastarfox.txt LEVEL1_1' }
            $logPath = Join-Path $output "$experience-output-$prior.log"
            $process = Start-Process -FilePath $binaryPath -WorkingDirectory $root -ArgumentList $arguments `
                -WindowStyle Hidden -PassThru -RedirectStandardError $logPath
            $handle = $process.Handle
            while (!$process.WaitForExit(30000)) { Write-Output "Menu fallback check running: PID $($process.Id), $logPath" }
            if ($process.ExitCode) { throw "Menu fallback launch failed: $logPath" }
            $log = Get-Content -LiteralPath $logPath -Raw
            $expected = "DisplayXR menu selection: previous-output=$prior output=$prior requested=0 active=0 status=UNAVAILABLE"
            if ([regex]::Matches($log,[regex]::Escape($expected)).Count -ne 2) {
                throw "Menu navigation failed to preserve/retry the unavailable output: $logPath"
            }
            if ($log -match 'calibrated session connected|first calibrated Leia') {
                throw 'This test requires unavailable DisplayXR; never certify a dummy window as a physical panel'
            }
            Write-Output "$experience OUTPUT ${prior}: real menu selection and retry preserved prior output, cancelled retries, retained UNAVAILABLE label."
        }
    }
    $after = if (Test-Path -LiteralPath $config) { (Get-FileHash -LiteralPath $config).Hash } else { '' }
    if ($before -ne $after) { throw 'Menu diagnostic changed saved preferences' }
} finally {
    Get-ChildItem Env: | Where-Object { $_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_VIDEODRIVER$|SDL_GPU_DRIVER$)' } | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach ($entry in $saved.GetEnumerator()) { [Environment]::SetEnvironmentVariable($entry.Key,$entry.Value,'Process') }
}
