# Vendor v3-printerdriver volledig verwijderen — casus Xerox Phaser 6125N

*Volledig gedocumenteerde procedure + debugging-handreiking, gebaseerd op een reële verwijderactie op Windows 11 24H2 (november 2026). Reproduceerbaar voor elke v3-printerdriver die blijft terugkomen.*

## 1. Context en symptomen

| | |
|---|---|
| Apparaat | Xerox Phaser 6125N, USB `VID_0924 & PID_3D61`, serial `LAL035151` |
| Driver | v3-printerdriver `xrhwszi.inf` (2007, WHQL), geïnstalleerd als `oem140.inf` |
| Symptoom | Verwijderen faalt met *"One or more devices are presently installed using the specified INF"*; bij elke USB-replug komt de volledige installatie terug (queue, poort, devnodes) |

De kern van het probleem: een v3-printerdriverpakket leeft op **zes** plekken tegelijk, en zolang er één overleeft, herbouwt Windows de rest.

## 2. De zes schuilplaatsen (de "ninja's")

| # | Plek | Pad | Rol |
|---|---|---|---|
| 1 | DriverStore | `C:\Windows\System32\DriverStore\FileRepository\xrhwszi.inf_amd64_<hash>\` | Officiële installatiebron voor PnP |
| 2 | Published INF | `C:\Windows\INF\oem140.inf` (+ `.PNF`, `.cat`) | Cache-kopie met PublishedName-koppeling |
| 3 | Spooler-werkbestanden | `C:\Windows\System32\spool\DRIVERS\x64\3\` | DLL's, GPD/CDD/XRS, language monitor |
| 4 | Kleurprofielen | `C:\Windows\System32\spool\drivers\color\` | ICM-profielen |
| 5 | **PCC-cab (de hoofdninja)** | `C:\Windows\System32\spool\drivers\x64\PCC\xrhwszi.inf_amd64_<hash>.cab` | Spooler-pakketcache: **herimporteert het pakket in de store bij apparaat-detectie** |
| 6 | Spooler-staging | `C:\Windows\System32\spool\DRIVERS\x64\{GUID}\` | Tijdelijke pakketkopie tijdens herimport |

Daarnaast twee "uitlopers" die zichzelf herstellen zolang bovenstaande bestaan:

- **Poort-registry**: `HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors\USB Monitor\Ports\USB001` (dynamische USB-poort met device-verwijzing; spooler herbouwt hieruit de devnode)
- **Devnodes**: `USBPRINT\<naam>\<id>` en `SWD\PRINTENUM{...}` (queue-devnode) — verdwijnen pas als poort én store schoon zijn

En een per-user restant: printer-*connections* naar gedeelde printers (`HKCU:\Printers\Connections`, zichtbaar als `\\WINNIE\<printer>,LocalOnly` in het eventlog) kunnen een driver vasthouden zonder in `Get-Printer` te verschijnen.

## 3. Verwijderprocedure (definitieve volgorde)

> **Gulden regel: de USB-kabel blijft eruit tot álle controles groen zijn.** Elke replug met een nog-aanwezige INF = volledige herinstallatie.

**Stap 0 — Inventarisatie**

```powershell
pnputil /enum-drivers | Select-String "xerox" -Context 4,6   # Published Name opzoeken
Get-ChildItem C:\Windows\System32\DriverStore\FileRepository -Directory | Where-Object Name -like "<inf>*"
Get-ChildItem C:\Windows\System32\spool\drivers\x64\PCC
Get-ChildItem C:\Windows\System32\spool\DRIVERS\x64\3
Get-Printer | Format-Table Name, DriverName, PortName
Get-PrinterPort
```

**Stap 1 — Queues en poort opruimen**

```powershell
Get-Printer | Where-Object { $_.PortName -eq "USB001" -or $_.PortName -like "WSD-*" } | Remove-Printer
Get-PrinterPort | Where-Object Name -like "WSD-*" | Remove-PrinterPort
# Poort verwijderen lukt pas via de registry (de spooler claimt dynamische poorten):
Stop-Service Spooler
Remove-Item "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\USB Monitor\Ports\USB001" -Recurse -Force
Start-Service Spooler
```

**Stap 2 — Per-user printer-connections** (gewone, niet-admin sessie)

```powershell
Get-ChildItem "HKCU:\Printers\Connections"
# indien aanwezig:
rundll32 printui.dll,PrintUIEntry /dl /n "\\<HOST>\<share>"
```

**Stap 3 — Vendor-port- en language-monitors**

```powershell
Get-ChildItem "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors"
# vendor-entries (bijv. "Language Monitor for Xerox ...") verwijderen met spooler gestopt:
Stop-Service Spooler
Remove-Item "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors\<vendormonitor>"
Start-Service Spooler
```

**Stap 4 — ÉÉRSTE: de PCC-cab en staging weg (dit was de missende stap!)**

```powershell
Stop-Service Spooler
Remove-Item "C:\Windows\System32\spool\drivers\x64\PCC\<inf>_amd64_<hash>.cab" -Force
Remove-Item "C:\Windows\System32\spool\DRIVERS\x64\{GUID-stagingdir}" -Recurse -Force -ErrorAction SilentlyContinue
```

**Stap 5 — Daarna pas de driverstore**

```powershell
pnputil /delete-driver oem###.inf /uninstall
# Faalt met "in use": eerst achtergebleven devnodes opruimen (nu disconnected):
pnputil /enum-devices /disconnected | Select-String -Context 6 "<vendor>"
pnputil /remove-device "<instance-id>"      # bijv. USBPRINT\... en SWD\PRINTENUM{...}
# Daarna herhalen; pas als laatste redmiddel:
pnputil /delete-driver oem###.inf /force
```

> `pnputil`-semantiek: `/uninstall` = driver van devices loskoppelen; `/force` = pakket verwijderen ondanks gebruik. **Samen worden ze genegeerd** ("/uninstall wint"). Met de PCC-cab weg en kabel eraf is `/uninstall` ineens voldoende.

**Stap 6 — Restbestanden vegen**

```powershell
Remove-Item C:\Windows\INF\oem###.inf, C:\Windows\INF\oem###.PNF, C:\Windows\INF\oem###.cat -Force -ErrorAction SilentlyContinue
Remove-Item "C:\Windows\System32\DriverStore\FileRepository\<inf>_amd64_<hash>" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item C:\Windows\System32\spool\DRIVERS\x64\3\<vendorprefix>* -Force -ErrorAction SilentlyContinue
Remove-Item C:\Windows\System32\spool\drivers\color\<vendorprefix>*.icm -Force -ErrorAction SilentlyContinue
```

Blijft de FileRepository-map weigeren: eigendomsrechten overnemen:

```powershell
takeown /f "<map>" /r /d y
icacls "<map>" /grant Administrators:F /t
```

**Stap 7 — Reboot (kabel eraf), dan de replug-test**

Na reboot: kabel erin. Verwachte eindtoestand: apparaat verschijnt **driverloos/onbekend** en Windows vraagt eventueel om een INF — met een lege store kan er niets meer herinstalleerd worden.

## 4. Verificatiechecklist

```powershell
pnputil /enum-drivers | Select-String "xerox"                        # leeg
pnputil /enum-devices /disconnected | Select-String "xerox"          # leeg
Get-ChildItem C:\Windows\System32\spool\drivers\x64\PCC              # geen vendor-cab
Get-ChildItem C:\Windows\System32\spool\DRIVERS\x64\3                # geen vendorbestanden
Get-ChildItem "HKLM:\SYSTEM\CurrentControlSet\Control\Print\Monitors"  # geen vendor-monitors
Get-PrinterPort                                                     # geen vendor-poortbeschrijving
Get-WinEvent -LogName Microsoft-Windows-PrintService/Admin -MaxEvents 20   # geen nieuwe 318-events
```

Optioneel (WU-deur dicht):

```powershell
reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate" /v ExcludeWUDriversInQualityUpdate /t REG_DWORD /d 1 /f
```

## 5. Debugging: waar vind je wat

| Bron | Locatie / commando | Wat het je vertelt |
|---|---|---|
| **PrintService Admin-log** | `Get-WinEvent -LogName Microsoft-Windows-PrintService/Admin` | Event 318 (driver-settings-upgrade-fouten) bij spoolerstart; onthult verborgen queue-connecties (`\\HOST\printer,LocalOnly`) en driverpaden |
| **PrintService Operational-log** | `Get-WinEvent -LogName Microsoft-Windows-PrintService/Operational` | Poort- en job-gebeurtenissen |
| **setupapi.dev.log** | `C:\Windows\INF\setupapi.dev.log` — bijv. `Select-String -Path ... -Pattern "<infbestand>"` | **De gouden bron**: toont exact wanneer en van welk pad een pakket geïmporteerd is (zo werd de PCC-cab/staging-herimport ontmaskerd, inclusief tijdstempen) |
| **Poort-foutcodes** | `try { Remove-PrinterPort ... -EA Stop } catch { $_.Exception \| fl * -Force }` | HRESULT zegt alles: `0x80070002` = file-not-found → poort verwijst naar afwezig device (dode dynamische poort) |
| **Poort-registry** | `HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors\USB Monitor\Ports` + `reg query ... /s /f USB001` | Device-koppeling, Parent Device Id, en de portlist-referentie |
| **Monitor-inventaris** | `HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors` | Vendor-port- en language-monitors (bijv. `XRZWSLBI.DLL`) |
| **Device-query's** | `pnputil /enum-devices /drivers` en `/disconnected` | Welke INF elke devnode vasthoudt, inclusief spook-devnodes |
| **Driverstore** | `pnputil /enum-drivers`, `Get-WindowsDriver -Online` | Published Names, originele INF-namen, datums |

Interpretatie-hulpen:
- Fout `1801` = ongeldige printernaam, `87` = ongeldige parameter → typisch voor stale connecties/queues.
- Event 318-regens bij exact het boot-tijdstip → spooler probeert bij elke start iets te valideren dat er eigenlijk niet meer is.
- `Driver package uninstalled. Failed to delete driver package` → de `/uninstall` lukte, maar er is nog een devnode; check `/enum-devices /disconnected`.

## 6. Valkuilen uit de praktijk (lessons learned)

1. **Kabel eraf is geen detail maar de kern**: elke replug met een gevulde store = volledige resurrectie via beste-match-installatie.
2. **De PCC-cab is de echte ninja**: de DriverStore opruimen zonder `spool\drivers\x64\PCC\` leeg te maken is zinloos — de spooler herimporteert het pakket bij apparaat-detectie (bewijsbaar in setupapi.dev.log).
3. **Poort verwijderen faalt zolang het device-pad onoplosbaar is** (unplugged) → registry-chirurgie met spooler gestopt is de enige betrouwbare route voor dynamische poorten.
4. **`pnputil /force + /uninstall` samen werkt niet** — de force-vlag wordt genegeerd.
5. **Verborgen klanten van een INF**: `USBPRINT\...`-devnodes, `SWD\PRINTENUM{...}`-queue-devnodes en per-user share-connecties (`\\HOST\printer,LocalOnly`) — geen van alle zichtbaar in `Get-Printer`.
6. **`\\.\USB001` bestaat niet als device-file** (err 2 / `0x80070002`): spooler-poorten zijn API's, geen kernel-objecten; raw data kan alleen via winspool (RAW-datatype) of via een gedeelde queue (`copy /b bestand \\HOST\Share`).
7. Reboot tussentijds om bestandsvergrendelingen te breken als `Remove-Item` op driver-DLL's faalt.

## 7. Bijlage: raw printen naar de schone USB-poort (Win 11 24H2)

Na de schoonmaak is de Phaser puur inbox (`usbprint.inf`) en ontstaat `USB001` automatisch zodra een queue de printer gebruikt:

```powershell
# Eenmalig: testqueue op de poort + share als RAW-kanaal
Add-Printer -Name "Phaser Raw" -DriverName "Generic / Text Only" -PortName USB001
Set-Printer -Name "Phaser Raw" -Shared $True -ShareName "PhaserRaw"
```

```bat
:: Per job: raw blob (bijv. HBPL uit foo2sq21hbpl) rechtstreeks naar de printer
copy /b C:\temp\colorpage.hbpl \\%COMPUTERNAME%\PhaserRaw
```

Binnen PowerShell: `\\$env:COMPUTERNAME\PhaserRaw` (en let op: `powershell -Command "... $True ..."` vanuit PowerShell interpoleert `$True` — draai Zulke regels direct in de sessie).

Alternatief zonder share: winspool RAW via .NET (`OpenPrinter` → `StartDocPrinter` met `pDataType = "RAW"` → `WritePrinter`). Zie ook `pappl/phaser6125-pappl.c` (`usbprint:` scheme, zelfde mechanisme in C).

---

*Device-gegevens voor hergebruik: Xerox Phaser 6125N · USB `0924:3d61` · serial `LAL035151` · printer-interface-GUID `{28d78fad-5a12-11d1-ae5b-0000f803a8c2}` · oorspronkelijke driver `xrhwszi.inf` (2007) → `oem140.inf`.*
