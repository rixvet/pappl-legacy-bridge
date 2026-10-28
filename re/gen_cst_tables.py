#!/usr/bin/env python3
"""Genereer native/cst_tables.h — de CST-tabellen van FXM_CC.

Bron: reverse engineered uit de originele 32-bit FXM_CC binary
(Xerox/Fuji DocuPrint C525A / Phaser 6125N driver):

  CST_TRC   gamma22TRC  — 256 x u16le, .data @ 0x080516e0
                          8-bit sRGB -> 16-bit lineair (gamma 2.2)
  CST_ITRC  XYZ2sRGBTrc — 4096 bytes, .data @ 0x080502e0
                          12-bit lineair -> 8-bit (inverse TRC)
  CST_M     3x3 matrix in signed Q16 fixed point, zoals berekend door
            cil_Create_CST_Param_WpGm(sRGB primaries + gamma22TRC):
            M[i] = ROUND((float)m_i * 65536), m = XYZ2sRGBMatrix x sRGBprimaries.
            Live gedumpt uit het draaiende proces (re/dump-params.gdb,
            cst-param.bin +0x600).  Pixel math: out = ((M[i]>>2)*v + ...) >> 18
            == m*v/16 (16-bit -> 12-bit).

De gegenereerde tabellen zijn byte-exact geverifieerd tegen de originele
FXM_CC output (re/verify_model.c: 256^3 kleuren + een echte pagina).
Zie RE-NOTES.md §9.

Gebruik:  python3 re/gen_cst_tables.py   (vanaf de repo root)
"""
import struct
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(REPO, 're', 'build')
OUT = os.path.join(REPO, 'native', 'cst_tables.h')


def read(p):
    with open(p, 'rb') as f:
        return f.read()


def main():
    g22 = read(os.path.join(BUILD, 'gamma22trc.bin'))
    itrc = read(os.path.join(BUILD, 'xyz2srgbtrc.bin'))
    cst = read(os.path.join(BUILD, 'cst-param.bin'))
    assert len(g22) == 512, 'gamma22trc.bin: 512 bytes verwacht'
    assert len(itrc) == 4096, 'xyz2srgbtrc.bin: 4096 bytes verwacht'
    assert len(cst) == 1684, 'cst-param.bin: 1684 bytes verwacht'

    # sanity: de TRC-tabellen in de runtime dump zijn 3x gamma22TRC
    assert cst[0x000:0x200] == g22, 'TRC0 != gamma22TRC'
    assert cst[0x200:0x400] == g22, 'TRC1 != gamma22TRC'
    assert cst[0x400:0x600] == g22, 'TRC2 != gamma22TRC'
    assert struct.unpack('<i', cst[0x62c:0x630])[0] == 0, 'CST status != 0'
    assert struct.unpack('<i', cst[0x690:0x694])[0] == 0, 'CST flag != 0 (sRGB)'

    trc = struct.unpack('<256H', g22)
    M = struct.unpack('<9i', cst[0x600:0x624])

    def fmt_u16(vals, per=16):
        lines = []
        for i in range(0, len(vals), per):
            lines.append('    ' + ', '.join('0x%04x' % v for v in vals[i:i + per]) + ',')
        return '\n'.join(lines)

    def fmt_u8(vals, per=32):
        lines = []
        for i in range(0, len(vals), per):
            lines.append('    ' + ', '.join('0x%02x' % v for v in vals[i:i + per]) + ',')
        return '\n'.join(lines)

    hdr = f"""/* cst_tables.h — GEGENEREERD door re/gen_cst_tables.py — niet handmatig editten.
 *
 * FXM_CC colour-space-tabellen (Xerox/Fuji DocuPrint C525A / Phaser 6125N
 * 32-bit driver), reverse engineered uit de binary.  Byte-exact geverifieerd
 * tegen de originele FXM_CC output (re/verify_model.c).  Zie RE-NOTES.md §9.
 *
 *   CST_TRC   gamma 2.2 TRC, 256 x u16:  8-bit sRGB  -> 16-bit lineair
 *   CST_ITRC  inverse TRC, 4096 entries: 12-bit lineair -> 8-bit sRGB
 *   CST_M     3x3 matrix, signed Q16 fixed point (m * 65536, row-major).
 *             Pixel math (signed, wrapping): out = ((M[i]>>2)*v + ...) >> 18
 *             == m * v / 16  (16-bit lineair -> 12-bit).
 */
#ifndef CST_TABLES_H
#define CST_TABLES_H

#include <stdint.h>

/* gamma 2.2 TRC: CST_TRC[i] = round(65535 * (i/255)^2.2) — 8 -> 16 bit */
static const uint16_t CST_TRC[256] = {{
{fmt_u16(trc)}
}};

/* inverse TRC (XYZ2sRGBTrc): 12-bit lineair -> 8-bit */
static const uint8_t CST_ITRC[4096] = {{
{fmt_u8(itrc)}
}};

/* 3x3 matrix, signed Q16 (m * 65536), row-major — sRGB primaries + gamma22TRC.
 * m = XYZ2sRGBMatrix x sRGBprimaries (≈ identity; off-diagonals zijn klein
 * maar niet nul — de exacte waarden zijn nodig voor byte-exactheid). */
static const int32_t CST_M[9] = {{
    {M[0]}, {M[1]}, {M[2]},
    {M[3]}, {M[4]}, {M[5]},
    {M[6]}, {M[7]}, {M[8]}
}};

#endif /* CST_TABLES_H */
"""
    with open(OUT, 'w') as f:
        f.write(hdr)
    print('geschreven: %s (%d bytes)' % (OUT, len(hdr)))
    print('  CST_TRC[0..3]   =', trc[:4], ' CST_TRC[255] =', trc[255])
    print('  CST_ITRC[0..3]  =', list(itrc[:4]), ' CST_ITRC[4095] =', itrc[4095])
    print('  CST_M           =', M)
    return 0


if __name__ == '__main__':
    sys.exit(main())
