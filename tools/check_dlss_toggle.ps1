param([string]$Executable='build/current/starfox_pc.exe',
    [string]$OutputDirectory='tmp/dlss-toggle-check', [switch]$ModelToggle,
    [switch]$MenuPreview,[switch]$ProxyPresentation)
$ErrorActionPreference='Stop'
$proof=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proof | Out-Null
$exe=[IO.Path]::GetFullPath($Executable)
$executableHash=(Get-FileHash -LiteralPath $exe).Hash
Write-Output "DLSS executable: $exe SHA-256=$executableHash"
$preferences=Join-Path ([IO.Path]::GetDirectoryName($exe)) 'pregame.cfg'
$preferencesHash=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
# Runtime discovery can use the verified EXE resources or a loose package.
# Assert actual initialization/evaluation below rather than requiring a folder.
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES='24'
        STARFOX_TEST_REQUIRE_CLEAN_RUNTIME='1'
        DISABLE_VK_LAYER_reshade_1='1' # Installed Vulkan manifest's test-only opt-out.
        STARFOX_TEST_RENDERER='GPU';STARFOX_TEST_EXPERIENCE='ORIGINAL'
        STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_UNPACED='1'
        STARFOX_TEST_RENDER_SCALE='1';STARFOX_TEST_VSYNC='0'
        STARFOX_TEST_STEREO_OUTPUT='0';STARFOX_TEST_FSR1_SELECTION='0'
        STARFOX_TEST_DLSS_SELECTION='0';STARFOX_TEST_DLSS45_SELECTION='0';STARFOX_TEST_DLSS_TOGGLE='1'
        STARFOX_TEST_CAMERA_RESPONSE='0'
        STARFOX_TEST_RAY_TRACING='0';STARFOX_TEST_REFLECTIVE_SURFACES='0'
    }
    if($ModelToggle) {
        $settings.Remove('STARFOX_TEST_DLSS_TOGGLE')
        $settings.STARFOX_TEST_DLSS_SELECTION='1'
        $settings.STARFOX_TEST_DLSS_MODEL_TOGGLE='1'
        $settings.STARFOX_TEST_PREROLL_TICKS='1000'
    }
    if($ProxyPresentation){$settings.STARFOX_TEST_DLSS_PRESENT_PROXY='1'}
    if($MenuPreview) {
        $settings.STARFOX_TEST_FRAMES='120'
        $settings.STARFOX_TEST_DLSS_TOGGLE_PERIOD='40'
        $settings.STARFOX_TEST_MENU_PREVIEW='1'
        $settings.STARFOX_TEST_PREROLL_TICKS='1000'
        $settings.STARFOX_TEST_DISPLAY_MODE='32_9'
        $settings.STARFOX_TEST_ENVIRONMENT_0='0'
        $settings.STARFOX_TEST_ENVIRONMENT_3='1'
        $settings.STARFOX_TEST_BLOOM='0'
        $settings.STARFOX_TEST_BLOOM_2D='0'
        $settings.STARFOX_TEST_ADAPTIVE_EXPOSURE='0'
        $settings.STARFOX_TEST_MATERIAL='0'
        $settings.STARFOX_TEST_EFFECT='0'
        $settings.STARFOX_TEST_WORLD_EFFECT='0'
    }
    foreach($key in $settings.Keys){Set-Item -LiteralPath "Env:$key" -Value $settings[$key]}
    $log=Join-Path $proof 'toggle.log'
    $run=Start-Process $exe -WindowStyle Hidden -PassThru `
        -ArgumentList 'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_1' `
        -RedirectStandardError $log
    $handle=$run.Handle
    while(!$run.WaitForExit(30000)) {
        Write-Output "DLSS toggle check still running: PID $($run.Id), $log"
    }
    if($run.ExitCode -ne 0){throw "DLSS toggle check failed ($($run.ExitCode)): $log"}
    if((Get-FileHash -LiteralPath $exe).Hash -ne $executableHash){throw 'Executable changed during DLSS check'}
    $lines=Get-Content -LiteralPath $log
    if($lines -notcontains 'test-graphics-runtime: no-legacy-injector') {
        throw 'DLSS check did not validate the loaded graphics runtime'
    }
    $text=$lines -join "`n"
    $summaries=@([regex]::Matches($text,'dlss-presentation-lifecycle: completed=(\d+) attempts=(\d+)'))
    $completed=0
    foreach($summary in $summaries) {
        if($summary.Groups[1].Value -ne $summary.Groups[2].Value){throw "DLSS frame-end retried/failed: $log"}
        $completed+=[int]$summary.Groups[1].Value
    }
    $evaluatedFrames=@($lines | Where-Object {$_ -match '^dlss-gameplay: evaluated'}).Count
    $heldFrames=@($lines | Where-Object {$_ -match '^dlss-preview: reused reconstructed frame='}).Count
    if(!$ProxyPresentation -and ($completed -ne ($evaluatedFrames+$heldFrames) -or
        $text -notmatch 'dlss-presentation: explicit frame-end; native swapchain retained' -or
        $text -match 'dlss-presentation: upgraded|dlss-presentation: restored|dlss-sdk-(error|warning)|dlss-presentation: frame-end failed')) {
        throw "Explicit DLSS frame-end/native-swapchain lifecycle incomplete: $log"
    }
    if($ProxyPresentation -and $summaries.Count){throw 'Proxy comparison unexpectedly used explicit frame-end'}
    if($ModelToggle) {
        $text=$lines -join "`n"
        $presets=@($lines | Where-Object {$_ -match '^dlss-model:'})
        $evaluated=@($lines | Where-Object {$_ -match '^dlss-gameplay: evaluated'})
        $samples=if($MenuPreview){32}else{8}
        $reused=@($lines | Where-Object {$_ -match '^dlss-preview: reused reconstructed frame='})
        if($presets.Count -ne 3 -or $presets[0] -notmatch 'preset K' -or
            $presets[1] -notmatch 'preset M' -or $presets[2] -notmatch 'preset K' -or
            $evaluated.Count -ne (3*$samples) -or
            ($MenuPreview -and $reused.Count -ne 24) -or
            $evaluated[0] -notmatch 'reset=1' -or $evaluated[$samples] -notmatch 'reset=1' -or
            $evaluated[2*$samples] -notmatch 'reset=1' -or
            $evaluated[$samples-1] -notmatch 'reset=0' -or $evaluated[2*$samples-1] -notmatch 'reset=0' -or
            $evaluated[3*$samples-1] -notmatch 'reset=0' -or
            $text -match 'dlss-gameplay: failed|dlss-prepare:' -or
            $text -match 'dlss-fallback: unjittered scene replay') {
            throw "Standard/4.5/standard model switch did not reconfigure and reset cleanly: $log"
        }
        "DLSS K/M/K: each model change resets history; evaluated=$($evaluated.Count) retained-preview=$($reused.Count)."
        return
    }
    $upgraded=@($lines | Where-Object {$_ -match '^dlss-presentation: upgraded$'}).Count
    $restored=@($lines | Where-Object {$_ -match '^dlss-presentation: restored$'}).Count
    $restarted=@($lines | Where-Object {$_ -match '^dlss-lifecycle: restarted before renderer creation$'}).Count
    $evaluated=@($lines | Where-Object {$_ -match '^dlss-gameplay: evaluated'}).Count
    if($MenuPreview) {
        $retained=@($lines | Where-Object {$_ -match '^dlss-preview: reused reconstructed frame='})
        # OFF->ON recreates the renderer. SDL can report the initial logical
        # window size for one frame before its drawable size settles; this
        # must reset accumulation, not reuse the earlier-size output.
        if($evaluated -lt 32 -or $retained.Count -lt 1 -or ($evaluated+$retained.Count) -ne 40 -or
            @($retained | Where-Object {$_ -notmatch 'reset=0.*frozen=1 samples=32'}).Count) {
            throw 'OFF/ON/OFF preview did not reconstruct and retain the actual ON scene'
        }
    }
    $failures=@($lines | Where-Object {$_ -match '^dlss-(gameplay|lifecycle|presentation): .*failed'}).Count
    $last_off=(Get-Content -LiteralPath $log -Raw) -split 'dlss-lifecycle: restarted before renderer creation' | Select-Object -Last 1
    if(($ProxyPresentation -and ($upgraded -lt 1 -or $restored -lt 1)) -or $restarted -lt 2 -or $evaluated -lt 1 -or $failures -gt 0){
        throw "OFF/ON/OFF lifecycle incomplete: upgraded=$upgraded restored=$restored restarted=$restarted evaluated=$evaluated; $log"
    }
    if($last_off -match 'dlss-presentation: upgraded' -or $last_off -notmatch 'current renderer is not native D3D12') {
        throw "Final OFF renderer did not return to native Vulkan presentation: $log"
    }
    "DLSS OFF/ON/OFF: upgraded=$upgraded restored=$restored restarted=$restarted evaluated=$evaluated explicit-frame-ends=$completed"
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach($key in $saved.Keys){[Environment]::SetEnvironmentVariable($key,$saved[$key],'Process')}
    $preferencesAfter=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
    if($preferencesAfter -ne $preferencesHash){throw 'Saved preferences changed during DLSS diagnostics'}
}
