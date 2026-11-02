@echo off
REM build-windows.bat — build phaser6125-pappl.exe on Windows with MSVC.
REM
REM Prerequisites:
REM   - Visual Studio 2022 (with "Desktop development with C++" workload, v143 toolset)
REM   - NuGet.exe on PATH (or use VS's built-in restore)
REM   - PAPPL 1.4.x installed as NuGet package libpappl1_native 1.4.12
REM     (or build PAPPL from source and adjust vcnet/phaser6125-pappl.vcxproj)
REM   - Ghostscript for Windows (gswin64c.exe) placed next to the built .exe
REM
REM Usage:
REM   vcnet\build-windows.bat
REM
REM Output: x64\Release\phaser6125-pappl.exe
setlocal

cd /d "%~dp0"

where nuget >nul 2>nul
if %errorlevel%==0 (
    nuget restore vcnet\packages.config -PackagesDirectory vcnet\packages
) else (
    echo NuGet not found on PATH — using MSBuild restore instead.
    msbuild vcnet\phaser6125-pappl.vcxproj /t:Restore
)

msbuild vcnet\phaser6125-pappl.vcxproj /p:Configuration=Release /p:Platform=x64 /m

if exist "x64\Release\phaser6125-pappl.exe" (
    echo.
    echo OK: x64\Release\phaser6125-pappl.exe
    echo.
    echo Next steps:
    echo   1. Place gswin64c.exe (Ghostscript 10.x 64-bit) next to the .exe
    echo   2. Run:  phaser6125-pappl.exe server -o server-port=8631
    echo   3. Run:  phaser6125-pappl.exe add -d Phaser6125 -m xerox_phaser_6125n -v socket://192.168.178.10:9100
    echo   4. On other PCs: add printer via http://^<this-pc^>:8631/ipp/print/Phaser6125
) else (
    echo BUILD FAILED
    exit /b 1
)
