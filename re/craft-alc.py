#!/usr/bin/env python3
# craft FXRaster (44-byte header + RGB data) en draai FXM_ALC, dump de sq21 stream
import struct, subprocess, sys, os
REPO='/home/rick/src/phaser6125'
SYSROOT='/home/rick/sysroot-i386'
LOADER=SYSROOT+'/lib/ld-linux.so.2'
LIBPATH=SYSROOT+'/usr/lib/i386-linux-gnu:'+SYSROOT+'/lib/i386-linux-gnu'
FILTER=REPO+'/analysis/tarball/usr/lib/cups/filter/FXM_ALC'
PPD=REPO+'/re/build/Phaser_6125N.ppd'
OPTS="PageSize=A4 FXOutputMode=600x600 FXColorMode=Color InstalledMemory=64Meg Option1=1Tray-S FXInputSlot=AutoSelect"

def fxraster(w,h,pixels):  # pixels: list of (r,g,b) row-major, len w*h
    data = bytes(b for px in pixels for b in px)
    hdr = struct.pack('<11I', w, h, 24, w*3, len(data), 600, 600, 0, 0, 1, 0)
    return hdr + data

def run(name, w, h, pixels):
    fxr = fxraster(w,h,pixels)
    env = dict(os.environ, PPD=PPD, CUPS_FONTPATH='/usr/share/fonts')
    p = subprocess.run([LOADER,'--library-path',LIBPATH,FILTER,'1','rick',name,'1',OPTS],
                       input=fxr, capture_output=True, env=env)
    out = p.stdout
    # parse ALC output header
    if len(out) >= 44:
        f = struct.unpack('<11I', out[:44])
        stream = out[44:]
        print(f"== {name}: in {w}x{h} -> out hdr w={f[0]} h={f[1]} bpp={f[2]} rowb={f[3]} dsize={f[4]} f9={f[9]} f10={f[10]} | stream {len(stream)} bytes")
        print("   stream:", stream[:96].hex(' '))
        if len(stream) > 96: print("   ...tail:", stream[-32:].hex(' '))
        open(f'/home/rick/src/phaser6125/re/build/alc-{name}.sq21','wb').write(stream)
    else:
        print(f"== {name}: output too small ({len(out)} bytes), stderr:", p.stderr[:200])

W=(0,0,0); Wh=(255,255,255)
run('zero-8x1', 8,1, [W]*8)
run('white-8x1', 8,1, [Wh]*8)
run('alt-8x1', 8,1, [W,Wh]*4)
run('zero-8x2', 8,2, [W]*16)
run('zero-16x1', 16,1, [W]*16)
run('one-8x1', 8,1, [W]*4+[(1,2,3)]+[W]*3)
