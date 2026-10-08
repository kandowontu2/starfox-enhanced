param([string]$Binary='build/release/starfox_pc.exe',
    [string]$OutputDirectory='tmp/camera-observer-frontend-check')
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output) {throw 'Choose a new output directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$preferences=Join-Path (Split-Path ([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$before=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
$cases=@(
    @{name='software';Renderer='SOFTWARE'},
    @{name='d3d12';Renderer='GPU';GpuBackend='direct3d12'},
    @{name='vulkan-taa';Renderer='GPU';GpuBackend='vulkan';Taa=$true},
    @{name='d3d12-dlss45';Renderer='GPU';GpuBackend='direct3d12';Dlss45=1},
    @{name='sbs';Renderer='GPU';GpuBackend='direct3d12';Stereo=2})
$results=@()
foreach($case in $cases) {
    $settings=@{Binary=$Binary;OutputDirectory=(Join-Path $output $case.name);
        Frames=60;CaptureFirst=10;CaptureInterval=10;PrerollTicks=1000;
        Experience='ORIGINAL';Stage='LEVEL1_1';GroundEnabled=0;Ground=0;Sky=0;
        RayTracing=0;Reflections=0;RenderScale=1;CameraResponse=12;
        Presses='2:16384,14:4096,34:4096,40:16384';PressFrames=3}
    foreach($key in $case.Keys) {if($key -ne 'name') {$settings[$key]=$case[$key]}}
    & (Join-Path $PSScriptRoot 'capture_lava.ps1') @settings
    $log=Get-Content -LiteralPath (Join-Path $settings.OutputDirectory 'runtime.log') -Raw
    $poses=[regex]::Matches($log,'(?m)^camera-response-pose: ([^\r\n]+)')
    if($poses.Count -ne 60) {throw "Missing camera event observations: $($case.name)"}
    $frozen=$poses[20].Groups[1].Value
    if([double]($frozen.Split(',')[0]) -ge 0 -or $frozen -notmatch 'shot=1$') {
        throw "No actual player recoil was retained at pause: $($case.name)"
    }
    foreach($index in 21..30) {
        if($poses[$index].Groups[1].Value -ne $frozen) {throw "Paused response changed: $($case.name) frame=$index"}
    }
    if($poses[59].Groups[1].Value -eq $frozen -or $log -notmatch 'camera-response-pose: [^\r\n]+ shot=2') {
        throw "Resume failed to advance response or observe the next real shot: $($case.name)"
    }
    $path=if($case.Renderer -eq 'SOFTWARE'){'software'}else{'resident'}
    if($log -notmatch "camera-response: $path world/HUD=1") {throw "Camera did not compose: $($case.name)"}
    if($case.Taa -and ($log -notmatch 'taa: resolved' -or $log -notmatch 'temporal-paused-native:')) {
        throw 'TAA pause/reset path was not exercised'
    }
    if($case.Dlss45 -and $log -notmatch 'temporal-paused-native:') {throw 'DLSS pause/reset path was not exercised'}
    $results+=@{name=$case.name;observations=$poses.Count;retained_pause=$true;second_shot=$true}
    Write-Output "PASS $($case.name): player recoil, exact paused pose, resumed second shot and world/HUD composition"
}
$after=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
if($after -ne $before) {throw 'Saved preferences changed during camera checks'}
@{executable=[IO.Path]::GetFullPath($Binary);sha256=(Get-FileHash -LiteralPath $Binary).Hash;
    preferences_unchanged=$true;results=$results} | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output 'PASS: shared camera observer, temporal pause resets and saved preferences'
