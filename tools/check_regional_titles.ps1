param([string]$OutputDirectory='tmp/regional-title-composition',
    [ValidateSet('ORIGINAL','EX')][string]$Experience='ORIGINAL')
$ErrorActionPreference='Stop'
$proofPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proofPath | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy'; STARFOX_TEST_HIDDEN='1'; STARFOX_TEST_FRAMES='1'
        STARFOX_TEST_SKIP_PREROLL='1'; STARFOX_TEST_EXPERIENCE=$Experience
        STARFOX_TEST_DISPLAY_MODE='16_9'; STARFOX_TEST_PRESENTATION_FPS='60'
        STARFOX_TEST_UNPACED='1'; STARFOX_TEST_VSYNC='0'; STARFOX_TEST_MSU1='0'
        STARFOX_TEST_RENDERER='GPU'; STARFOX_TEST_RENDER_SCALE='2'
        STARFOX_TEST_PREROLL_TICKS='200'; STARFOX_TEST_RAY_TRACING='0'
        STARFOX_TEST_BLOOM='0'; STARFOX_TEST_BLOOM_2D='0'
        STARFOX_TEST_EFFECT='0'; STARFOX_TEST_WORLD_EFFECT='0'
        STARFOX_TEST_HDR_EFFECT='0'; STARFOX_TEST_CHROMATIC_ABERRATION='0'
        STARFOX_TRACE_RENDER_STATE='1'
    }
    foreach($key in $settings.Keys) {[Environment]::SetEnvironmentVariable($key,$settings[$key],'Process')}
    $arguments=if($Experience -eq 'EX') {
        'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt TITLEMAP'
    } else {'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT TITLEMAP'}
    foreach($language in 0..5) {
        $env:STARFOX_TEST_LANGUAGE="$language"
        $capture=Join-Path $proofPath "language-$language.bmp"
        $env:STARFOX_CAPTURE_PATH=$capture
        $log=Join-Path $proofPath "language-$language.log"
        $process=Start-Process build/current/starfox_pc.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError $log
        $processHandle=$process.Handle
        if(!$process.WaitForExit(30000)) {throw "Title capture still running: PID $($process.Id), $log"}
        if($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath $capture)) {throw "Title capture failed: $log"}
        if(!(Select-String -LiteralPath $log -Pattern 'render-state flow=1 ' -Quiet)) {throw "Capture did not reach title flow: $log"}
        Write-Output "Captured $Experience language $language title: $capture"
    }
    $hashes=@(0..5 | ForEach-Object {(Get-FileHash -LiteralPath (Join-Path $proofPath "language-$_.bmp") -Algorithm SHA256).Hash})
    if($Experience -eq 'EX') {
        if(@($hashes | Sort-Object -Unique).Count -ne 1) {throw 'Original regional logo selection changed the EX title'}
    } else {
        if($hashes[0] -ne $hashes[3] -or $hashes[0] -ne $hashes[4] -or
            $hashes[2] -ne $hashes[5] -or
            @($hashes[0..2] | Sort-Object -Unique).Count -ne 3) {
            throw 'Regional title composition groups do not match US/Japan/Starwing selection'
        }
        # Regression for this fixed 16:9/2x/tick-200 fixture: wrapped logo
        # ink previously leaked into this small outer-margin column.
        Add-Type -AssemblyName System.Drawing
        foreach($language in 0..5) {
            $bitmap=[Drawing.Bitmap]::FromFile((Join-Path $proofPath "language-$language.bmp"))
            try {
                for($y=64;$y -lt 76;++$y) {for($x=758;$x -lt 760;++$x) {
                    $pixel=$bitmap.GetPixel($x,$y)
                    if($pixel.R -or $pixel.G -or $pixel.B) {throw 'Regional logo wrapped into the outer title margin'}
                }}
            } finally {$bitmap.Dispose()}
        }
    }
    Write-Output 'Regional title composition checks passed.'
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys) {[Environment]::SetEnvironmentVariable($key,$saved[$key],'Process')}
}
