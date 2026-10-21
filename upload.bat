@echo off
setlocal
cd /d "%~dp0"
if not exist web (
  call npm run build -- --bootstrap
) else (
  call npm run build
)
if errorlevel 1 exit /b 1
if "%1"=="firmware" (
  pio run -e d1_mini -t upload
  exit /b %errorlevel%
)
if "%1"=="fs" (
  pio run -e d1_mini -t uploadfs
  exit /b %errorlevel%
)
if "%1"=="all" (
  pio run -e d1_mini -t upload
  if errorlevel 1 exit /b 1
  pio run -e d1_mini -t uploadfs
  exit /b %errorlevel%
)
echo Usage: upload.bat firmware ^| fs ^| all
exit /b 2
