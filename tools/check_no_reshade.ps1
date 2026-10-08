param([string]$Binary='build/current/starfox_pc.exe',
    [Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$binaryPath=(Resolve-Path -LiteralPath $Binary).Path
if(Test-Path -LiteralPath $OutputDirectory) {throw 'Use a new diagnostic output directory'}
$output=(New-Item -ItemType Directory -Path $OutputDirectory).FullName
$binaryDirectory=Split-Path $binaryPath
$protected=@{}
foreach($name in @('pregame.cfg','ReShade.ini','ReShade.log')) {
    $path=Join-Path $binaryDirectory $name
    $protected[$path]=if(Test-Path -LiteralPath $path) {(Get-FileHash -LiteralPath $path).Hash}else{''}
}
$environmentPattern='^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$|VK_LOADER_LAYERS_(DISABLE|ALLOW|ENABLE)$|VK_INSTANCE_LAYERS$)'
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match $environmentPattern} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$process=$null;$results=@()
try {
    foreach($experience in 'ORIGINAL','EX') {foreach($driver in 'direct3d12','vulkan') {foreach($initial in 'SOFTWARE','GPU') {
        $case="$experience-$driver-$initial"
        $log=Join-Path $output "$case.log"
        $settings=@{
            # Deliberately enable the installed implicit layer in the incoming
            # environment. The EXE, not this test's launcher, must disable it.
            DISABLE_VK_LAYER_reshade_1='0';VK_LOADER_LAYERS_DISABLE='VK_LAYER_TEST_unrelated';
            SDL_AUDIODRIVER='dummy';SDL_GPU_DRIVER=$driver;
            STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES='40';STARFOX_TEST_UNPACED='1';
            STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS='1000';
            STARFOX_TEST_RENDERER=$initial;STARFOX_TEST_EXPERIENCE=$experience;
            STARFOX_TEST_REQUIRE_CLEAN_RUNTIME='1';STARFOX_TEST_RENDERER_CYCLE='1';
            STARFOX_TEST_DISPLAY_MODE='16_9';STARFOX_TEST_RENDER_SCALE='1';
            STARFOX_TEST_PRESENTATION_FPS='60';STARFOX_TEST_TIMING_MODE='ORIGINAL';
            STARFOX_TEST_VSYNC='0';STARFOX_TEST_MSU1='0';STARFOX_TEST_STEREO_OUTPUT='0';
            STARFOX_TRACE_GPU='1';STARFOX_TEST_SHOW_FPS='0'
        }
        foreach($name in @('DLSS_SELECTION','DLSS45_SELECTION','FSR1_SELECTION','ANTI_ALIASING','2D_FILTER',
            'RAY_TRACING','REFLECTIVE_SURFACES','SOFTWARE_SHADOWS','RTX_LIGHTING','HDR_EFFECT','BLOOM','BLOOM_2D',
            'EFFECT','WORLD_EFFECT','MATERIAL','MANIPULATION','MODEL_FX','WORLD_FX','WORLD_DISTORTION',
            'CAMERA_RESPONSE','GLOBAL_ENHANCEMENTS','SCENE_ENHANCEMENTS','DEPTH_ENHANCEMENTS','PARTICLE_ENHANCEMENTS',
            'PHOSPHOR_PERSISTENCE','ADAPTIVE_EXPOSURE','VOLUMETRIC_FOG','MOTION_BLUR_QUALITY','WATER_CAUSTICS')) {
            $settings["STARFOX_TEST_$name"]='0'
        }
        for($field=0;$field -lt 6;++$field) {$settings["STARFOX_TEST_ENVIRONMENT_$field"]='0'}
        foreach($entry in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
        $process=Start-Process -FilePath $binaryPath -ArgumentList 'LEVEL1_1' -WorkingDirectory $binaryDirectory `
            -WindowStyle Hidden -PassThru -RedirectStandardError $log -RedirectStandardOutput (Join-Path $output "$case.stdout.log")
        $handle=$process.Handle
        while(!$process.WaitForExit(250)) {
            $process.Refresh();$modules=@()
            try {$modules=@($process.Modules | Where-Object {$_.ModuleName -match 'ReShade|renodx|dlssnr'})}
            catch {if(!$process.HasExited) {throw}}
            if($modules.Count) {throw "Legacy injector loaded in $case : $($modules.ModuleName -join ',')"}
        }
        if($process.ExitCode -ne 0) {throw "Renderer switch failed in $case ($($process.ExitCode)); see $log"}
        $text=Get-Content -LiteralPath $log -Raw
        if([regex]::Matches($text,'test-graphics-runtime: no-legacy-injector').Count -lt 5 -or
            [regex]::Matches($text,'renderer-cycle frame=').Count -ne 4 -or
            $text -notmatch 'native-pipeline:' -or $text -match 'renderer-recovery:|neural-filter:|dlss5-control:') {
            throw "Missing clean runtime/live GPU switch coverage in $case; see $log"
        }
        $results+=@{case=$case;clean_runtime_checks=[regex]::Matches($text,'test-graphics-runtime: no-legacy-injector').Count;renderer_switches=4}
        $process.Dispose();$process=$null
        "PASS: $case, four live renderer switches, no ReShade/RenoDX module"
    }}}
    foreach($entry in $protected.GetEnumerator()) {
        $after=if(Test-Path -LiteralPath $entry.Key) {(Get-FileHash -LiteralPath $entry.Key).Hash}else{''}
        if($after -ne $entry.Value) {throw "Renderer test changed $($entry.Key)"}
    }
    @{binary=$binaryPath;sha256=(Get-FileHash -LiteralPath $binaryPath).Hash;cases=$results;
        scope='Real Original/EX, D3D12/Vulkan, Software/GPU starts and 32 live renderer switches; app disables installed layer, no launcher suppression; protected settings/injector files unchanged'} |
        ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'manifest.json')
} finally {
    if($process) {$process.Refresh();if(!$process.HasExited) {Stop-Process -Id $process.Id};$process.Dispose()}
    Get-ChildItem Env: | Where-Object {$_.Name -match $environmentPattern} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
