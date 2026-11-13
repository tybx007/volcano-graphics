@echo off
rem ===========================================================================
rem  build.bat - double-click friendly wrapper around build.ps1
rem  Examples:
rem      build.bat                 (build volcanox.exe)
rem      build.bat -Target all     (build the final program + all stages)
rem      build.bat -Target final -Run
rem ===========================================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
if errorlevel 1 (
    echo.
    echo Build failed. Read the message above and see docs\SETUP.md.
    pause
    exit /b 1
)
