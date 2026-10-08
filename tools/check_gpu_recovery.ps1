param(
    [Parameter(Mandatory)][string]$Binary,
    [Parameter(Mandatory)][string]$Output,
    [ValidateSet('direct3d12','vulkan')][string]$Driver='direct3d12'
)
$ErrorActionPreference='Stop'
$binaryPath=(Resolve-Path -LiteralPath $Binary).Path
$binaryHash=(Get-FileHash -LiteralPath $binaryPath).Hash
if(Test-Path -LiteralPath $Output) {throw 'Use a new output directory; recovery checks never overwrite existing data.'}
$outputPath=(New-Item -ItemType Directory -Path $Output).FullName
$fixture=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../tests/fixtures/gpu-recovery.cfg')).Path
$testBinary=Join-Path $outputPath 'starfox_pc.exe'
# Reuse one executable across all cases. On the same volume, this allocates
# no second 260+ MB embedded-asset image. No original preferences are touched.
try {New-Item -ItemType HardLink -Path $testBinary -Target $binaryPath | Out-Null}
catch {Copy-Item -LiteralPath $binaryPath -Destination $testBinary}
$config=Join-Path $outputPath 'pregame.cfg'
$marker=Join-Path $outputPath 'gpu-session-pending'
$asset=Join-Path $outputPath 'keep-user-asset.bin'
Copy-Item -LiteralPath $fixture -Destination $asset
$assetHash=(Get-FileHash -LiteralPath $asset).Hash
$environment=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {
    $environment[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
$process=$null
"GPU recovery executable SHA-256: $binaryHash"
function Assert-Asset {
    if(!(Test-Path -LiteralPath $asset) -or (Get-FileHash -LiteralPath $asset).Hash -ne $assetHash) {
        throw 'GPU recovery changed a user-asset sentinel'
    }
}
function Reset-Config {
    Copy-Item -LiteralPath $fixture -Destination $config
}
function Invoke-Check([string]$Name,[hashtable]$Options,[string[]]$Arguments=@(),[switch]$Interrupt) {
    $log=Join-Path $outputPath "$Name.log"
    $settings=@{
        SDL_AUDIODRIVER='dummy';SDL_GPU_DRIVER=$Driver;DISABLE_VK_LAYER_reshade_1='1';
        STARFOX_TEST_FRAMES=$(if($Interrupt){'10000000'}else{'40'});
        STARFOX_TEST_HIDDEN='1';STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_UNPACED='1';
        STARFOX_TEST_VSYNC='0';STARFOX_TEST_DLSS_SELECTION='0';STARFOX_TEST_DLSS45_SELECTION='0';
        STARFOX_TEST_FSR1_SELECTION='0';STARFOX_TEST_RENDER_SCALE='1';STARFOX_TEST_GPU_GUARD='1';
        STARFOX_TEST_SHOW_FPS='0';STARFOX_TRACE_GPU='1'
    }
    foreach($entry in $Options.GetEnumerator()) {$settings[$entry.Key]=$entry.Value}
    foreach($entry in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
    try {
        $start=@{FilePath=$testBinary;WorkingDirectory=$outputPath;WindowStyle='Hidden';PassThru=$true;RedirectStandardError=$log}
        if($Arguments.Count) {$start.ArgumentList=$Arguments}
        $script:process=Start-Process @start
        $handle=$script:process.Handle
        if($Interrupt) {
            $deadline=[DateTime]::UtcNow.AddSeconds(45)
            do {
                $script:process.Refresh()
                if($script:process.HasExited) {throw "Interrupted-session process exited prematurely: $Name"}
                $text=Get-Content -LiteralPath $log -Raw -ErrorAction SilentlyContinue
                if($text -match 'native-pipeline:' -and (Test-Path -LiteralPath $marker -PathType Leaf)) {break}
                if([DateTime]::UtcNow -ge $deadline) {throw "No live GPU gameplay checkpoint: $Name"}
                Start-Sleep -Milliseconds 100
            } while($true)
            # Only stop the process this check created, after a live GPU frame.
            Stop-Process -Id $script:process.Id
            $script:process.WaitForExit()
            if(!(Test-Path -LiteralPath $marker -PathType Leaf)) {throw 'Forced close erased the GPU recovery marker'}
        } else {
            if(!$script:process.WaitForExit(45000)) {throw "GPU recovery check timed out: $Name"}
            if($script:process.ExitCode -ne 0) {throw "GPU recovery check failed: $Name ($($script:process.ExitCode))"}
        }
        Assert-Asset
        return Get-Content -LiteralPath $log -Raw
    } finally {
        foreach($entry in $settings.GetEnumerator()) {Remove-Item -LiteralPath "Env:$($entry.Key)" -ErrorAction SilentlyContinue}
        if($script:process) {
            $script:process.Refresh()
            if(!$script:process.HasExited) {Stop-Process -Id $script:process.Id; $script:process.WaitForExit()}
            $script:process.Dispose();$script:process=$null
        }
    }
}
function Assert-Fallback([string]$Text,[string]$Name,[int]$Count=1) {
    if([regex]::Matches($Text,'renderer-recovery: Software selected').Count -ne $Count) {
        throw "Unexpected repeated/missing fallback: $Name"
    }
    if($Text -notmatch 'renderer-picker:.*mode=SOFTWARE actual=software') {throw "Wrong effective renderer: $Name"}
    if($Text -match 'native-pipeline:') {throw "Failed GPU still rendered a native GPU frame: $Name"}
}
try {
    foreach($experience in @('ORIGINAL','EX')) {
        Reset-Config
        $options=@{SDL_GPU_DRIVER='sfe-no-such-driver';STARFOX_TEST_RENDERER='GPU';STARFOX_TEST_EXPERIENCE=$experience}
        $text=Invoke-Check "bad-driver-$experience" $options @('LEVEL1_1')
        Assert-Fallback $text "bad-driver-$experience"
        if(Test-Path -LiteralPath $marker) {throw 'Successful Software fallback retained its GPU marker'}
    }
    Reset-Config
    New-Item -ItemType Directory -Path $marker | Out-Null
    $text=Invoke-Check 'blocked-journal' @{STARFOX_TEST_RENDERER='GPU'} @('LEVEL1_1')
    Assert-Fallback $text 'blocked-journal'
    if($text -notmatch 'GPU recovery journal unavailable' -or !(Test-Path -LiteralPath $marker -PathType Container)) {
        throw 'Unwritable journal did not fail closed while preserving its directory'
    }
    Remove-Item -LiteralPath $marker # Exact, empty directory created by this check.
    Reset-Config
    $text=Invoke-Check 'gpu-clean-close' @{STARFOX_TEST_RENDERER='GPU'} @('LEVEL1_1')
    if($text -notmatch 'native-pipeline:' -or $text -match 'renderer-recovery:') {throw 'GPU retry after storage recovery failed'}
    if(Test-Path -LiteralPath $marker) {throw 'Completed GPU teardown did not clear the marker'}
    Reset-Config
    $null=Invoke-Check 'gpu-forced-close' @{STARFOX_TEST_RENDERER='GPU'} @('LEVEL1_1') -Interrupt
    $text=Invoke-Check 'software-after-interruption' @{} @('LEVEL1_1')
    if($text -notmatch 'renderer-picker:.*mode=SOFTWARE actual=software' -or $text -match 'native-pipeline:') {
        throw 'Interrupted GPU session did not boot into Software without a reinstall'
    }
    if((Get-Content -LiteralPath $config -Raw) -notmatch '(?m)^RENDERER_MODE 1\s*$') {
        throw 'Recovered Software choice was not persisted'
    }
    if((Get-Content -LiteralPath $config -Raw) -notmatch '(?m)^MUSIC_VOLUME 71\s*$' -or
       (Get-Content -LiteralPath $config -Raw) -notmatch '(?m)^SFX_VOLUME 83\s*$') {
        throw 'GPU recovery changed unrelated settings'
    }
    if(Test-Path -LiteralPath $marker) {throw 'Successful recovery retained its marker'}
    Reset-Config
    $text=Invoke-Check 'live-gpu-failure-cycle' @{
        SDL_GPU_DRIVER='sfe-no-such-driver';STARFOX_TEST_RENDERER='SOFTWARE';STARFOX_TEST_RENDERER_CYCLE='1'
    } @('LEVEL1_1')
    # Four explicit GPU opt-in requests at 8-frame intervals, not 40 retries.
    Assert-Fallback $text 'live-gpu-failure-cycle' 4
    if(Test-Path -LiteralPath $marker) {throw 'Live Software fallback retained its marker'}
    Assert-Asset
    if((Get-FileHash -LiteralPath $binaryPath).Hash -ne $binaryHash) {throw 'Executable changed during recovery checks'}
    "PASS GPU recovery ($Driver): 7 real process cases; assets and unrelated settings preserved; no per-frame retry loop"
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach($entry in $environment.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
