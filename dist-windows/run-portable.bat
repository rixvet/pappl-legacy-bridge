@echo off
REM run-portable.bat — run the Xerox Printer 6125N IPP server WITHOUT installing.
REM No admin rights needed: port 8631 is unprivileged, spool goes to %LocalAppData%.
REM
REM Usage:
REM   1. Unzip Phaser6125N-PAPPL-win64.zip anywhere (e.g. USB stick, Downloads)
REM   2. Double-click run-portable.bat
REM   3. Add the printer:  phaser6125-pappl.exe add -d "Xerox Printer 6125N" -m xerox_phaser_6125n -v %PRINTER_DEV% -o printer-device-id="MFG:Xerox;MDL:Printer 6125N;SN:LAL035151;CMD:HBPL,PWGRaster,URF,PDF,PS,JPEG,PNG,TXT;"
REM   4. From other PCs:  http://<this-pc>:8631/ipp/print/Phaser6125
REM      (or auto-discovery if Bonjour/dnssd.dll is present)

setlocal

REM ---- Configuration: set your printer IP here (or via PRINTER_IP env var) ----
if "%PRINTER_IP%"=="" set PRINTER_IP=192.168.178.10
REM Device: network (default) or USB. For USB set: set PRINTER_DEV=usbprint://./USB001
REM --- USB raw share (Win11 24H2+: \\.\USB001 is spooler-internal, err 2) ---
REM Create a raw share on the USB port; gateway writes via \\%COMPUTERNAME%\PhaserRaw
powershell -NoProfile -Command "Add-PrinterDriver -Name 'Generic / Text Only' -ErrorAction SilentlyContinue; if (-not (Get-Printer -Name 'Phaser Raw' -ErrorAction SilentlyContinue)) { Add-Printer -Name 'Phaser Raw' -DriverName 'Generic / Text Only' -PortName USB001 -ErrorAction SilentlyContinue }; Set-Printer -Name 'Phaser Raw' -Shared $True -ShareName 'PhaserRaw' -ErrorAction SilentlyContinue"
if "%PRINTER_DEV%"=="" set PRINTER_DEV=socket://%PRINTER_IP%:9100
set HERE=%~dp0
cd /d "%HERE%"

set SPOOL=%LocalAppData%\Phaser6125PAPPL
if not exist "%SPOOL%" mkdir "%SPOOL%"

echo Xerox Printer 6125N PAPPL — portable mode
if not exist gs\bin\gswin64c.exe (
    echo ERROR: gs\bin\gswin64c.exe not found in %HERE%gs\bin\
    pause
    exit /b 1
)
echo Ghostscript: %HERE%gs\bin\gswin64c.exe
echo Spool dir:  %SPOOL%
echo.

REM Remove legacy queue name (mDNS advertised it as "Phaser6125")
phaser6125-pappl.exe printers | findstr /C:"Phaser6125" >nul 2>nul
if not errorlevel 1 (
    echo Removing legacy queue "Phaser6125" ...
    phaser6125-pappl.exe delete -d Phaser6125 2>nul
)
REM Check if printer already added; if not, add it
phaser6125-pappl.exe printers | findstr /C:"Xerox Printer 6125N" >nul 2>nul
if errorlevel 1 (
    echo Adding printer "Xerox Printer 6125N" (%PRINTER_DEV%) ...
    phaser6125-pappl.exe add -d "Xerox Printer 6125N" -m xerox_phaser_6125n -v %PRINTER_DEV% -o printer-device-id="MFG:Xerox;MDL:Printer 6125N;SN:LAL035151;CMD:HBPL,PWGRaster,URF,PDF,PS,JPEG,PNG,TXT;"
    if errorlevel 1 (
        echo Printer may already exist or add failed — continuing anyway.
    )
) else (
    echo Printer "Xerox Printer 6125N" already configured.
)

echo.
echo Starting IPP server on port 8631 ...
echo Web UI:  http://localhost:8631/
echo From other PCs: http://^<this-pc^>:8631/ipp/print/Xerox_Printer_6125N
echo Press Ctrl+C to stop.
echo.

phaser6125-pappl.exe server -o server-port=8631 -o spool-directory="%SPOOL%" -o log-level=info
