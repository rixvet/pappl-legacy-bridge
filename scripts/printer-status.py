#!/usr/bin/env python3
"""
printer-status.py — fetch the status of the Xerox Phaser 6125N, in Python.

Two sources, both read-only (no login needed):
  1. HTTP (CentreWare IS web interface):
       - global status, page count, paper trays
       - supplies (Imaging Unit / Fuser / cartridges)
       - last job-log lines
  2. SNMP (this printer speaks ONLY SNMPv1, community 'public'):
       - sysDescr / sysName / sysUpTime
       - prtMarkerSupplies (RFC 1759): description, maxCapacity, level

Known limitation: this model reports toner levels as "--" via HTTP en
level -2 (unknown) via SNMP — the installed cartridges are non-original,
the printer cannot/will not report a level for them. Imaging Unit and
Fuser do report percentages.

Usage:  python3 scripts/printer-status.py
Env:      PRINTER_IP=192.168.178.10   SNMP_COMMUNITY=public   SNMP_TIMEOUT=2
"""
import os
import re
import socket
import struct
import sys
import urllib.request

IP = os.environ.get("PRINTER_IP", "192.168.178.10")
COMMUNITY = os.environ.get("SNMP_COMMUNITY", "public").encode()
SNMP_TIMEOUT = float(os.environ.get("SNMP_TIMEOUT", "2"))
BASE = "http://" + IP


# ---------------------------------------------------------------- HTTP ----
def http(path):
    with urllib.request.urlopen(BASE + path, timeout=5) as r:
        # pages end in a NUL byte; strip it for the parser
        return r.read().decode("utf-8", "ignore").replace("\x00", "")


def tags(html):
    """strip HTML tags -> 1 value per line (drop empty)"""
    txt = re.sub(r"<[^>]+>", "\n", html)
    txt = txt.replace("&nbsp;", " ").replace("&amp;", "&")
    return [l.strip() for l in txt.splitlines() if l.strip()]


def http_status():
    print("-- Status --")
    m = re.search(r">Status</a>:</td><td>([^<]*)", http("/menustat.htm"))
    print(m.group(1) if m else "(unknown)")


def http_page_count():
    print("-- Page Count --")
    lines = tags(http("/status/statgeneralx.htm"))
    for i, l in enumerate(lines):
        if l == "Page Count":
            j = i + 1
            while j < len(lines) and lines[j] == "Page Count":
                j += 1  # duplicate header ('Page Count' caption + label)
            if j < len(lines):
                print(lines[j])
                return
    print("(unknown)")


def http_trays():
    print("-- Paper Tray(s) --")
    lines = tags(http("/status/statgeneralx.htm"))
    for i, l in enumerate(lines):
        if re.fullmatch(r"(Manual Feed Slot|Tray [0-9]+)", l):
            if i + 4 < len(lines):
                print("  %-18s %s" % (l + ":", lines[i + 4]))


def http_supplies():
    print("-- Supplies (HTTP) --")
    lines = tags(http("/status/statsuppliesx.htm"))

    def after_all(label, skip=("Level",)):
        """value line after EVERY occurrence of the label (caption+label duplicates)"""
        vals = []
        for i, l in enumerate(lines):
            if l == label:
                j = i + 1
                while j < len(lines) and lines[j] in skip:
                    j += 1
                if j < len(lines):
                    vals.append(lines[j])
        return vals

    for label in ("Imaging Unit", "Fuser"):
        vals = after_all(label)
        if vals:
            print("  %s: %s" % (label, vals[-1]))
    for c in ("Cyan", "Magenta", "Yellow", "Black"):
        vals = after_all(c)
        if vals:
            print("  %s cartridge: %s" % (c, vals[-1]))


def http_jobs():
    print("-- Last jobs --")
    rows = re.findall(r"<tr><td>.*?</tr>", http("/job/logsys.htm"), re.S)
    for row in rows[-3:]:
        cells = re.sub(r"</td><td[^>]*>", " ", row)
        cells = re.sub(r"<[^>]+>", "", cells).replace("\r", " ").strip()
        print("  " + cells)


# ---------------------------------------------------------------- SNMP ----
# Minimal SNMPv1 client (GET/GETNEXT), no external dependencies.

