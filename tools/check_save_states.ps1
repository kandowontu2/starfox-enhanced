param([string]$Experience='ORIGINAL', [string]$OutputDirectory='tmp/save-state-proof', [switch]$LoadOnly)
$ErrorActionPreference='Stop'
$proofPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proofPath | Out-Null
$env:SDL_AUDIODRIVER='dummy'
$env:STARFOX_TEST_HIDDEN='1'
$env:STARFOX_TEST_FRAMES='120'
$env:STARFOX_TEST_SKIP_PREROLL='1'
$env:STARFOX_TEST_EXPERIENCE=$Experience
$env:STARFOX_TEST_DISPLAY_MODE='16_9'
$env:STARFOX_TEST_PRESENTATION_FPS='60'
$env:STARFOX_TEST_UNPACED='1'
$env:STARFOX_TEST_VSYNC='0'
$env:STARFOX_TEST_MSU1='0'
$env:STARFOX_TEST_RENDER_SCALE='2'
$env:STARFOX_TEST_PREROLL_TICKS='240'
$env:STARFOX_TEST_STATE_DIRECTORY=Join-Path $proofPath 'slots'
# Real SDL key events: save, advance, load, open slot selector, select next.
$env:STARFOX_TEST_STATE_ACTIONS=if($LoadOnly) {'20:2,30:3,31:4'} else {'20:1,80:2,90:3,91:4'}
$env:STARFOX_CAPTURE_PATH=Join-Path $proofPath 'slot-selector.bmp'
Remove-Item Env:STARFOX_TEST_PRESSES -ErrorAction SilentlyContinue
$arguments=if($Experience -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL1_1'} else {'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_1'}
$process=Start-Process -FilePath build/current/starfox_pc.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError "$proofPath/runtime.log"
$processHandle=$process.Handle
if(-not $process.WaitForExit(30000)) {throw "Save-state verification still running (PID $($process.Id))"}
if($process.ExitCode -ne 0) {throw "Runtime failed; see $proofPath/runtime.log"}
$log=Get-Content "$proofPath/runtime.log" -Raw
if((!$LoadOnly -and $log -notmatch 'state saved slot=0') -or $log -notmatch 'state loaded slot=0' -or $log -match 'state operation failed') {throw "Save/load did not complete; see runtime.log"}
Write-Output "Verified save/load and captured selector: $proofPath"
