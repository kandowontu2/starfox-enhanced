@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0Starfox-Assets.BIN" (
  echo Starfox-Assets.BIN is missing.
  echo Drag your own supported retail ROM onto BUILD-ASSETS.bat first.
  pause
  exit /b 2
)
"%~dp0starfox_pcvr.exe" %* > "%~dp0pcvr-log.txt" 2>&1
set "SFE_PCVR_EXIT_CODE=%ERRORLEVEL%"
>> "%~dp0pcvr-log.txt" echo PCVR exit code: %SFE_PCVR_EXIT_CODE%
if not "%SFE_PCVR_EXIT_CODE%"=="0" (
  echo PCVR exited with error code %SFE_PCVR_EXIT_CODE%. Details:
  type "%~dp0pcvr-log.txt"
  echo.
  echo The full log is saved beside this launcher as pcvr-log.txt.
  pause
  exit /b %SFE_PCVR_EXIT_CODE%
)
exit /b 0
