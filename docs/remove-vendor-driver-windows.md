# Completely removing a vendor v3 printer driver — case study Xerox Phaser 6125N

*Fully documented procedure + debugging guide, based on a real removal on Windows 11 24H2 (November 2026). Reproducible for any v3 printer driver that keeps coming back.*

## 1. Context and symptoms

| | |
|---|---|
| Device | Xerox Phaser 6125N, USB `VID_0924 & PID_3D61`, serial `LAL035151` |
| Driver | v3-printerdriver `xrhwszi.inf` (2007, WHQL), installed as `oem140.inf` |
| Symptom | Removal fails with *"One or more devices are presently installed using the specified INF"*; every USB replug brings the full installation back (queue, port, devnodes) |

The core of the problem: a v3 printer driver package lives in **six** places at once, and as long as one survives, Windows rebuilds the rest.

## 2. The six hiding places (the "ninjas")

| # | Place | Path | Role |
|---|---|---|---|
| 1 | DriverStore | `C:\Windows\System32\DriverStore\FileRepository\xrhwszi.inf_amd64_<hash>\` | Official PnP installation source |
| 2 | Published INF | `C:\Windows\INF\oem140.inf` (+ `.PNF`, `.cat`) | Cache copy with PublishedName mapping |
| 3 | Spooler working files | `C:\Windows\System32\spool\DRIVERS\x64\3\` | DLLs, GPD/CDD/XRS, language monitor |
| 4 | Color profiles | `C:\Windows\System32\spool\drivers\color\` | ICM profiles |
| 5 | **PCC-cab (the main ninja)** | `C:\Windows\System32\spool\drivers\x64\PCC\xrhwszi.inf_amd64_<hash>.cab` | Spooler package cache: **re-imports the package into the store on device detection** |
| 6 | Spooler staging | `C:\Windows\System32\spool\DRIVERS\x64\{GUID}\` | Temporary package copy during re-import |

Plus two "off-shoots" that self-heal as long as the above exist:

- **Port registry**: `HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors\USB Monitor\Ports\USB001` (dynamic USB port with device reference; the spooler rebuilds the devnode from this)
- **Devnodes**: `USBPRINT\<naam>\<id>` en `SWD\PRINTENUM{...}` (queue-devnode) — only disappear once port AND store are clean

And a per-user leftover: printer *connections* to shared printers (`HKCU:\Printers\Connections`, visible as `\\WINNIE\<printer>,LocalOnly` in the event log) can hold a driver without showing up in `Get-Printer`.

## 3. Removal procedure (definitive order)

> **Golden rule: keep the USB cable unplugged until ALL checks are green.** Every replug with a still-present INF = full reinstallation.

**Step 0 — Inventory**

```powershell
pnputil /enum-drivers | Select-String "xerox" -Context 4,6   # look up Published Name
Get-ChildItem C:\Windows\System32\DriverStore\FileRepository -Directory | Where-Object Name -like "<inf>*"
Get-ChildItem C:\Windows\System32\spool\drivers\x64\PCC
Get-ChildItem C:\Windows\System32\spool\DRIVERS\x64\3
Get-Printer | Format-Table Name, DriverName, PortName
Get-PrinterPort
```

**Step 1 — Clean up queues and port**

```powershell
Get-Printer | Where-Object { $_.PortName -eq "USB001" -or $_.PortName -like "WSD-*" } | Remove-Printer
Get-PrinterPort | Where-Object Name -like "WSD-*" | Remove-PrinterPort
# Port removal only works via the registry (the spooler claims dynamic ports):
Stop-Service Spooler
Remove-Item "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\USB Monitor\Ports\USB001" -Recurse -Force
Start-Service Spooler
```

**Step 2 — Per-user printer connections** (normal, non-admin session)

```powershell
Get-ChildItem "HKCU:\Printers\Connections"
# if present:
rundll32 printui.dll,PrintUIEntry /dl /n "\\<HOST>\<share>"
```

**Step 3 — Vendor port- and language monitors**

```powershell
Get-ChildItem "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors"
# remove vendor entries (e.g. "Language Monitor for Xerox ...") with the spooler stopped:
Stop-Service Spooler
Remove-Item "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\<vendormonitor>"
Start-Service Spooler
```

**Step 4 — FIRST: remove the PCC cab and staging (this was the missing step!)**

```powershell
Stop-Service Spooler
Remove-Item "C:\Windows\System32\spool\drivers\x64\PCC\<inf>_amd64_<hash>.cab" -Force
Remove-Item "C:\Windows\System32\spool\DRIVERS\x64\{GUID-stagingdir}" -Recurse -Force -ErrorAction SilentlyContinue
```

**Step 5 — Only then the driver store**

```powershell
pnputil /delete-driver oem###.inf /uninstall
# Fails with "in use": first clean up leftover devnodes (now disconnected):
pnputil /enum-devices /disconnected | Select-String -Context 6 "<vendor>"
pnputil /remove-device "<instance-id>"      # e.g. USBPRINT\... and SWD\PRINTENUM{...}
# Then repeat; only as a last resort:
pnputil /delete-driver oem###.inf /force
```

> `pnputil` semantics: `/uninstall` = detach driver from devices; `/force` = delete package despite use. **Together they are ignored** ("/uninstall wins"). With the PCC cab gone and the cable unplugged, `/uninstall` suddenly suffices.

**Step 6 — Wipe leftover files**

```powershell
Remove-Item C:\Windows\INF\oem###.inf, C:\Windows\INF\oem###.PNF, C:\Windows\INF\oem###.cat -Force -ErrorAction SilentlyContinue
Remove-Item "C:\Windows\System32\DriverStore\FileRepository\<inf>_amd64_<hash>" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item C:\Windows\System32\spool\DRIVERS\x64\3\<vendorprefix>* -Force -ErrorAction SilentlyContinue
Remove-Item C:\Windows\System32\spool\drivers\color\<vendorprefix>*.icm -Force -ErrorAction SilentlyContinue
```

If the FileRepository folder still refuses: take ownership:

```powershell
takeown /f "<map>" /r /d y
icacls "<map>" /grant Administrators:F /t
```

**Step 7 — Reboot (cable unplugged), then the replug test**

After reboot: plug the cable in. Expected end state: the device shows up **driverless/unknown** and Windows may ask for an INF — with an empty store nothing can be reinstalled anymore.

## 4. Verification checklist

```powershell
pnputil /enum-drivers | Select-String "xerox"                        # empty
pnputil /enum-devices /disconnected | Select-String "xerox"          # empty
Get-ChildItem C:\Windows\System32\spool\drivers\x64\PCC              # no vendor cab
Get-ChildItem C:\Windows\System32\spool\DRIVERS\x64\3                # no vendor files
Get-ChildItem "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors"  # no vendor monitors
Get-PrinterPort                                                     # no vendor port description
Get-WinEvent -LogName Microsoft-Windows-PrintService/Admin -MaxEvents 20   # no new 318 events
```

Optional (close the WU door):

```powershell
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate" /v ExcludeWUDriversInQualityUpdate /t REG_DWORD /d 1 /f
```

## 5. Debugging: where to find what

| Source | Location / command | What it tells you |
|---|---|---|
| **PrintService Admin log** | `Get-WinEvent -LogName Microsoft-Windows-PrintService/Admin` | Event 318 (driver settings upgrade errors) at spooler start; reveals hidden queue connections (`\\HOST\printer,LocalOnly`) and driver paths |
| **PrintService Operational log** | `Get-WinEvent -LogName Microsoft-Windows-PrintService/Operational` | Port and job events |
| **setupapi.dev.log** | `C:\Windows\INF\setupapi.dev.log` — e.g. `Select-String -Path ... -Pattern "<inf-file>"` | **The golden source**: shows exactly when and from which path a package was imported (this is how the PCC-cab/staging re-import was unmasked, including timestamps) |
| **Port error codes** | `try { Remove-PrinterPort ... -EA Stop } catch { $_.Exception \| fl * -Force }` | The HRESULT says it all: `0x80070002` = file-not-found → port points at a missing device (dead dynamic port) |
| **Port registry** | `HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors\USB Monitor\Ports` + `reg query ... /s /f USB001` | Device mapping, Parent Device Id, and the port list reference |
| **Monitor inventory** | `HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors` | Vendor port- and language monitors (e.g. `XRZWSLBI.DLL`) |
| **Device queries** | `pnputil /enum-devices /drivers` and `/disconnected` | Which INF holds each devnode, including ghost devnodes |
| **Driver store** | `pnputil /enum-drivers`, `Get-WindowsDriver -Online` | Published names, original INF names, dates |

Interpretation aids:
- Error `1801` = invalid printer name, `87` = invalid parameter → typical for stale connections/queues.
- Event 318 repeats at exactly boot time → the spooler tries to validate something at every start that is actually no longer there.
- `Driver package uninstalled. Failed to delete driver package` → the `/uninstall` succeeded, but a devnode remains; check `/enum-devices /disconnected`.

## 6. Pitfalls from practice (lessons learned)

1. **Cable unplugged is not a detail but the core**: every replug with a filled store = full resurrection via best-match install.
2. **The PCC cab is the real ninja**: cleaning the DriverStore without emptying `spool\drivers\x64\PCC\` is pointless — the spooler re-imports the package on device detection (provable in setupapi.dev.log).
3. **Port removal fails as long as the device path is unresolvable** (unplugged) → registry surgery with the spooler stopped is the only reliable route for dynamic ports.
4. **`pnputil /force + /uninstall` together does not work** — the force flag is ignored.
5. **Hidden clients of an INF**: `USBPRINT\...` devnodes, `SWD\PRINTENUM{...}` queue devnodes and per-user share connections (`\\HOST\printer,LocalOnly`) — none of them visible in `Get-Printer`.
6. **`\\.\USB001` does not exist as a device file** (err 2 / `0x80070002`): spooler ports are APIs, not kernel objects; raw data can only go via winspool (RAW datatype) or via a shared queue (`copy /b file \\HOST\Share`).
7. Reboot in between to break file locks if `Remove-Item` on driver DLLs fails.

## 7. Appendix: raw printing to the cleaned USB port (Win 11 24H2)

After cleanup the Phaser is purely inbox (`usbprint.inf`) and `USB001` appears automatically once a queue uses the printer:

```powershell
# One-time: test queue on the port + share as RAW channel
Add-Printer -Name "Phaser Raw" -DriverName "Generic / Text Only" -PortName USB001
Set-Printer -Name "Phaser Raw" -Shared $True -ShareName "PhaserRaw"
```

```bat
:: Per job: raw blob (e.g. HBPL from foo2sq21hbpl) straight to the printer
copy /b C:\temp\colorpage.hbpl \\%COMPUTERNAME%\PhaserRaw
```

Inside PowerShell: `\\$env:COMPUTERNAME\PhaserRaw` (note: `powershell -Command "... $True ..."` from within PowerShell interpolates `$True` — run such lines directly in the session).

Alternative without share: winspool RAW via .NET (`OpenPrinter` → `StartDocPrinter` with `pDataType = "RAW"` → `WritePrinter`). See also `pappl/phaser6125-pappl.c` (`usbprint:` scheme, same mechanism in C).

---

*Device data for reuse: Xerox Phaser 6125N · USB `0924:3d61` · serial `LAL035151` · printer-interface-GUID `{28d78fad-5a12-11d1-ae5b-0000f803a8c2}` · original driver `xrhwszi.inf` (2007) → `oem140.inf`.*
