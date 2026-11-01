param(
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name='baseline',
    [ValidateSet('vulkan','direct3d12')][string]$Driver='vulkan',
    [ValidateSet('ORIGINAL','EX')][string]$Experience='ORIGINAL',
    [ValidateRange(240,10000)][int]$Frames=3000,
    [string]$OutputDirectory='build/validation/active',
    [switch]$LowPower,[switch]$LiveSettings,[switch]$HostCartridge,[switch]$Direct
)
# No executable, ROM, save or preference copies. Run hidden, restore diagnostic
# environment overrides, and stop only the process this check created.
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe=Join-Path $root 'build/current/starfox_pc.exe'
$output=[IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
$scratch=[IO.Path]::GetFullPath((Join-Path $root 'build/validation/active'))
if([IO.Path]::GetPathRoot($output) -ine 'C:\' -or ($output -ine $scratch -and
    !$output.StartsWith($scratch+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase))) {
    throw 'Output must remain within the active C: validation scratch set.'
}
$validation=Join-Path $root 'build/validation'
$bytes=[long]0
foreach($file in Get-ChildItem -LiteralPath $validation -File -Recurse -ErrorAction SilentlyContinue) {$bytes+=$file.Length}
if($bytes+4000000 -gt 1GB -or (Get-PSDrive C).Free-4000000 -lt 20GB) {
    throw 'Validation scratch budget or 20 GiB free-space reserve would be exceeded.'
}
$null=New-Item -ItemType Directory -Path $output -Force
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$hashes=@{}
foreach($path in @((Join-Path $root 'pregame.cfg'),(Join-Path $root 'build/current/pregame.cfg'))) {
    if(Test-Path -LiteralPath $path) {$hashes[$path]=(Get-FileHash -LiteralPath $path).Hash}
}
$process=$null
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy';SDL_GPU_DRIVER=$Driver;DISABLE_VK_LAYER_reshade_1='1'
        STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES="$Frames";STARFOX_TEST_PRESSES='180:4096'
        STARFOX_TEST_UNPACED='1';STARFOX_TEST_GPU_VALIDATION='1';STARFOX_TEST_RENDERER='GPU'
        STARFOX_TEST_EXPERIENCE=$Experience;STARFOX_TEST_VSYNC='0';STARFOX_TEST_RENDER_SCALE='1'
        STARFOX_TEST_DLSS_SELECTION='0';STARFOX_TEST_DLSS45_SELECTION='0';STARFOX_TEST_FSR1_SELECTION='0'
        STARFOX_TEST_STEREO_OUTPUT='0';STARFOX_TEST_BLOOM='0';STARFOX_TEST_ANTI_ALIASING='OFF'
        STARFOX_TEST_ENVIRONMENT_0='0';STARFOX_TEST_ENVIRONMENT_3='0';STARFOX_TEST_RAY_TRACING='0'
        STARFOX_TRACE_GPU='1';STARFOX_TEST_MSU1='0';STARFOX_TEST_WINDOW_SIZE='640x480'
    }
    if($LowPower) {$settings.STARFOX_TEST_LOW_POWER_GPU='1'}
    if($LiveSettings) {$settings.STARFOX_TEST_GPU_SETTINGS_CYCLE='1'}
    if($HostCartridge) {$settings.STARFOX_TEST_HOST_CARTRIDGE='1'}
    foreach($item in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($item.Key)" $item.Value}
    $log=Join-Path $output "issue49-$Name-$Driver.log"
    $stdout=Join-Path $output "issue49-$Name-$Driver.stdout.log"
    $start=@{FilePath=$exe;WorkingDirectory=$root;WindowStyle='Hidden';PassThru=$true;RedirectStandardError=$log;RedirectStandardOutput=$stdout}
    if($Direct) {$start.ArgumentList='LEVEL1_1'}
    $process=Start-Process @start
    $null=$process.Handle
    if(!$process.WaitForExit(180000)) {throw "Owned validation process timed out: $($process.Id)"}
    $process.Refresh()
    if($process.ExitCode -ne 0) {throw "Validation exited $($process.ExitCode): $log"}
    $text=Get-Content -LiteralPath $log -Raw
    if($text -match 'VK_ERROR_DEVICE_LOST|Validation Error|VUID-|GPU effects fallback|native-geometry:.*replaying complete frame') {throw "GPU failure or validation error: $log"}
    if($text -notmatch 'native-pipeline: resident raster') {throw "Resident GPU gameplay not reached: $log"}
    if($HostCartridge -and ($text -notmatch 'gpu-compatibility:.*host cartridge' -or $text -match 'native-pipeline: GPU late cartridge layers|native-background: GPU resident ordered layers')) {throw "Unsafe cartridge path entered: $log"}
    if($LiveSettings -and ($text -notmatch 'gpu-settings-cycle:' -or ($HostCartridge -and $text -notmatch 'gpu-settings: retired'))) {throw "Live settings retirement not exercised: $log"}
    "PASS issue49 $Name $Driver frames=$Frames"
} finally {
    if($process) {
        $process.Refresh()
        if(!$process.HasExited) {Stop-Process -Id $process.Id;$process.WaitForExit()}
        $process.Dispose()
    }
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($item in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($item.Key)" $item.Value}
    foreach($path in $hashes.Keys) {if((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]) {throw "Settings changed: $path"}}
}
