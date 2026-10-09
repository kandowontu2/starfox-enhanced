param([string]$OutputDirectory='tmp/results-proof', [string]$Experience='ORIGINAL')
$ErrorActionPreference='Stop'
$proofPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proofPath | Out-Null
$env:SDL_AUDIODRIVER='dummy'
$env:STARFOX_TEST_HIDDEN='1'
$env:STARFOX_TEST_SKIP_PREROLL='1'
$env:STARFOX_TEST_FRAMES='3600'
$env:STARFOX_CAPTURE_RESULTS='1'
$env:STARFOX_TEST_UNPACED='1'
$env:STARFOX_TEST_EXPERIENCE=$Experience
$env:STARFOX_TEST_PRESENTATION_FPS='60'
$env:STARFOX_TEST_DISPLAY_MODE='16_9'
$env:STARFOX_TEST_RENDER_SCALE='2'
$env:STARFOX_TEST_MSU1='0'
$env:STARFOX_TEST_VSYNC='0'
$env:STARFOX_TEST_TIMING_MODE='ORIGINAL'
$env:STARFOX_TEST_CLEAR='CL_WARP'
$env:STARFOX_TEST_PREROLL_TICKS='200'
$env:STARFOX_CAPTURE_PATH=Join-Path $proofPath 'results.bmp'
$arguments=if($Experience -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL1_2'} else {'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_2'}
$process=Start-Process -FilePath build/current/starfox_pc.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError "$proofPath/runtime.log"
# Retain the native process handle before waiting. Windows PowerShell can lose
# ExitCode when its lazily opened Process handle is first queried after exit.
$captureProcessHandle=$process.Handle
if(-not $process.WaitForExit(30000)) {throw "Capture still running: PID $($process.Id)"}
if($process.ExitCode -ne 0) {throw "Capture failed; see runtime.log"}
Write-Output "Captured results: $proofPath"
