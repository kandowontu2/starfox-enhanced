param([string]$Binary='build/current/starfox_pc.exe',
    [Parameter(Mandatory=$true)][string]$PresentationDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12')
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a new output directory'}
$hash=(Get-FileHash -LiteralPath $Binary).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$preferencesHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
$accepted=@(Get-Content -LiteralPath (Join-Path $PresentationDirectory 'results.json') -Raw | ConvertFrom-Json)
foreach($count in 2,4,8) {
    $case=@($accepted | Where-Object {$_.case -eq "msaa-$count" -and $_.exact -and $_.sha256 -eq $hash})
    if($case.Count -ne 1){throw "Missing accepted $count-sample presentation for this exact executable"}
}
New-Item -ItemType Directory -Path $output | Out-Null
$common=@{Binary=$Binary;GpuBackend=$GpuBackend;Experience='ORIGINAL';Stage='LEVEL1_1';
    PrerollTicks=1000;Frames=32;CaptureFirst=8;CaptureInterval=8;FixedTemporalClock=$true;
    RenderScale=1;GroundEnabled=0;RayTracing=0;Reflections=0}
$off=Join-Path $output 'off'
& (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $off -Stereo 2 -AaType 6 -AaQuality 0
$images=@(Get-ChildItem -LiteralPath $off -Filter '*.bmp')
if($images.Count -ne 5){throw 'Incomplete OFF reference'}
$results=@()
foreach($count in 2,4,8) {
    $active=Join-Path $PresentationDirectory "msaa-$count-direct"
    $changed=0
    foreach($image in $images) {
        $candidate=Join-Path $active $image.Name
        if(!(Test-Path -LiteralPath $candidate)){throw 'Incomplete active reference'}
        if((Get-FileHash -LiteralPath $image.FullName).Hash -ne (Get-FileHash -LiteralPath $candidate).Hash){++$changed}
    }
    if($changed -ne $images.Count){throw "$count-sample MSAA produced a no-op image"}
    $results+=@{case="msaa-$count-not-noop";changed_images=$changed;sha256=$hash}
}
$mono=Join-Path $output 'mono-msaa'
$fault=Join-Path $output 'failed-right-palette'
& (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $mono -Stereo 0 -AaType 6 -AaQuality 2
& (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $fault -Stereo 2 -AaType 6 -AaQuality 2 -FailStereoMsaaResolve
$monoImages=@(Get-ChildItem -LiteralPath $mono -Filter '*.bmp')
if($monoImages.Count -ne 5){throw 'Incomplete mono reference'}
foreach($image in $monoImages) {
    if((Get-FileHash -LiteralPath $image.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $fault $image.Name)).Hash) {
        throw "Failed right palette leaked a partial eye into mono: $($image.Name)"
    }
}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $hash){throw 'Executable changed'}
$afterHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
if($afterHash -ne $preferencesHash){throw 'Saved preferences changed'}
$results+=@{case='failed-right-palette';exact_mono_images=$monoImages.Count;declined_pairs=32;sha256=$hash}
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output 'PASS: all three MSAA sample counts visibly change the scene; failed right resolve matches mono exactly.'
