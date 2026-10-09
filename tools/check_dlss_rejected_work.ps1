param([Parameter(Mandatory)][string]$Binary,
      [Parameter(Mandatory)][string]$OutputDirectory,
      [ValidateRange(8,64)][int]$Frames=12)
$ErrorActionPreference='Stop'
$exe=(Resolve-Path -LiteralPath $Binary).Path
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Use a fresh evidence directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$sha=(Get-FileHash -LiteralPath $exe).Hash
$records=@()
foreach($experience in 'ORIGINAL','EX') {
    $common=@{Binary=$exe;Experience=$experience;Stage='LEVEL1_1';Frames=$Frames;PrerollTicks=1000;
        GroundEnabled=0;Sky=0;RayTracing=0;Reflections=0;RenderScale=2;GpuBackend='direct3d12';
        CaptureFirst=1;CaptureInterval=1;HideFps=$true;FixedTemporalClock=$true}
    $baseline=Join-Path $output "$experience-native"
    & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory $baseline
    foreach($model in 'standard','4.5') {
        $selection=if($model -eq 'standard'){@{Dlss=1}}else{@{Dlss45=4}}
        $name="$experience-$model-rejected-work"
        $directory=Join-Path $output $name
        & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common @selection -OutputDirectory $directory `
            -CancelDlssEvaluationOnce -GpuValidation -DlssValidation
        $text=Get-Content -LiteralPath (Join-Path $directory 'runtime.log') -Raw
        if([regex]::Matches($text,'(?m)^dlss-test: cancelling evaluated command\r?$').Count -ne 1 -or
           [regex]::Matches($text,'(?m)^dlss-gameplay: failed: Injected cancellation after SDK evaluation\r?$').Count -ne 1 -or
           [regex]::Matches($text,'(?m)^dlss-fallback: unjittered scene replay\r?$').Count -ne 1) {
            throw "Real SDK rejection/fallback was not exercised exactly once: $name"
        }
        if([regex]::Matches($text,'(?m)^dlss-gameplay: rejected SDK work drained\r?$').Count -ne 1 -or
            $text -match 'rejected SDK drain failed') {throw "Recorded SDK work was not drained: $name"}
        $evaluations=[regex]::Matches($text,'(?m)^dlss-gameplay: evaluated frame=(\d+) reset=(\d+)')
        if($evaluations.Count -ne $Frames-1){throw "Incomplete recovered SDK frames: $name"}
        for($i=0;$i -lt $evaluations.Count;++$i) {
            if([int]$evaluations[$i].Groups[1].Value -ne $i+1 -or
               [int]$evaluations[$i].Groups[2].Value -ne $(if($i -eq 0){1}else{0})) {
                throw "SDK tokens/history failed to recover: $name"
            }
        }
        if($text -notmatch "dlss-presentation-lifecycle: completed=$Frames attempts=$Frames") {
            throw "SDK frame-end lifecycle mismatch: $name"
        }
        $native=Join-Path $baseline 'lava.bmp.frame-1.bmp'
        $rejected=Join-Path $directory 'lava.bmp.frame-1.bmp'
        if((Get-FileHash -LiteralPath $native).Hash -ne (Get-FileHash -LiteralPath $rejected).Hash) {
            throw "Rejected SDK frame differs from real unjittered native image: $name"
        }
        $records+=@{case=$name;recovered=$evaluations.Count;frame_ends=$Frames;native_fallback_exact=$true;
            critical=0;discarded=0;validation_stages=@('sdk-teardown','renderer-teardown')}
        Write-Output "PASS $name : native fallback exact, SDK recovery, zero critical/discarded messages through renderer teardown"
    }
}
if((Get-FileHash -LiteralPath $exe).Hash -ne $sha){throw 'Player changed during validation'}
@{player=$exe;sha256=$sha;cases=$records;
  scope='Actual normal PC mono player, real K/M SDK rejection after encoding, native fallback pixels and recovered GPU/resource states. Not neural quality, physical stereo, gameplay FPS or platform acceptance.'} |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'results.json')
