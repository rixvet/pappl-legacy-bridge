# capture-discovery.ps1 — capture mDNS/WSD/SSDP/IPP traffic during Windows "Add device"
# Usage: run this, then do Refresh / Add Device in Windows Settings, wait for duration.
# Output: C:\temp\discovery.pcap + printed summary
param(
    [int]$DurationSec = 45,
    [string]$OutPcap = "C:\temp\discovery.pcap",
    [int]$Interface = 5   # Ethernet 2 (LAN) — get interfaces via: tshark -D
)
$tshark = "C:\Program Files\Wireshark\tshark.exe"
if (-not (Test-Path $tshark)) { Write-Host "tshark niet gevonden: $tshark"; exit 1 }

$filter = "udp port 5353 or udp port 3702 or udp port 1900 or udp port 161 or tcp port 631 or tcp port 8631"
Write-Host "Capturing $DurationSec sec on interface $Interface → $OutPcap"
Write-Host "DOE NU: Refresh + Add Device in Windows Settings → Printers & scanners"
& $tshark -i $Interface -f $filter -a "duration:$DurationSec" -w $OutPcap 2>&1
Write-Host "=== capture done, summary ==="
& $tshark -r $OutPcap -q -z io,phs 2>&1 | Select-Object -First 20
Write-Host "=== mDNS records van deze host ==="
& $tshark -r $OutPcap -Y "dns.flags.response==1" -T fields -e dns.ptr.domain_name -e dns.txt 2>$null | Select-Object -Unique | Select-Object -First 20
Write-Host "=== WSD (3702) ==="
& $tshark -r $OutPcap -Y "udp.port==3702" 2>$null | Select-Object -First 10
