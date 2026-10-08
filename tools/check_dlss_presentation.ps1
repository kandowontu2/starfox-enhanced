param([string]$Binary='build/current/starfox_pc.exe',
    [string]$OutputDirectory='tmp/dlss-explicit-presentation',
    [ValidateRange(33,120)][int]$Frames=40)
$ErrorActionPreference='Stop'
$exe=(Resolve-Path -LiteralPath $Binary).Path
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a new directory; existing evidence will not be overwritten'}
New-Item -ItemType Directory -Path $output | Out-Null
$sha=(Get-FileHash -LiteralPath $exe).Hash
$results=@()
$capture=Join-Path $PSScriptRoot 'capture_lava.ps1'
function Assert-Explicit([string]$Log,[int]$Expected) {
    $text=Get-Content -LiteralPath $Log -Raw
    $summary=@([regex]::Matches($text,'dlss-presentation-lifecycle: completed=(\d+) attempts=(\d+)'))
    if($summary.Count -ne 1 -or [int]$summary[0].Groups[1].Value -ne $Expected -or
        [int]$summary[0].Groups[2].Value -ne $Expected -or
        $text -notmatch 'dlss-presentation: explicit frame-end; native swapchain retained' -or
        $text -match 'dlss-presentation: upgraded|dlss-presentation: restored|dlss-sdk-(error|warning)|frame-end failed') {
        throw "Missing/failed/duplicate SDK frame-end or unexpected DXGI wrapping: $Log"
    }
    return $text
}
foreach($experience in 'ORIGINAL','EX') {foreach($model in 'standard','4.5') {
    # Both a moving scene and a frozen 32-sample/held preview, with each model.
    $preview=$experience -eq 'EX'
    $name="$experience-$model-$(if($preview){'preview'}else{'game'})"
    $selection=if($model -eq 'standard'){@{Dlss=1}}else{@{Dlss45=4}}
    $common=@{Binary=$exe;Experience=$experience;Stage='LEVEL1_1';Frames=$Frames;PrerollTicks=1000;
        GroundEnabled=0;Sky=1;RayTracing=0;Reflections=0;RenderScale=2;GpuBackend='direct3d12';
        CaptureFirst=8;CaptureInterval=8;MenuPreview=$preview;FixedTemporalClock=$true}
    $legacy=Join-Path $output "$name-proxy"
    $direct=Join-Path $output "$name-explicit"
    & $capture @common @selection -OutputDirectory $legacy -DlssProxyPresentation
    & $capture @common @selection -OutputDirectory $direct
    $reference=Get-Content -LiteralPath (Join-Path $legacy 'runtime.log') -Raw
    if($reference -notmatch 'dlss-presentation: upgraded' -or $reference -notmatch 'dlss-presentation: restored' -or
        $reference -match 'dlss-presentation: explicit frame-end|dlss-presentation-lifecycle:|dlss-gameplay: failed|dlss-sdk-error:') {
        throw "Legacy proxy control did not actually execute clean reconstruction: $name"
    }
    $candidate=Assert-Explicit (Join-Path $direct 'runtime.log') $Frames
    $evaluations=[regex]::Matches($candidate,'^dlss-gameplay: evaluated', 'Multiline').Count
    $held=[regex]::Matches($candidate,'^dlss-preview: reused reconstructed frame=', 'Multiline').Count
    $expectedEvaluations=if($preview){32}else{$Frames}
    if($evaluations -ne $expectedEvaluations -or $held -ne ($Frames-$expectedEvaluations) -or
        $candidate -match 'dlss-gameplay: failed|dlss-fallback: unjittered scene replay') {throw "Real evaluation/held counts differ: $name"}
    $images=@(Get-ChildItem -LiteralPath $legacy -Filter '*.bmp')
    if($images.Count -lt 5){throw "Insufficient captured proxy images: $name"}
    foreach($image in $images) {
        $other=Join-Path $direct $image.Name
        if(!(Test-Path -LiteralPath $other) -or (Get-FileHash -LiteralPath $image.FullName).Hash -ne
            (Get-FileHash -LiteralPath $other).Hash) {throw "Explicit/proxy pixels differ: $name/$($image.Name)"}
    }
    $results+=@{case=$name;exact_images=$images.Count;evaluations=$evaluations;held=$held;frame_ends=$Frames}
    Write-Output "PASS $name : $($images.Count) exact proxy/explicit images; $Frames once-only SDK frame ends"
}}
foreach($experience in 'ORIGINAL','EX') {
    $common=@{Binary=$exe;Experience=$experience;Stage='LEVEL1_1';Frames=$Frames;PrerollTicks=1000;
        GroundEnabled=0;Sky=0;RayTracing=0;Reflections=0;RenderScale=2;GpuBackend='direct3d12';
        CaptureFirst=1;CaptureInterval=8;FixedTemporalClock=$true}
    $mono=Join-Path $output "$experience-cancel-mono"
    & $capture @common -OutputDirectory $mono
    foreach($model in 'standard','4.5') {
        $selection=if($model -eq 'standard'){@{Dlss=1}}else{@{Dlss45=4}}
        $name="$experience-$model-cancelled-command"
        $directory=Join-Path $output $name
        & $capture @common @selection -OutputDirectory $directory -CancelDlssEvaluationOnce
        $text=Assert-Explicit (Join-Path $directory 'runtime.log') $Frames
        if([regex]::Matches($text,'^dlss-test: cancelling evaluated command','Multiline').Count -ne 1 -or
            [regex]::Matches($text,'^dlss-gameplay: failed: Injected cancellation after SDK evaluation','Multiline').Count -ne 1 -or
            [regex]::Matches($text,'^dlss-fallback: unjittered scene replay','Multiline').Count -ne 1 -or
            [regex]::Matches($text,'^dlss-gameplay: evaluated','Multiline').Count -ne ($Frames-1) -or
            $text -notmatch 'dlss-gameplay: evaluated frame=1 reset=1' -or
            $text -notmatch ('dlss-gameplay: evaluated frame='+($Frames-1)+' reset=0') -or
            [regex]::Matches($text,'^dlss-gameplay: failed:', 'Multiline').Count -ne 1) {
            throw "Cancelled real SDK command did not drain and resume with reset history: $name"
        }
        $recovered=@([regex]::Matches($text,'^dlss-gameplay: evaluated frame=(\d+) reset=(\d+)', 'Multiline'))
        for($i=0;$i -lt $recovered.Count;++$i) {
            if([int]$recovered[$i].Groups[1].Value -ne ($i+1) -or
                [int]$recovered[$i].Groups[2].Value -ne $(if($i -eq 0){1}else{0})) {
                throw "Recovered SDK tokens repeated/skipped or history reset after recovery: $name"
            }
        }
        $reference=Join-Path $mono 'lava.bmp.frame-1.bmp'
        $fallback=Join-Path $directory 'lava.bmp.frame-1.bmp'
        if((Get-FileHash -LiteralPath $reference).Hash -ne (Get-FileHash -LiteralPath $fallback).Hash) {
            throw "Cancelled SDK frame differs from unjittered mono fallback: $name"
        }
        $results+=@{case=$name;cancelled=1;recovered_evaluations=$Frames-1;frame_ends=$Frames;exact_mono_fallback=$true}
        Write-Output "PASS $name : real evaluated command cancelled; exact mono fallback; SDK/history recovered"
    }
}
if((Get-FileHash -LiteralPath $exe).Hash -ne $sha){throw 'Executable changed during presentation validation'}
@{executable=$exe;sha256=$sha;cases=$results} | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $output 'results.json')
