param(
    [string]$OutputDirectory='tmp/sbs-presentation-check',
    [ValidateSet('ORIGINAL','EX')][string]$Experience='ORIGINAL',
    [ValidateRange(1,2)][int]$Mode=2,
    [ValidateRange(0,3)][int]$Bloom=0,
    [switch]$GridLines,
    [switch]$Profile,
    [switch]$Sequence,
    [switch]$ScrambleWipe,
    [ValidateSet(30,60,120,144,240)][int]$PresentationFps=60,
    [ValidatePattern('^[A-Za-z0-9_]+$')][string]$Level='LEVEL1_1',
    [switch]$RequireDust,
    [switch]$RequireText,
    [switch]$LevelClear,
    [switch]$Ending,
    [switch]$RayTracing,
    [switch]$RequireResidentGeometry,
    [switch]$ForceMonoFallback,
    [switch]$PortableShadows,
    [switch]$DownloadShadows,
    [ValidateRange(0,10000)][int]$PrerollTicks=1000,
    [ValidateRange(1,10000)][int]$Frames=48
)
$ErrorActionPreference='Stop'
if($Ending -and $LevelClear) {throw 'Choose either Ending or LevelClear'}
if($ForceMonoFallback -and (!$RayTracing -or $DownloadShadows)) {throw 'Forced fallback requires resident ray tracing'}
if($Sequence -and ($Profile -or $Frames -gt 960)) {throw 'Sequence capture is limited to 960 frames and cannot be profiled'}
$sbsPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $sbsPath | Out-Null
$capture=Join-Path $sbsPath 'presentation.bmp'
$log=Join-Path $sbsPath 'runtime.log'
$env:SDL_AUDIODRIVER='dummy'
$env:STARFOX_TEST_HIDDEN='1'
$env:STARFOX_TEST_FRAMES="$Frames"
if($Profile -and $Frames -le 60) {throw 'Profiling requires more than 60 frames for warmup'}
$env:STARFOX_TRACE_PROFILE=if($Profile){'1'}else{$null}
$env:STARFOX_TRACE_PROFILE_DISTRIBUTION=if($Profile){'1'}else{$null}
$env:STARFOX_TEST_PROFILE_WARMUP=if($Profile){'60'}else{$null}
$env:STARFOX_TEST_PREROLL_TICKS="$PrerollTicks"
$env:STARFOX_TEST_CLEAR=if($LevelClear){'CL_WARP'}else{$null}
$env:STARFOX_TEST_ENDING=if($Ending){'1'}else{$null}
$env:STARFOX_TEST_ENDING_PREROLL=$null
$env:STARFOX_TEST_PRESENTATION_FPS="$PresentationFps"
$env:STARFOX_TEST_SCRAMBLE_WIPE=if($ScrambleWipe){'1'}else{$null}
$env:STARFOX_TEST_TIMING_MODE='ORIGINAL'
# Startup's black host frames use the mono capture counter. Skip that host-only
# warmup so a forced mono fallback captures gameplay, not the loading screen.
$env:STARFOX_TEST_SKIP_PREROLL='1'
$env:STARFOX_TEST_UNPACED='1'
$env:STARFOX_TEST_STEREO_OUTPUT="$Mode"
$env:STARFOX_TEST_EXPERIENCE=$Experience
$env:STARFOX_TEST_DISPLAY_MODE='16_9'
$env:STARFOX_TEST_RENDER_SCALE='1'
$env:STARFOX_TEST_RENDERER='GPU'
$env:STARFOX_TEST_BLOOM="$Bloom"
$env:STARFOX_TEST_GRID_LINES=if($GridLines){'1'}else{$null}
if($GridLines -and $Experience -ne 'EX') {throw 'Connected grid capture requires EX'}
$env:STARFOX_TEST_RAY_TRACING=if($RayTracing -or $PortableShadows){'1'}else{'0'}
$env:STARFOX_DISABLE_DXR=if($PortableShadows){'1'}else{$null}
$env:STARFOX_TEST_FORCE_PORTABLE_SHADOWS=if($PortableShadows){'1'}else{$null}
$env:STARFOX_TEST_STEREO_SHADOW_DOWNLOAD=if($DownloadShadows){'1'}else{$null}
$env:STARFOX_TEST_VSYNC='0'
$env:STARFOX_TRACE_GPU='1'
$env:STARFOX_TRACE_GPU_RAYS=if($RequireResidentGeometry){'1'}else{$null}
$env:STARFOX_TEST_FAIL_STEREO_PRESENT=if($ForceMonoFallback){'1'}else{$null}
$env:STARFOX_CAPTURE_PRESENTATION_PATH=$capture
$env:STARFOX_CAPTURE_PRESENTATION_SEQUENCE=if($Sequence){'1'}else{$null}
$arguments=if($Experience -eq 'EX') {
    "tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt $Level"
} else {"upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT $Level"}
$started=Get-Date
$captureProcess=Start-Process -FilePath build/current/starfox_pc.exe -ArgumentList $arguments `
    -WindowStyle Hidden -PassThru -RedirectStandardError $log
$captureHandle=$captureProcess.Handle
if(!$captureProcess.WaitForExit(60000)) {
    throw "Capture still running: PID $($captureProcess.Id). Do not restart; inspect this process."
}
if($captureProcess.ExitCode -ne 0) {throw "Capture failed: $($captureProcess.ExitCode); see $log"}
if(!(Test-Path -LiteralPath $capture) -or (Get-Item -LiteralPath $capture).LastWriteTime -lt $started) {
    throw 'No fresh stereo capture was written'
}
$bytes=[IO.File]::ReadAllBytes($capture)
if($bytes.Length -lt 54 -or $bytes[0] -ne 66 -or $bytes[1] -ne 77) {throw 'Invalid BMP capture'}
$width=[BitConverter]::ToInt32($bytes,18)
$height=[Math]::Abs([BitConverter]::ToInt32($bytes,22))
$expectedWidth=if($Mode -eq 2 -and !$ForceMonoFallback){800}else{400}
if($width -ne $expectedWidth -or $height -ne 224) {throw "Unexpected SBS dimensions: ${width}x${height}"}
if($Sequence) {
    for($frame=1;$frame -le $Frames;++$frame) {
        $framePath="$capture.frame-$frame.bmp"
        if(!(Test-Path -LiteralPath $framePath) -or (Get-Item -LiteralPath $framePath).LastWriteTime -lt $started) {
            throw "Missing fresh SBS frame $frame"
        }
        $frameBytes=[IO.File]::ReadAllBytes($framePath)
        if($frameBytes.Length -lt 54 -or $frameBytes[0] -ne 66 -or $frameBytes[1] -ne 77 -or
            [BitConverter]::ToInt32($frameBytes,18) -ne $expectedWidth -or
            [Math]::Abs([BitConverter]::ToInt32($frameBytes,22)) -ne 224) {
            throw "Invalid SBS frame $frame"
        }
    }
    Write-Output "Verified $Frames fresh SBS frames; readback timings are not performance measurements."
}
$trace=Get-Content -LiteralPath $log
if(!$ForceMonoFallback -and ($trace -match 'stereo failure:')) {throw 'Stereo fallback occurred; see runtime.log'}
if($ForceMonoFallback -and (!($trace -match 'injected presentation failure') -or !($trace -match 'shadow-backend: GPU-resident hardware DXR shadows \(GPU caster geometry\)'))) {
    throw 'Forced fallback did not rebuild mono GPU shadows'
}
if($ScrambleWipe -and !($trace -match 'native-pipeline: GPU horizontal scramble wipe')) {throw 'GPU scramble wipe was not exercised'}
if(!$ForceMonoFallback -and !($trace -match "stereo presented: ${expectedWidth}x224")) {throw 'No stereo presentation trace'}
if($GridLines -and !($trace -match 'GPU connected grid recorded')) {throw 'Connected-grid GPU path was not exercised'}
if($RequireDust -and !($trace -match 'GPU dust recorded')) {throw 'Dust GPU path was not exercised'}
if($RequireText -and !($trace -match 'GPU projected text recorded')) {throw 'Projected-text GPU path was not exercised'}
if($RayTracing -and !$PortableShadows -and !($trace -match 'shadow-backend: (Hardware DXR 1.1:|GPU-resident hardware DXR shadows)')) {throw 'Hardware ray tracing was not exercised'}
if($PortableShadows) {
    if(!($trace -match 'shadow-backend: GPU resident compute shadows')) {throw 'Portable shadows were not exercised'}
}
if($RayTracing -or $PortableShadows) {
    $expectedResident=if($DownloadShadows){'0,0'}else{'1,1'}
    if(!($trace -match "stereo-shadow-resident: $expectedResident")) {throw 'Unexpected stereo shadow residency'}
}
if($RequireResidentGeometry -and (!($trace -match 'stereo GPU caster geometry') -or ($trace -match 'ray-scene CPU caster fallback'))) {
    throw 'Stereo did not stay on GPU caster geometry for the complete capture'
}
Write-Output "$Experience SBS mode $Mode passed dimension/submission checks: $capture"
if($Profile) {
    $profileLines=$trace | Where-Object {$_ -match '^render-profile-us '}
    if(!$profileLines) {throw 'No measured render profile was produced'}
    Write-Output $profileLines
}
Write-Output 'Visual inspection, depth correctness and full gameplay parity are separate checks.'
