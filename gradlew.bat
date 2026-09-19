@echo off
where gradle >nul 2>nul
if %ERRORLEVEL% EQU 0 (
  gradle %*
  exit /b %ERRORLEVEL%
)
echo Gradle is not installed. Install Gradle 8.10.2+ or generate the standard Gradle wrapper.
exit /b 1
