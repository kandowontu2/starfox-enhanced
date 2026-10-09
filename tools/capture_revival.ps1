param(
    [string]$Executable='build/current/starfox_pc.exe',
    [string]$OutputDirectory='tmp/issue43-baseline',
    [string]$Stage='LEVEL3_2',
    [ValidateSet('ORIGINAL','EX')][string]$Experience='EX',
    [int]$Preroll=500,
    [int]$Frames=1800,
    [ValidateRange(1,1800)][int]$CaptureInterval=60
)
$ErrorActionPreference='Stop'
$proofPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proofPath | Out-Null
$env:SDL_AUDIODRIVER='dummy'
$env:STARFOX_TEST_HIDDEN='1'
$env:STARFOX_TEST_FRAMES="$Frames"
$env:STARFOX_TEST_PREROLL_TICKS="$Preroll"
$env:STARFOX_TEST_EXPERIENCE=$Experience
$env:STARFOX_TEST_TIMING_MODE='ORIGINAL'
$env:STARFOX_TEST_PRESENTATION_FPS='60'
$env:STARFOX_TEST_UNPACED='1'
$env:STARFOX_TEST_VSYNC='0'
$env:STARFOX_TEST_RENDER_SCALE='2'
$env:STARFOX_TEST_REVIVAL='1'
$env:STARFOX_TEST_BLOOM='0'
$env:STARFOX_TEST_ENHANCED_SHADOWS='0'
$env:STARFOX_TEST_RAY_TRACING='0'
$env:STARFOX_CAPTURE_DIR=$proofPath
$env:STARFOX_CAPTURE_INTERVAL="$CaptureInterval"
$rom=if($Experience -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC'} else {'upstream-ultrastarfox/SF.SFC'}
$symbols=if($Experience -eq 'EX') {'assets/symbols/starfox-ex.txt'} else {'upstream-ultrastarfox/SYMBOLS.TXT'}
$proofProcess=Start-Process -FilePath $Executable -ArgumentList "`"$rom`" `"$symbols`" $Stage" -WindowStyle Hidden -PassThru -RedirectStandardError "$proofPath/runtime.log"
$proofProcessHandle=$proofProcess.Handle
Write-Output "Revival capture PID $($proofProcess.Id) -> $proofPath"
if(-not $proofProcess.WaitForExit(60000)) {throw "Capture still running (PID $($proofProcess.Id))"}
if($proofProcess.ExitCode -ne 0) {throw "Capture failed: exit $($proofProcess.ExitCode); see runtime.log"}
Get-Content "$proofPath/runtime.log"
