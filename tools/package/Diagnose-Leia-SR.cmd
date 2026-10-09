@echo off
cd /d "%~dp0"
starfox_leia_sr_check.exe --probe > Leia-SR-diagnostics.txt 2>&1
type Leia-SR-diagnostics.txt
echo.
echo This checks SDK initialization, not native 3D image quality.
echo Send Leia-SR-diagnostics.txt and startup.log with your report.
pause
