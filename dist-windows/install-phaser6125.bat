@echo off
REM install-phaser6125.bat — install the Xerox Printer 6125N PAPPL print server on Windows 11.
REM
REM What this does:
REM   1. Copies phaser6125-pappl.exe + DLLs to %ProgramFiles%\Phaser6125PAPPL\
REM   2. Checks for gswin64c.exe (Ghostscript) — see note below
REM   3. Adds the Xerox Printer 6125N printer pointing at socket://192.168.178.10:9100
REM   4. Starts the IPP server on port 8631
REM
REM After running this, other Windows 11 PCs can add the printer via:
REM   Settings → Bluetooth & devices → Printers & scanners → Add device →
REM   "The printer that I want isn't listed" → "Select a shared printer by name" →
REM   http://^<this-pc-ip^>:8631/ipp/print/Phaser6125
REM   (Windows uses the inbox IPP class driver — no legacy V3/V4 driver needed)
REM
REM Ghostscript: download Ghostscript 10.x for Windows (64-bit) from
REM   https://ghostscript.com/releases/gsdnld.html
REM and place gswin64c.exe in the same folder as phaser6125-pappl.exe.

setlocal
set INSTALL_DIR=%ProgramFiles%\Phaser6125PAPPL

echo Installing Xerox Printer 6125N PAPPL to %INSTALL_DIR% ...
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
copy /Y phaser6125-pappl.exe "%INSTALL_DIR%\" >nul
copy /Y *.dll "%INSTALL_DIR%\" >nul
if exist gs\bin\gswin64c.exe (
    xcopy /E /Y /I gs "%INSTALL_DIR%\gs" >nul
    echo Ghostscript bundled: %INSTALL_DIR%\gs\bin\gswin64c.exe
) else (
    echo WARNING: gs\bin\gswin64c.exe not found — printing will not work.
)

REM mDNS/DNS-SD auto-discovery: PAPPL advertises _ipp._tcp via Bonjour when
REM dnssd.dll is available. Install Bonjour Print Services for Windows
REM (https://support.apple.com/kb/DL999) or place dnssd.dll next to the .exe.
where dnssd.dll >nul 2>nul || (
    if not exist "%INSTALL_DIR%\dnssd.dll" (
        echo NOTE: dnssd.dll not found — mDNS auto-discovery disabled.
        echo       Other PCs can still add the printer manually via:
        echo       http://^<this-pc^>:8631/ipp/print/Xerox_Printer_6125N
    )
)

echo.
echo Adding printer "Xerox Printer 6125N" (socket://192.168.178.10:9100) ...
"%INSTALL_DIR%\phaser6125-pappl.exe" add -d "Xerox Printer 6125N" -m xerox_phaser_6125n -v socket://192.168.178.10:9100
if errorlevel 1 (
    echo Printer may already exist — trying modify instead:
    "%INSTALL_DIR%\phaser6125-pappl.exe" modify -d "Xerox Printer 6125N" -v socket://192.168.178.10:9100
)

echo.
echo Starting IPP server on port 8631 ...
start "Phaser6125 PAPPL Server" /min "%INSTALL_DIR%\phaser6125-pappl.exe" server -o server-port=8631

echo.
echo Done. The printer is now available as an IPP Everywhere device.
echo From other PCs: http://^<this-pc^>:8631/ipp/print/Xerox_Printer_6125N
pause
