param([string]$Binary='build/release/starfox_pc.exe',
    [string]$OutputDirectory='tmp/plain-menu-check',
    # Live navigation includes a press at frame 62; shorter runs cannot check it.
    [ValidateRange(66,240)][int]$Frames=80)
$ErrorActionPreference='Stop'
$binaryPath=[IO.Path]::GetFullPath($Binary)
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output) {throw 'Choose a new output directory; existing results will not be overwritten'}
New-Item -ItemType Directory -Path $output | Out-Null
$preferences=Join-Path ([IO.Path]::GetDirectoryName($binaryPath)) 'pregame.cfg'
$preferencesHash=if(Test-Path -LiteralPath $preferences) {(Get-FileHash -LiteralPath $preferences).Hash}else{''}
$reshadeLog=Join-Path ([IO.Path]::GetDirectoryName($binaryPath)) 'ReShade.log'
$reshadeLogHash=if(Test-Path -LiteralPath $reshadeLog) {(Get-FileHash -LiteralPath $reshadeLog).Hash}else{''}
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
$results=@()
function Invoke-MenuCheck([string]$Name,[string]$Renderer,[string]$Driver,[string]$Experience,
    [bool]$Heavy,[int]$Dlss=0,[int]$Dlss45=0,[bool]$Preview=$false,[string]$Presses='',[int]$AaType=-1,[int]$Stereo=0) {
    $directory=Join-Path $output $Name
    New-Item -ItemType Directory -Path $directory | Out-Null
    $settings=@{
        # Installed system-wide Vulkan hook: isolate this test process without
        # changing the user's manifest, configuration or other applications.
        DISABLE_VK_LAYER_reshade_1='1';
        SDL_AUDIODRIVER='dummy';STARFOX_TEST_FRAMES="$Frames";STARFOX_TEST_HIDDEN='1';
        STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_UNPACED='1';STARFOX_TEST_VSYNC='0';
        STARFOX_TEST_EXPERIENCE=$Experience;STARFOX_TEST_RENDERER=$Renderer;
        STARFOX_TEST_DISPLAY_MODE='16_10';STARFOX_TEST_SHOW_FPS='0';
        STARFOX_TEST_PRESENTATION_FPS='120';STARFOX_TEST_TIMING_MODE='ORIGINAL';
        STARFOX_TEST_RENDER_SCALE=$(if($Heavy){'10'}else{'1'});
        STARFOX_TEST_DLSS_SELECTION="$Dlss";STARFOX_TEST_DLSS45_SELECTION="$Dlss45";
        STARFOX_TEST_FSR1_SELECTION='0';STARFOX_TEST_STEREO_OUTPUT="$Stereo";
        STARFOX_TEST_RAY_TRACING=$(if($Heavy){'1'}else{'0'});
        STARFOX_TEST_REFLECTIVE_SURFACES=$(if($Heavy){'3'}else{'0'});
        STARFOX_TEST_BLOOM=$(if($Heavy){'3'}else{'0'});
        STARFOX_TEST_BLOOM_2D=$(if($Heavy){'3'}else{'0'});
        STARFOX_TEST_ANTI_ALIASING=$(if($Heavy){'3'}else{'0'});
        STARFOX_TEST_AA_TYPE=$(if($AaType -ge 0){"$AaType"}elseif($Heavy){'6'}else{'0'});
        STARFOX_TEST_2D_FILTER=$(if($Heavy){'5'}else{'0'});
        STARFOX_TEST_RTX_LIGHTING=$(if($Heavy){'3'}else{'0'});
        STARFOX_TEST_EFFECT=$(if($Heavy){'6'}else{'0'});
        STARFOX_TEST_WORLD_EFFECT=$(if($Heavy){'6'}else{'0'});
        STARFOX_TEST_MATERIAL=$(if($Heavy){'54'}else{'0'});
        STARFOX_TEST_MANIPULATION=$(if($Heavy){'60'}else{'0'});
        STARFOX_TEST_MODEL_FX=$(if($Heavy){'84'}else{'0'});
        STARFOX_TEST_WORLD_FX=$(if($Heavy){'85'}else{'0'});
        STARFOX_TEST_WORLD_DISTORTION=$(if($Heavy){'86'}else{'0'});
        STARFOX_TEST_GLOBAL_ENHANCEMENTS=$(if($Heavy){'67108863'}else{'0'});
        STARFOX_TEST_SCENE_ENHANCEMENTS=$(if($Heavy){'255'}else{'0'});
        STARFOX_TEST_DEPTH_ENHANCEMENTS=$(if($Heavy){'15'}else{'0'});
        STARFOX_TEST_PARTICLE_ENHANCEMENTS=$(if($Heavy){'15'}else{'0'});
        STARFOX_TEST_PHOSPHOR_PERSISTENCE=$(if($Heavy){'3'}else{'0'});
        STARFOX_TEST_ADAPTIVE_EXPOSURE=$(if($Heavy){'3'}else{'0'});
        STARFOX_TEST_HDR_EFFECT=$(if($Heavy){'3'}else{'0'});
        STARFOX_TEST_CHROMATIC_ABERRATION=$(if($Heavy){'3'}else{'0'});
        STARFOX_TEST_CAMERA_RESPONSE=$(if($Heavy){'63'}else{'0'});
        STARFOX_TEST_MOTION_BLUR_QUALITY=$(if($Heavy){'3'}else{'0'});
        STARFOX_TEST_VOLUMETRIC_FOG=$(if($Heavy){'1'}else{'0'});
        STARFOX_TEST_ENVIRONMENT_0=$(if($Heavy){'1'}else{'0'});
        STARFOX_TEST_ENVIRONMENT_1=$(if($Heavy){'9'}else{'0'});
        STARFOX_TEST_ENVIRONMENT_2='0';STARFOX_TEST_ENVIRONMENT_3=$(if($Heavy){'1'}else{'0'});
        STARFOX_TEST_PRESSES=$Presses;STARFOX_TEST_PRESS_FRAMES='3';
        STARFOX_TRACE_GPU='1';STARFOX_TRACE_GPU_RAYS='1';STARFOX_TRACE_SCENE_FX='1';
        STARFOX_TRACE_PLAIN_UI='1';
        STARFOX_CAPTURE_PATH=(Join-Path $directory 'menu.bmp');
        STARFOX_CAPTURE_PRESENTATION_PATH=(Join-Path $directory 'display.bmp');
        STARFOX_CAPTURE_PRESENTATION_FIRST="$Frames";STARFOX_CAPTURE_PRESENTATION_LAST="$Frames"
    }
    if($Driver) {$settings.SDL_GPU_DRIVER=$Driver}
    if($Preview) {
        $settings.STARFOX_TEST_MENU_PREVIEW='1'
        $settings.STARFOX_CAPTURE_LOADING_PATH=Join-Path $directory 'rendering.bmp'
    }
    $process=$null
    try {
        foreach($entry in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
        $log=Join-Path $directory 'runtime.log'
        # No stage arguments: this is the real initial setup screen, not a
        # diagnostic gameplay frame with an options overlay.
        $process=Start-Process -FilePath $binaryPath -WindowStyle Hidden -PassThru -RedirectStandardError $log
        $handle=$process.Handle
        while(!$process.WaitForExit(30000)) {
            Write-Output "Menu check still running: $Name, PID $($process.Id), $log"
        }
        if($process.ExitCode -ne 0) {throw "Menu check exited with $($process.ExitCode): $log"}
        if(!(Test-Path -LiteralPath (Join-Path $directory 'menu.bmp')) -or
            !(Test-Path -LiteralPath (Join-Path $directory 'display.bmp'))) {throw "Missing menu capture: $Name"}
        $text=Get-Content -LiteralPath $log -Raw
        if($Preview) {
            if(!(Test-Path -LiteralPath (Join-Path $directory 'rendering.bmp'))) {throw 'Preview did not present its loading indicator'}
            if((Get-FileHash -LiteralPath (Join-Path $directory 'rendering.bmp')).Hash -eq
                (Get-FileHash -LiteralPath (Join-Path $directory 'menu.bmp')).Hash) {throw 'Preview remained on the loading indicator'}
            if($text -notmatch 'native-pipeline:') {throw 'Preview did not enter the scene pipeline'}
        } else {
            $samples=[regex]::Matches($text,'plain-menu: frame=(\d+) scale=1 effects=0 scene=0 work-us=(\d+)')
            if($samples.Count -ne $Frames) {throw "Not all $Frames menu frames used the plain path: $Name"}
            if($text -match 'viewport configured|dlss-gameplay: evaluated|dlss-preview:|dlss-presentation-lifecycle:|dlss-presentation: explicit frame-end|native-pipeline:|motion-blur-live:|depth-fx:|volumetric-fog:|native-scene-fx:.* fog=[1-9]|ray-water:|DXR rendering pipeline initialized') {
                throw "Scene/effect work ran with Preview OFF: $Name"
            }
            $uploads=[regex]::Matches($text,'plain-ui-texture: uploaded').Count
            $retained=[regex]::Matches($text,'plain-ui-texture: retained').Count
            if($uploads+$retained -ne $Frames) {throw "Missing plain UI texture decisions: $Name"}
            if(!$Presses -and ($uploads -gt 4 -or $retained -lt $Frames-4)) {
                throw "Unchanged plain UI was repeatedly uploaded: $Name"
            }
            [long[]]$times=@($samples | Where-Object {[int]$_.Groups[1].Value -ge 10 -and [int]$_.Groups[1].Value -lt $Frames-1} |
                ForEach-Object {[long]$_.Groups[2].Value} | Sort-Object)
            $script:results+=@{name=$Name;renderer=$Renderer;driver=$Driver;experience=$Experience;
                heavy=$Heavy;dlss=$Dlss;dlss45=$Dlss45;stereo=$Stereo;aa_type=[int]$settings.STARFOX_TEST_AA_TYPE;frames=$samples.Count;
                menu_uploads=$uploads;menu_retained=$retained;
                work_median_us=$times[[int][Math]::Floor($times.Length*.5)];
                work_p95_us=$times[[int][Math]::Floor($times.Length*.95)];
                display_sha256=(Get-FileHash -LiteralPath (Join-Path $directory 'display.bmp')).Hash}
        }
    } finally {
        # Do not kill a live diagnostic on an observation/error path. The
        # retained process handle above is waited in bounded chunks to exit.
        foreach($key in $settings.Keys) {Remove-Item -LiteralPath "Env:$key" -ErrorAction SilentlyContinue}
    }
}
try {
    foreach($experience in 'ORIGINAL','EX') {
        foreach($backend in @(@('SOFTWARE',''),@('GPU','direct3d12'),@('GPU','vulkan'))) {
            $name="$experience-$($backend[0])-$($backend[1])"
            Invoke-MenuCheck "$name-baseline" $backend[0] $backend[1] $experience $false
            Invoke-MenuCheck "$name-heavy" $backend[0] $backend[1] $experience $true
            $pair=@($results | Select-Object -Last 2)
            if($pair[0].display_sha256 -ne $pair[1].display_sha256) {throw "Enhancements changed the Preview-OFF display: $name"}
            # SSAA requests extra raster dimensions only for actual scenes.
            # A plain menu must not allocate/render those extra samples either.
            Invoke-MenuCheck "$name-heavy-SSAA" $backend[0] $backend[1] $experience $true -AaType 3
            if($pair[0].display_sha256 -ne $results[-1].display_sha256) {throw "SSAA changed the Preview-OFF display: $name"}
            Invoke-MenuCheck "$name-SR-preview-off" $backend[0] $backend[1] $experience $true -Stereo 9
            if($pair[0].display_sha256 -ne $results[-1].display_sha256) {throw "SR Platform changed the Preview-OFF display: $name"}
        }
    }
    foreach($model in 'standard','4.5') {
        Invoke-MenuCheck "GPU-dlss-$model" 'GPU' 'direct3d12' 'ORIGINAL' $true $(if($model -eq 'standard'){1}else{0}) $(if($model -eq '4.5'){1}else{0})
        Invoke-MenuCheck "SOFTWARE-to-GPU-$model" 'SOFTWARE' '' 'ORIGINAL' $true $(if($model -eq 'standard'){1}else{0}) $(if($model -eq '4.5'){1}else{0}) $false '5:1024,14:1024,23:1024,32:1024,41:1024,50:256'
        $transition=Get-Content -LiteralPath (Join-Path $output "SOFTWARE-to-GPU-$model/runtime.log") -Raw
        if([regex]::Matches($transition,'renderer-picker: requested=AUTO mode=GPU actual=gpu driver=direct3d12').Count -ne 1 -or
            $transition -match 'mode=GPU actual=gpu driver=vulkan' -or
            [regex]::Matches($transition,'dlss-lifecycle: restarted before renderer creation').Count -ne 1) {
            throw "Software -> GPU did not directly restore D3D12/DLSS: $model"
        }
    }
    # Navigate to 3D and change AA while the preview is disabled.
    Invoke-MenuCheck 'GPU-options-navigation' 'GPU' 'direct3d12' 'ORIGINAL' $true 0 0 $false '5:2048,14:2048,23:2048,32:2048,41:2048,50:128,62:256'
    Invoke-MenuCheck 'GPU-preview-loading' 'GPU' 'direct3d12' 'ORIGINAL' $false 0 0 $true
    $hashAfter=if(Test-Path -LiteralPath $preferences) {(Get-FileHash -LiteralPath $preferences).Hash}else{''}
    if($hashAfter -ne $preferencesHash) {throw 'Saved preferences changed during diagnostics'}
    $reshadeLogAfter=if(Test-Path -LiteralPath $reshadeLog) {(Get-FileHash -LiteralPath $reshadeLog).Hash}else{''}
    if($reshadeLogAfter -ne $reshadeLogHash) {throw 'ReShade injector log changed; menu isolation is not accepted'}
    @{executable=$binaryPath;sha256=(Get-FileHash -LiteralPath $binaryPath).Hash;
        preferences_unchanged=$true;reshade_log_unchanged=$true;results=$results;loading_capture='GPU-preview-loading/rendering.bmp'} |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'results.json')
    $results | ForEach-Object {[pscustomobject]$_} | Format-Table name,frames,work_median_us,work_p95_us -AutoSize
    Write-Output 'PASS: plain UI is unchanged by heavy enhancements; preview loading presented; preferences unchanged.'
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} |
        ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
