#!/usr/bin/env python3
"""Extract static CST tables/constants from the 32-bit FXM_CC ELF.

File offset = vaddr - 0x08048000 (both .rodata and .data).
Outputs (in re/build/):
  xyz2srgbtrc.bin   4096 bytes  @ 0x080502e0  (inverse TRC, 12bit -> 8bit)
  gamma22trc.bin     512 bytes  @ 0x080516e0  (256 x u16 LE, 8bit -> 12bit)
  srgb-prim.bin      216 bytes  @ 0x08050290  (sBlue/Green/RedXYZ, 3x3 doubles)
  xyz2srgb-matrix.bin 72 bytes  @ 0x0804ede0  (9 doubles)
  cst-consts.txt     DAT_0804eec8 (double) + DAT_0804eecc (float)
"""
import struct, os

ELF = "/home/rick/src/phaser6125/analysis/tarball/usr/lib/cups/filter/FXM_CC"
OUT = "/home/rick/src/phaser6125/re/build"
BASE = 0x08048000

data = open(ELF, "rb").read()

def at(vaddr, n):
    off = vaddr - BASE
    return data[off:off + n]

def w(name, blob):
    p = os.path.join(OUT, name)
    open(p, "wb").write(blob)
    print(f"{name}: {len(blob)} bytes")

# XYZ2sRGBTrc: 4096 bytes (0x080502e0 .. 0x080512e0)
w("xyz2srgbtrc.bin", at(0x080502e0, 0x1000))
# gamma22TRC: 256 u16 (0x080516e0 .. 0x080518e0)
w("gamma22trc.bin", at(0x080516e0, 0x200))
# sRGB primaries: sBlueXYZ @0x08050290, sGreenXYZ @0x080502a8, sRedXYZ @0x080502c0
prim = at(0x08050290, 0x48)  # 3 x 24 bytes
w("srgb-prim.bin", prim)
# XYZ2sRGBMatrix: 9 doubles @ 0x0804ede0
w("xyz2srgb-matrix.bin", at(0x0804ede0, 72))

# constants
d0 = struct.unpack("<d", at(0x0804eec8, 8))[0]
f0 = struct.unpack("<f", at(0x0804eecc, 4))[0]
with open(os.path.join(OUT, "cst-consts.txt"), "w") as fh:
    fh.write(f"DAT_0804eec8 (double) = {d0!r}\nDAT_0804eecc (float)  = {f0!r}  hex={struct.unpack('<I', at(0x0804eecc,4))[0]:#x}\n")
print(f"DAT_0804eec8 (double) = {d0!r}")
print(f"DAT_0804eecc (float)  = {f0!r}")

# sanity: print primaries matrix + matrix
P = struct.unpack("<9d", prim)   # order in memory: sBlue, sGreen, sRed rows? (24B each)
print("sRGB primaries (as stored, 3x3 doubles):")
for r in range(3):
    print("  ", ["%.10g" % P[r * 3 + c] for c in range(3)])
M = struct.unpack("<9d", at(0x0804ede0, 72))
print("XYZ2sRGBMatrix (3x3 doubles):")
for r in range(3):
    print("  ", ["%.10g" % M[r * 3 + c] for c in range(3)])
# product M x P^T? try both orders, see which is ~identity
import itertools
def matmul(A, B):
    return [sum(A[r * 3 + k] * B[k * 3 + c] for k in range(3)) for r in range(3) for c in range(3)]
prod = matmul(M, P)
print("XYZ2sRGB x Prim (row-major):")
for r in range(3):
    print("  ", ["%.6g" % prod[r * 3 + c] for c in range(3)])
tr = [P[c * 3 + r] for r in range(3) for c in range(3)]
prod2 = matmul(M, tr)
print("XYZ2sRGB x Prim^T:")
for r in range(3):
    print("  ", ["%.6g" % prod2[r * 3 + c] for c in range(3)])

# TRC tables sanity
g = struct.unpack("<256H", at(0x080516e0, 0x200))
print(f"gamma22TRC[0..8]   = {g[:9]}")
print(f"gamma22TRC[128]    = {g[128]}  (expect ~ {round(4095*(128/255)**2.2)})")
print(f"gamma22TRC[254,255]= {g[254]},{g[255]}")
t = at(0x080502e0, 0x1000)
print(f"XYZ2sRGBTrc[0..8]  = {list(t[:9])}")
print(f"XYZ2sRGBTrc[2048]  = {t[2048]}  (expect ~ {round(255*(2048/4095)**(1/2.2))})")
print(f"XYZ2sRGBTrc[4094,4095] = {t[4094]},{t[4095]}")
