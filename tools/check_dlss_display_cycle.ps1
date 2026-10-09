param([string]$Binary='build/release/starfox_pc.exe',
    [string]$OutputDirectory='tmp/dlss-display-cycle',
    [string]$Python='python',
    [ValidateSet('Main','3D')][string]$Menu='3D',
    [switch]$CaptureDrawable,
    [switch]$Fullscreen,
    [switch]$NativeModelCheck,
    [switch]$PlainScene,
    [switch]$Paced,
    [ValidateRange(1,6)][int]$RenderScale=4,
    [ValidateRange(1,480)][int]$PresentationFps=60)
$ErrorActionPreference='Stop'
if($NativeModelCheck -and $PlainScene) {
    throw 'NativeModelCheck requires native-identical model pixels; PlainScene requires genuinely reconstructed (different) model pixels. Choose one check.'
}
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output) {throw 'Use a new output directory; existing captures will not be overwritten'}
$common=@{Binary=$Binary;Experience='ORIGINAL';Stage='LEVEL1_1';Frames=96;PrerollTicks=1000;
    GroundEnabled=0;Sky=1;RayTracing=1;Reflections=3;Bloom=3;Display='32_9';
    Exposure=3;SceneEnhancements=255;ParticleEnhancements=15;CameraResponse=51;
    MenuPreview=$true;GpuBackend='direct3d12';RenderScale=$RenderScale;
    CaptureFirst=65;CaptureLast=96;CaptureInterval=1;
    Paced=$Paced.IsPresent;PresentationFps=$PresentationFps}
if($NativeModelCheck -or $PlainScene) {
    if($Menu -ne 'Main') {throw 'Native model comparison requires Main (unchanged quality labels)'}
    $common.RayTracing=0;$common.Reflections=0;$common.Bloom=0;$common.Exposure=0
    $common.SceneEnhancements=0;$common.ParticleEnhancements=0;$common.CameraResponse=0
}
if($CaptureDrawable) {
    $common.CaptureDrawable=$true
}
if($Fullscreen){$common.Fullscreen=$true}
if($Menu -eq '3D') {$common.Presses='5:2048,14:2048,20:2048,29:2048,38:128'}
$capture=Join-Path $PSScriptRoot 'capture_lava.ps1'
& $capture @common -OutputDirectory (Join-Path $output 'native')
$directories=@()
foreach($model in 'k','m') {foreach($mode in 1,2,3,4) {
    $directory=Join-Path $output "$model-mode$mode"
    $selection=if($model -eq 'k'){@{Dlss=$mode}}else{@{Dlss45=$mode}}
    & $capture @common @selection -OutputDirectory $directory
    $log=Get-Content -LiteralPath (Join-Path $directory 'runtime.log') -Raw
    if($log -notmatch 'dlss-preview: reused reconstructed frame=' -or $log -notmatch 'dlss-gameplay: evaluated frame=') {
        throw 'Capture did not verify an actual reconstructed and retained preview; use the latest binary'
    }
    $directories+=$directory
}}
$checker=Join-Path $PSScriptRoot 'check_dlss_display_preview.py'
# Main-menu labels/values are unchanged by DLSS. The 3D submenu has different
# quality values, so compare only its stable labels. Render Upscale is independent.
$regions=if($Menu -eq 'Main') {@('--menu-region','.38','.10','.64','.97')} else {
    @('--menu-region','.38','.10','.50','.35','--menu-region','.38','.40','.50','.53',
      '--menu-region','.38','.53','.56','.97')
}
[string[]]$drawableCheck=if($CaptureDrawable){@('--drawable')}else{@()}
[string[]]$modelCheck=if($NativeModelCheck){@('--native-models')}else{@()}
[string[]]$holdCheck=if($PlainScene){@('--held-models')}else{@()}
$json=& $Python $checker (Join-Path $output 'native') @directories --first 65 --last 96 --step 1 --include-native @regions @drawableCheck @modelCheck @holdCheck
if($LASTEXITCODE -ne 0) {throw 'DLSS full-cycle preview check failed; captures retained for diagnosis'}
$report=@{executable=[IO.Path]::GetFullPath($Binary);sha256=(Get-FileHash -LiteralPath $Binary).Hash;
    capture_first=65;capture_last=96;capture_interval=1;paced=$Paced.IsPresent;
    presentation_fps=$PresentationFps;render_scale=$RenderScale;menu=$Menu;drawable=$CaptureDrawable.IsPresent;
    fullscreen=$Fullscreen.IsPresent;native_model_check=$NativeModelCheck.IsPresent;
    plain_scene=$PlainScene.IsPresent;
    results=($json | ConvertFrom-Json)}
$report | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $output 'stability.json')
Write-Output $json
# These are newly generated diagnostic frames owned by this run. Preserve the
# first/last comparisons, final image, logs and report; discard redundant BMPs.
$removed=0L
foreach($directory in @((Join-Path $output 'native'))+$directories) {
    foreach($frame in 66..95) {
        $path=[IO.Path]::GetFullPath((Join-Path $directory "lava.bmp.frame-$frame.bmp"))
        if(!$path.StartsWith($output+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
            throw 'Capture cleanup target escaped this run output directory'
        }
        if(Test-Path -LiteralPath $path) {
            $removed+=(Get-Item -LiteralPath $path).Length
            Remove-Item -LiteralPath $path
        }
    }
}
Write-Output "DLSS full-cycle preview passed. Removed $removed bytes of redundant generated captures."
