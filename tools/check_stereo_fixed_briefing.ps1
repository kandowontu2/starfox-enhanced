param([string]$Binary='build/current/starfox_pc.exe',
    [string]$OutputDirectory='tmp/stereo-fixed-briefing-check',
    [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='direct3d12')
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a new output directory; existing results will not be overwritten'}
New-Item -ItemType Directory -Path $output | Out-Null
$binaryHash=(Get-FileHash -LiteralPath $Binary).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Binary))) 'pregame.cfg'
$preferencesHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
$results=@()
foreach($experience in 'ORIGINAL','EX') {
    $common=@{Binary=$Binary;GpuBackend=$GpuBackend;Frames=400;CaptureFirst=160;CaptureInterval=80;
        FixedTemporalClock=$true;Stage='PLANETSELECT';PrerollTicks=1000;Presses='1:4096';
        Experience=$experience;Stereo=2;RenderScale=2;GroundEnabled=0;Sky=0;RayTracing=0;Reflections=0;
        CheckFixedLayers=$true}
    $reference=Join-Path $output "$experience-duplicate"
    $candidate=Join-Path $output "$experience-shared"
    & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $reference -DuplicateFixedLayers
    & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $candidate
    $counts=@()
    foreach($slot in 2,3) {
        $repeated=@(Select-String -LiteralPath (Join-Path $reference 'runtime.log') -Pattern "^stereo-fixed-layer: rendered slot=$slot eye=[01]$").Count
        $rendered=@(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -Pattern "^stereo-fixed-layer: rendered slot=$slot eye=0$").Count
        $shared=@(Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -Pattern "^stereo-fixed-layer: shared slot=$slot eye=1$").Count
        if(!$shared -or $rendered -ne $shared -or $repeated -ne 2*$shared -or
            (Select-String -LiteralPath (Join-Path $candidate 'runtime.log') -Pattern "^stereo-fixed-layer: rendered slot=$slot eye=1$" -Quiet) -or
            (Select-String -LiteralPath (Join-Path $reference 'runtime.log') -Pattern '^stereo-fixed-layer: shared ' -Quiet)) {
            throw "$experience briefing slot $slot did not halve eligible producer submissions"
        }
        $counts+=@{slot=$slot;reference_submissions=$repeated;candidate_submissions=$rendered;shared_right_eye_sources=$shared}
    }
    foreach($directory in @($reference,$candidate)) {
        $log=Join-Path $directory 'runtime.log'
        if(@(Select-String -LiteralPath $log -Pattern '^stereo presented:').Count -ne 400 -or
            !(Select-String -LiteralPath $log -SimpleMatch 'native-pipeline: GPU resident isolated sources' -Quiet) -or
            (Select-String -LiteralPath $log -Pattern 'VK_ERROR_|device lost|GPU effects fallback:|stereo failure:|replaying complete frame' -Quiet)) {
            throw "Briefing did not finish complete resident GPU pairs: $directory"
        }
    }
    $images=@(Get-ChildItem -LiteralPath $reference -Filter '*.bmp')
    if($images.Count -ne 5){throw "Unexpected briefing capture coverage: $experience"}
    foreach($image in $images) {
        $actual=Join-Path $candidate $image.Name
        if(!(Test-Path -LiteralPath $actual) -or (Get-FileHash -LiteralPath $actual).Hash -ne (Get-FileHash -LiteralPath $image.FullName).Hash) {
            throw "Briefing shared-layer parity failed: $experience/$($image.Name)"
        }
    }
    $results+=@{experience=$experience;backend=$GpuBackend;images=$images.Count;exact=$true;sha256=$binaryHash;isolated_producers=$counts}
    Write-Output "PASS $experience briefing: $($images.Count) exact packed images; both isolated producers shared"
}
$afterHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
if($afterHash -ne $preferencesHash){throw 'Saved preferences changed during briefing tests'}
if((Get-FileHash -LiteralPath $Binary).Hash -ne $binaryHash){throw 'Executable changed during briefing tests'}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'results.json')