def _ber_len(n):
    if n < 128:
        return bytes([n])
    b = n.to_bytes((n.bit_length() + 7) // 8, "big")
    return bytes([0x80 | len(b)]) + b


def _tlv(tag, val):
    return bytes([tag]) + _ber_len(len(val)) + val


def _oid(s):
    p = [int(x) for x in s.split(".")]
    out = bytearray([40 * p[0] + p[1]])
    for v in p[2:]:
        enc = bytearray([v & 0x7F])
        v >>= 7
        while v:
            enc.insert(0, 0x80 | (v & 0x7F))
            v >>= 7
        out += enc
    return bytes(out)


def _int(v):
    n = (v.bit_length() + 8) // 8 or 1
    return v.to_bytes(n, "big", signed=True)


def _request(pdu_tag, oids):
    vbs = b"".join(_tlv(0x30, _tlv(0x06, _oid(o)) + _tlv(0x05, b"")) for o in oids)
    pdu = _tlv(pdu_tag, _tlv(0x02, _int(1)) + _tlv(0x02, _int(0)) +
               _tlv(0x02, _int(0)) + _tlv(0x30, vbs))
    msg = _tlv(0x30, _tlv(0x02, _int(0)) + _tlv(0x04, COMMUNITY) + pdu)  # version 0 = v1
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(SNMP_TIMEOUT)
    try:
        s.sendto(msg, (IP, 161))
        data, _ = s.recvfrom(4096)
    except socket.timeout:
        return None
    finally:
        s.close()
    return data


class _R:
    def __init__(self, b):
        self.b, self.i = b, 0

    def tlv(self):
        tag = self.b[self.i]
        self.i += 1
        l = self.b[self.i]
        self.i += 1
        if l & 0x80:
            n = l & 0x7F
            l = int.from_bytes(self.b[self.i:self.i + n], "big")
            self.i += n
        val = self.b[self.i:self.i + l]
        self.i += l
        return tag, val


def _decode_value(tag, val):
    if tag == 0x02:
        return int.from_bytes(val, "big", signed=True)
    if tag == 0x04:
        return val.decode("utf-8", "replace")
    if tag in (0x40, 0x41, 0x42, 0x43):  # IpAddress / Counter / Gauge / TimeTicks
        return int.from_bytes(val, "big")
    if tag == 0x06:
        return _oid_str(val)
    return val


def _oid_str(b):
    parts = [b[0] // 40, b[0] % 40]
    v = 0
    for byte in b[1:]:
        v = (v << 7) | (byte & 0x7F)
        if not byte & 0x80:
            parts.append(v)
            v = 0
    return ".".join(str(p) for p in parts)


def _parse_response(data):
    r = _R(data)
    tag, msg = r.tlv()
    assert tag == 0x30
    r = _R(msg)
    _, _ver = r.tlv()
    _, _comm = r.tlv()
    ptag, pdu = r.tlv()
    assert ptag == 0xA2, "no GetResponse (PTAG=0x%02x)" % ptag
    r = _R(pdu)
    _, _rid = r.tlv()
    _, errstatus = r.tlv()
    _, _errindex = r.tlv()
    if int.from_bytes(errstatus, "big") != 0:
        raise RuntimeError("SNMP error-status %d" % int.from_bytes(errstatus, "big"))
    _, vbs = r.tlv()
    r = _R(vbs)
    out = []
    while r.i < len(r.b):
        _, vb = r.tlv()
        rr = _R(vb)
        _, oid = rr.tlv()
        vtag, vval = rr.tlv()
        out.append((_oid_str(oid), vtag, _decode_value(vtag, vval)))
    return out


def snmp_get(oid):
    data = _request(0xA0, [oid])
    if data is None:
        return None
    return _parse_response(data)[0][2]


def snmp_walk(base):
    """GETNEXT loop within a subtree; returns [(oid, value)]."""
    res = []
    cur = base
    base_enc = _oid(base)
    for _ in range(512):  # safety
        data = _request(0xA1, [cur])
        if data is None:
            return None  # timeout
        try:
            binds = _parse_response(data)
        except Exception:
            return res
        if not binds:
            break
        oid, _tag, val = binds[0]
        enc = _oid(oid)
        if not enc.startswith(base_enc) or len(enc) <= len(base_enc):
            break  # outside the subtree
        if oid == cur:
            break  # no progress
        res.append((oid, val))
        cur = oid
    return res


LEVEL_NAMES = {-1: "other", -2: "unknown", -3: "some remaining", -4: "empty"}
# RFC 1759 prtMarkerSupplies columns: 6=description 7=supplyUnit 8=maxCapacity 9=level
SUPPLIES_BASE = "1.3.6.1.2.1.43.11.1.1"


def snmp_info():
    print("-- SNMP (v1, community %s) --" % COMMUNITY.decode())
    descr = snmp_get("1.3.6.1.2.1.1.1.0")
    if descr is None:
        print("  no response on UDP/161 — SNMP agent disabled?")
        return
    print("  sysDescr:  %s" % descr)
    name = snmp_get("1.3.6.1.2.1.1.5.0")
    if name:
        print("  sysName:   %s" % name)
    up = snmp_get("1.3.6.1.2.1.1.3.0")
    if isinstance(up, int):
        print("  sysUpTime: %d:%02d:%02d" % (up // 360000, up // 6000 % 60, up // 100 % 60))

    rows = {}
    for oid, val in snmp_walk(SUPPLIES_BASE) or []:
        rest = oid[len(SUPPLIES_BASE) + 1:].split(".")
        if len(rest) < 2:
            continue
        col, row = int(rest[0]), ".".join(rest[1:])
        rows.setdefault(row, {})[col] = val
    if rows:
        print("  Supplies (SNMP prtMarkerSupplies):")
        for row in sorted(rows, key=lambda r: [int(x) for x in r.split(".")]):
            c = rows[row]
            desc = c.get(6, "?")
            level = c.get(9)
            mx = c.get(8)
            if isinstance(level, int) and level >= 0 and isinstance(mx, int) and mx > 0:
                v = "%d %% (%d/%d)" % (level * 100 // mx, level, mx)
            elif isinstance(level, int):
                v = LEVEL_NAMES.get(level, "level %d" % level)
            else:
                v = "?"
            print("    %-38s %s" % (desc + ":", v))


def main():
    print("== Xerox Phaser 6125N @ %s ==" % IP)
    try:
        http_status()
        http_page_count()
        http_trays()
        http_supplies()
        http_jobs()
    except Exception as e:
        print("HTTP error: %s" % e)
    try:
        snmp_info()
    except Exception as e:
        print("SNMP error: %s" % e)


if __name__ == "__main__":
    main()
