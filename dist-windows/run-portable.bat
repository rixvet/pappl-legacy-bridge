@echo off
REM run-portable.bat — run the Phaser 6125N IPP server WITHOUT installing.
REM No admin rights needed: port 8631 is unprivileged, spool goes to %LocalAppData%.
REM
REM Usage:
REM   1. Unzip Phaser6125N-PAPPL-win64.zip anywhere (e.g. USB stick, Downloads)
REM   2. Double-click run-portable.bat
REM   3. Add the printer:  phaser6125-pappl.exe add -d Phaser6125 -m xerox_phaser_6125n -v socket://192.168.178.10:9100
REM   4. From other PCs:  http://<this-pc>:8631/ipp/print/Phaser6125
REM      (or auto-discovery if Bonjour/dnssd.dll is present)

setlocal
set HERE=%~dp0
cd /d "%HERE%"

set SPOOL=%LocalAppData%\Phaser6125PAPPL
if not exist "%SPOOL%" mkdir "%SPOOL%"

echo Phaser 6125N PAPPL — portable mode
if not exist gs\bin\gswin64c.exe (
    echo ERROR: gs\bin\gswin64c.exe not found in %HERE%gs\bin\
    pause
    exit /b 1
)
echo Ghostscript: %HERE%gs\bin\gswin64c.exe
echo Spool dir:  %SPOOL%
echo.

REM Check if printer already added; if not, add it
phaser6125-pappl.exe printers | findstr /C:"Phaser6125" >nul 2>nul
if errorlevel 1 (
    echo Adding printer Phaser6125 (socket://192.168.178.10:9100) ...
    phaser6125-pappl.exe add -d Phaser6125 -m xerox_phaser_6125n -v socket://192.168.178.10:9100
    if errorlevel 1 (
        echo Printer may already exist or add failed — continuing anyway.
    )
) else (
    echo Printer Phaser6125 already configured.
)

echo.
echo Starting IPP server on port 8631 ...
echo Web UI:  http://localhost:8631/
echo From other PCs: http://^<this-pc^>:8631/ipp/print/Phaser6125
echo Press Ctrl+C to stop.
echo.

phaser6125-pappl.exe server -o server-port=8631 -o spool-directory="%SPOOL%" -o log-level=info
