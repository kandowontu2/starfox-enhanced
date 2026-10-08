param([string]$Binary='build/release/starfox_pc.exe',
    [string]$OutputDirectory='tmp/embedded-dlss-check',
    [ValidateRange(8,120)][int]$Frames=16)
$ErrorActionPreference='Stop'
$executable=(Resolve-Path -LiteralPath $Binary).Path
if(Test-Path -LiteralPath (Join-Path (Split-Path $executable) 'dlss')) {
    throw 'Use an embedded build without an adjacent dlss folder; this test must not pass through loose runtime DLLs'
}
# This is an RTX/D3D12 hardware integration check, not a GPU-less CI test.
# capture_lava restores all diagnostic environment variables after each run.
foreach($model in 'standard','4.5') {foreach($mode in 1,2,3,4) {
    $output=Join-Path $OutputDirectory "$model-mode-$mode"
    $selection=if($model -eq 'standard'){@{Dlss=$mode}}else{@{Dlss45=$mode}}
    & (Join-Path $PSScriptRoot 'capture_lava.ps1') -Binary $executable `
        -OutputDirectory $output -Experience ORIGINAL -Stage LEVEL1_1 `
        -Frames $Frames -PrerollTicks 1000 -GroundEnabled 0 -Sky 0 `
        -RayTracing 0 -Reflections 0 @selection -RenderScale 2 -GpuBackend direct3d12
    $log=Get-Content -LiteralPath (Join-Path $output 'runtime.log') -Raw
    if($log -notmatch 'using verified embedded standard runtime' -or
        $log -notmatch $(if($model -eq 'standard'){"dlss-model: requested standard DLSS preset K; mode=$mode"}else{"dlss-model: requested DLSS 4.5 preset M; mode=$mode"}) -or
        ([regex]::Matches($log,'dlss-gameplay: evaluated frame=')).Count -lt $Frames -or
        $log -match 'dlss-lifecycle: unavailable|dlss-gameplay: failed') {
        throw "Embedded DLSS $model mode $mode did not initialize and evaluate every frame"
    }
    $ends=[regex]::Match($log,'dlss-presentation-lifecycle: completed=(\d+) attempts=(\d+)')
    if(!$ends.Success -or [int]$ends.Groups[1].Value -ne $Frames -or [int]$ends.Groups[2].Value -ne $Frames -or
        $log -notmatch 'dlss-presentation: explicit frame-end; native swapchain retained' -or
        $log -match 'dlss-presentation: upgraded|dlss-presentation: restored|dlss-sdk-(error|warning)|frame-end failed') {
        throw "Embedded DLSS $model mode $mode did not retain native presentation and finish every SDK frame cleanly"
    }
}}
foreach($backend in 'direct3d12','vulkan') {
    $output=Join-Path $OutputDirectory "off-$backend"
    & (Join-Path $PSScriptRoot 'capture_lava.ps1') -Binary $executable `
        -OutputDirectory $output -Experience ORIGINAL -Stage LEVEL1_1 `
        -Frames $Frames -PrerollTicks 1000 -GroundEnabled 0 -Sky 0 `
        -RayTracing 0 -Reflections 0 -Dlss 0 -RenderScale 2 -GpuBackend $backend
    $log=Get-Content -LiteralPath (Join-Path $output 'runtime.log') -Raw
    if($log -match 'dlss-gameplay: evaluated|dlss-presentation: upgraded|dlss-presentation-lifecycle:|dlss-presentation: explicit frame-end') {
        throw "Disabled DLSS evaluated or replaced the ordinary $backend swapchain"
    }
}
Write-Output 'Separate embedded DLSS: four preset-K and four preset-M modes evaluated on D3D12; OFF preserved ordinary D3D12/Vulkan.'
