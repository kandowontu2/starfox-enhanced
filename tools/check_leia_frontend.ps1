param([string]$Binary='build/release/starfox_pc.exe',
      [string]$OutputDirectory='tmp/leia-frontend-recovery')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$exe=[IO.Path]::GetFullPath($Binary)
$output=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$config=Join-Path (Split-Path $exe -Parent) 'pregame.cfg'
$before=if(Test-Path -LiteralPath $config){(Get-FileHash -LiteralPath $config).Hash}else{''}
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_VIDEODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy';SDL_VIDEODRIVER='dummy';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES='12';
        STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_UNPACED='1';STARFOX_TEST_EXPERIENCE='ORIGINAL';
        STARFOX_TEST_RENDERER='SOFTWARE';STARFOX_TEST_RENDER_SCALE='1';STARFOX_TEST_VSYNC='0';
        STARFOX_TEST_DLSS_SELECTION='0';STARFOX_TEST_DLSS45_SELECTION='0';STARFOX_TEST_FSR1_SELECTION='0';
        STARFOX_TEST_STEREO_OUTPUT='0';STARFOX_TEST_CAMERA_RESPONSE='0';STARFOX_TEST_BLOOM='0';
        STARFOX_TEST_BLOOM_2D='0';STARFOX_TEST_RAY_TRACING='0';STARFOX_TEST_REFLECTIVE_SURFACES='0';
        STARFOX_TEST_RTX_LIGHTING='0';STARFOX_TEST_MODEL_SMOOTHING='0';STARFOX_TEST_ANTI_ALIASING='0';
        STARFOX_TEST_2D_FILTER='0';STARFOX_TEST_EFFECT='0';STARFOX_TEST_WORLD_EFFECT='0';STARFOX_TEST_MATERIAL='0';
        STARFOX_TEST_MANIPULATION='0';STARFOX_TEST_HDR_EFFECT='0';STARFOX_TEST_CHROMATIC_ABERRATION='0';
        STARFOX_TEST_GLOBAL_ENHANCEMENTS='0';STARFOX_TEST_SCENE_ENHANCEMENTS='0';STARFOX_TEST_DEPTH_ENHANCEMENTS='0';
        STARFOX_TEST_PARTICLE_ENHANCEMENTS='0';STARFOX_TEST_PHOSPHOR_PERSISTENCE='0';STARFOX_TEST_ADAPTIVE_EXPOSURE='0';
        STARFOX_TEST_WATER_CAUSTICS='0';STARFOX_TEST_MOTION_BLUR_QUALITY='0';STARFOX_TEST_VOLUMETRIC_FOG='0';
        STARFOX_TEST_ENVIRONMENT_0='0';STARFOX_TEST_ENVIRONMENT_3='0'
    }
    foreach($entry in $settings.GetEnumerator()){Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
    foreach($case in 'off','explicit-incompatible','no-panel-window','vulkan-no-panel-window') {
        Remove-Item -LiteralPath Env:SDL_GPU_DRIVER -ErrorAction SilentlyContinue
        $runtimeArgs='upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_1'
        if($case -ne 'off'){$runtimeArgs="--leia-sr $runtimeArgs"}
        if($case -eq 'explicit-incompatible'){$env:SDL_GPU_DRIVER='opengl'}
        if($case -eq 'vulkan-no-panel-window'){$env:SDL_GPU_DRIVER='vulkan'}
        $log=Join-Path $output "$case.log"
        $run=Start-Process $exe -ArgumentList $runtimeArgs -WorkingDirectory $root -WindowStyle Hidden -PassThru -RedirectStandardError $log
        $handle=$run.Handle
        if(!$run.WaitForExit(60000)){throw "Leia fallback still running: PID $($run.Id), $log"}
        if($run.ExitCode -ne 0){throw "Leia fallback failed: $log"}
        $text=Get-Content -LiteralPath $log -Raw
        if($case -eq 'off' -and $text -match 'Leia'){throw 'Native OFF launch unexpectedly probed/started Leia'}
        if($case -eq 'explicit-incompatible' -and $text -notmatch 'explicit SDL_GPU_DRIVER must be direct3d12 or vulkan') {
            throw 'Explicit incompatible backend was not respected'
        }
        if($case -ne 'off' -and $text -notmatch 'Leia SR (unavailable|reconnect unavailable)') {
            throw 'Opt-in without a usable panel/window did not report its fallback'
        }
        if($text -match 'calibrated session connected|first calibrated Leia') {throw 'Dummy window accepted as a physical Leia presentation target'}
        Write-Output "Leia frontend ${case}: completed normal 2D frames and exited cleanly."
    }
    $after=if(Test-Path -LiteralPath $config){(Get-FileHash -LiteralPath $config).Hash}else{''}
    if($before -ne $after){throw 'Diagnostic fallback launch overwrote saved preferences'}
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_VIDEODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach($entry in $saved.GetEnumerator()){[Environment]::SetEnvironmentVariable($entry.Key,$entry.Value,'Process')}
}
