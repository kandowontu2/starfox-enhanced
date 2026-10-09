param([ValidateSet('ORIGINAL','EX')][string]$Experience='ORIGINAL',
    [string]$OutputDirectory='tmp/runtime-options-proof',
    [string]$Binary='build/current/starfox_pc.exe',
    [ValidateSet('SOFTWARE','GPU')][string]$Renderer='GPU',
    [ValidateSet('direct3d12','vulkan')][string]$GpuDriver='direct3d12',
    [ValidateRange(0,4)][int]$Dlss=0,[ValidateRange(0,4)][int]$Dlss45=0,
    [switch]$Cheats,[switch]$PresentPacing)
$ErrorActionPreference='Stop'
$proofPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proofPath | Out-Null
$settings=@{
    SDL_AUDIODRIVER='dummy'; STARFOX_TEST_HIDDEN='1'; STARFOX_TEST_FRAMES='120'
    STARFOX_TEST_SKIP_PREROLL='1'; STARFOX_TEST_EXPERIENCE=$Experience
    STARFOX_TEST_DISPLAY_MODE='16_9'; STARFOX_TEST_PRESENTATION_FPS='60'
    STARFOX_TEST_UNPACED='1'; STARFOX_TEST_VSYNC='0'; STARFOX_TEST_MSU1='0'
    STARFOX_TEST_RENDER_SCALE='2'; STARFOX_TEST_PREROLL_TICKS='240'
    STARFOX_TEST_PRESSES=''; STARFOX_TEST_RAY_TRACING='0'
    STARFOX_TEST_STATE_ACTIONS='20:6,60:6,90:6'
    STARFOX_CAPTURE_PATH=(Join-Path $proofPath 'runtime-options.bmp')
    STARFOX_CAPTURE_PRESENTATION_PATH=(Join-Path $proofPath 'presentation.bmp')
    STARFOX_TEST_RENDERER=$Renderer; SDL_GPU_DRIVER=$GpuDriver
}
foreach($name in @('ENHANCED','SEPARATED_MODELS','ANTI_ALIASING','2D_FILTER',
    'RTX_LIGHTING','BLOOM','BLOOM_2D','EFFECT','WORLD_EFFECT','MODEL_SMOOTHING',
    'HDR_EFFECT','CHROMATIC_ABERRATION','SOFTWARE_SHADOWS','REFLECTIVE_SURFACES',
    'DLSS_SELECTION','FSR1_SELECTION','STEREO_OUTPUT','LANGUAGE')) {
    $settings["STARFOX_TEST_$name"]='0'
}
if($Dlss -or $Dlss45) {
    if($Renderer -ne 'GPU' -or $GpuDriver -ne 'direct3d12' -or $Cheats) {
        throw 'DLSS preview transitions require GPU/D3D12 and the ordinary F1 sequence'
    }
    $settings.STARFOX_TEST_DLSS_SELECTION="$Dlss"
    $settings.STARFOX_TEST_DLSS45_SELECTION="$Dlss45"
    $settings.STARFOX_TEST_PREROLL_TICKS='1000'
    $settings.STARFOX_TRACE_GPU='1'
    $settings.STARFOX_TEST_CAMERA_RESPONSE='0'
    $settings.STARFOX_TEST_ADAPTIVE_EXPOSURE='0'
}
$previous=@{}
if($PresentPacing) {
    $settings.STARFOX_TEST_UNPACED=$null
    $settings.STARFOX_TEST_PRESENT_PACING='1'
}
if($Cheats) {
    $settings.STARFOX_TEST_STATE_ACTIONS='10:6'
    # F1 selects OPTIONS; opening it selects CHEATS, independently of its
    # visual row position below 3D OUTPUT. Use distinct confirm presses.
    $settings.STARFOX_TEST_PRESSES='20:0x0080,40:0x0080'
    $settings.STARFOX_CAPTURE_PATH=Join-Path $proofPath 'runtime-cheats.bmp'
}
try {
    # Isolate scripted input/capture from prior diagnostic sessions, then restore.
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
        $previous[$_.Name]=$_.Value
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach($key in $settings.Keys) {
        [Environment]::SetEnvironmentVariable($key,$settings[$key],'Process')
    }
    $arguments=if($Experience -eq 'EX') {
        'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL1_1'
    } else {'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_1'}
    $process=Start-Process -FilePath $Binary -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError "$proofPath/runtime.log"
    $processHandle=$process.Handle
    if(-not $process.WaitForExit(30000)) {throw "Runtime-options check still running (PID $($process.Id)); inspect it before retrying"}
    if($process.ExitCode -ne 0) {throw "Runtime failed; see $proofPath/runtime.log"}
    if(!(Test-Path -LiteralPath $settings.STARFOX_CAPTURE_PRESENTATION_PATH)) {
        throw 'Final presentation capture missing'
    }
    $log=Get-Content "$proofPath/runtime.log" -Raw
    $events=@([regex]::Matches($log,'runtime-options open=(\d) experience=(\d+)'))
    $expectedEvents=if($Cheats){'1'}else{'1,0,1'}
    if((($events | ForEach-Object {$_.Groups[1].Value}) -join ',') -ne $expectedEvents) {
        throw 'F1 did not open, resume, and reopen runtime options'
    }
    $expectedExperience=if($Experience -eq 'EX'){'1'}else{'0'}
    if(@($events | Where-Object {$_.Groups[2].Value -ne $expectedExperience}).Count -ne 0) {
        throw "Runtime menu did not retain requested experience $Experience"
    }
    if($Dlss -or $Dlss45) {
        $frozen=$false;$resumeReset=$false;$previewFrames=0;$beforeMenu=0;$afterMenu=0;$sawMenu=$false
        foreach($line in ($log -split "`n")) {
            if($line -match '^runtime-options open=(\d)') {
                $frozen=$Matches[1] -eq '1';$sawMenu=$true
                if(!$frozen){$resumeReset=$true}
            } elseif($line -match '^dlss-preview:') {
                if(!$frozen -or $line -notmatch 'reused reconstructed frame=') {throw 'Unexpected retained preview outside frozen menu'}
                $previewFrames++
            } elseif($line -match '^dlss-gameplay: evaluated') {
                if($frozen) {$previewFrames++;continue}
                if($resumeReset -and $line -notmatch 'reset=1') {throw 'Resuming DLSS did not reset preview history'}
                $resumeReset=$false
                if($sawMenu){$afterMenu++}else{$beforeMenu++}
            }
        }
        if(!$beforeMenu -or !$afterMenu -or !$previewFrames -or $resumeReset -or
            $log -match 'dlss-gameplay: failed|dlss-fallback:') {
            throw 'DLSS/reconstructed-preview/resumed-DLSS transition incomplete'
        }
        Write-Output "DLSS transition verified: gameplay=$beforeMenu reconstructed-preview=$previewFrames resumed-gameplay=$afterMenu"
    }
    if($Cheats) {Write-Output "F1 and submenu input sequence completed; visually inspect runtime-cheats.bmp: $proofPath"}
    else {Write-Output "Verified real F1 open/resume/reopen events: $proofPath"}
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach($key in $previous.Keys) {[Environment]::SetEnvironmentVariable($key,$previous[$key],'Process')}
}
