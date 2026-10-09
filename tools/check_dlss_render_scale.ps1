param([string]$Binary='build/release/starfox_pc.exe',
    [string]$OutputDirectory='tmp/dlss-render-scale',
    [ValidateSet('ORIGINAL','EX')][string[]]$Experiences=@('ORIGINAL','EX'),
    [ValidateSet('4_3','16_9','32_9')][string]$Display='32_9',
    [ValidateRange(1,6)][int[]]$Scales=@(1,2,6),
    [ValidateRange(1,4)][int[]]$Modes=@(1,2,3,4),
    # The frozen preview evaluates 32 samples, then must present at least one
    # retained frame. A 32-frame run cannot satisfy the reuse assertion below.
    [ValidateRange(33,120)][int]$Frames=40)
$ErrorActionPreference='Stop'
$exe=[IO.Path]::GetFullPath($Binary)
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Use a new output directory; existing evidence will not be overwritten'}
New-Item -ItemType Directory -Path $output | Out-Null
$binaryHash=(Get-FileHash -LiteralPath $exe).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName($exe)) 'pregame.cfg'
$preferencesHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$results=@()
try {
    $settings=@{SDL_AUDIODRIVER='dummy';SDL_GPU_DRIVER='direct3d12';STARFOX_TEST_HIDDEN='1';
        STARFOX_TEST_FRAMES="$Frames";STARFOX_TEST_UNPACED='1';STARFOX_TEST_VSYNC='0';
        STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS='1000';STARFOX_TEST_MENU_PREVIEW='1';
        STARFOX_TEST_RENDERER='GPU';STARFOX_TEST_DISPLAY_MODE=$Display;STARFOX_TEST_STEREO_OUTPUT='0';
        STARFOX_TEST_FSR1_SELECTION='0';STARFOX_TEST_DLSS_SELECTION='0';STARFOX_TEST_DLSS45_SELECTION='0';
        STARFOX_TEST_AA_TYPE='0';STARFOX_TEST_ANTI_ALIASING='0';STARFOX_TEST_RENDER_SCALE_AUDIT='1';
        STARFOX_TEST_RAY_TRACING='0';STARFOX_TEST_REFLECTIVE_SURFACES='0';STARFOX_TEST_CAMERA_RESPONSE='0';
        STARFOX_TEST_ENVIRONMENT_0='0';STARFOX_TEST_ENVIRONMENT_3='1';
        STARFOX_TEST_BLOOM='0';STARFOX_TEST_BLOOM_2D='0';STARFOX_TEST_EFFECT='0';
        STARFOX_TEST_WORLD_EFFECT='0';STARFOX_TEST_GLOBAL_ENHANCEMENTS='0';
        STARFOX_TEST_SCENE_ENHANCEMENTS='0';STARFOX_TEST_DEPTH_ENHANCEMENTS='0';
        STARFOX_TEST_PARTICLE_ENHANCEMENTS='0';STARFOX_TEST_ADAPTIVE_EXPOSURE='0';
        STARFOX_TEST_MOTION_BLUR_QUALITY='0';STARFOX_TEST_VOLUMETRIC_FOG='0';STARFOX_TEST_MATERIAL='0'}
    foreach($entry in $settings.GetEnumerator()){Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
    foreach($experience in $Experiences){foreach($scale in $Scales){foreach($model in 'standard','4.5'){foreach($mode in $Modes){
        $env:STARFOX_TEST_EXPERIENCE=$experience;$env:STARFOX_TEST_RENDER_SCALE="$scale"
        $env:STARFOX_TEST_DLSS_SELECTION=if($model -eq 'standard'){"$mode"}else{'0'}
        $env:STARFOX_TEST_DLSS45_SELECTION=if($model -eq '4.5'){"$mode"}else{'0'}
        $name="$experience-$model-mode$mode-scale$scale";$log=Join-Path $output "$name.log"
        $arguments=if($experience -eq 'EX'){'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL1_1'}
            else{'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_1'}
        $process=Start-Process $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError $log
        $handle=$process.Handle
        if(!$process.WaitForExit(60000)){throw "Scale check still running: PID $($process.Id), $log"}
        if($process.ExitCode -ne 0){throw "Scale check failed: $name; $log"}
        $text=Get-Content -LiteralPath $log -Raw
        $expectedWidth=@{'4_3'=256;'16_9'=400;'32_9'=800}[$Display]*$scale;$expectedHeight=224*$scale
        if($text -notmatch "render-scale-audit: selected=$scale effective=$scale output=${expectedWidth}x${expectedHeight} "){
            throw "DLSS changed selected render scale or target size: $name; $log"
        }
        $evaluations=@([regex]::Matches($text,'dlss-gameplay: evaluated frame=[^\r\n]+'))
        if($evaluations.Count -lt 32 -or $text -match 'dlss-gameplay: failed|dlss-fallback: unjittered scene replay|replaying complete frame'){
            throw "Scale preservation bypassed or failed actual DLSS reconstruction: $name; $log"
        }
        foreach($evaluation in $evaluations){
            if($evaluation.Value -notmatch " output=${expectedWidth}x${expectedHeight}$"){
                throw "SDK output did not use selected render upscale: $name; $log"
            }
        }
        if($text -notmatch 'dlss-preview: reused reconstructed frame='){
            throw "Frozen preview did not retain the reconstructed image: $name; $log"
        }
        $results+=[pscustomobject]@{experience=$experience;display=$Display;model=$model;mode=$mode;scale=$scale;
            output_width=$expectedWidth;output_height=$expectedHeight;sdk_evaluations=$evaluations.Count}
        Write-Output "$name : selected/output ${scale}x preserved; SDK evaluations=$($evaluations.Count)."
    }}}}
    if((Get-FileHash -LiteralPath $exe).Hash -ne $binaryHash){throw 'Executable changed during scale diagnostics'}
    @{executable=$exe;sha256=$binaryHash;cases=$results} | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $output 'results.json')
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} |
        ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $saved.GetEnumerator()){Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
    $after=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
    if($after -ne $preferencesHash){throw 'Saved preferences changed during scale diagnostics'}
}
