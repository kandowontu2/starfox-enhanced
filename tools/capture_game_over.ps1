param(
    [string]$OutputDirectory='tmp/game-over-proof',
    [string]$Experience='ORIGINAL',
    [string]$DisplayMode='16_9',
    [int]$Frames=90
)
$ErrorActionPreference='Stop'
$proofPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proofPath | Out-Null
$env:SDL_AUDIODRIVER='dummy'
$env:STARFOX_TEST_HIDDEN='1'
$env:STARFOX_TEST_FRAMES="$Frames"
$env:STARFOX_TEST_EXPERIENCE=$Experience
$env:STARFOX_TEST_DISPLAY_MODE=$DisplayMode
$env:STARFOX_TEST_PRESENTATION_FPS='60'
$env:STARFOX_TEST_UNPACED='1'
$env:STARFOX_TEST_VSYNC='0'
$env:STARFOX_TEST_RENDER_SCALE='2'
$env:STARFOX_CAPTURE_PATH=Join-Path $proofPath 'gameover.bmp'
Remove-Item Env:STARFOX_TEST_PRESSES,Env:STARFOX_TEST_PREROLL_TICKS -ErrorAction SilentlyContinue
$arguments=if($Experience -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt GAMEOVER'} else {'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT GAMEOVER'}
$proofProcess=Start-Process -FilePath build/current/starfox_pc.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError "$proofPath/runtime.log"
if(-not $proofProcess.WaitForExit(60000)) {throw "Capture running (PID $($proofProcess.Id))"}
if($proofProcess.ExitCode -ne 0) {throw "Capture failed: $($proofProcess.ExitCode); see runtime.log"}
Write-Output "Captured Game Over in $proofPath"
