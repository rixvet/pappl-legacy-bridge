#!/usr/bin/env python3
"""Generate tests/colorpage.ps — a colour/image torture test page for the
Phaser 6125N native driver.

Contents (A4, 595x842 pt):
  - title + 7 lines of coloured text
  - 8 CMYK-ish colour bars
  - 256x256 RGB rainbow gradient (embedded hex image via currentfile/ASCIIHexDecode)
  - 128x128 random noise patch (worst case for the sq21 diff encoder / HID switching)
  - fine 4px checkerboard (edge-case: high-frequency alternating pixels)
  - thin horizontal lines in rotating hues
"""
import random

W, H = 595, 842
out = []

def emit(s=''):
    out.append(s)

def hexdata(raw, per_line=76):
    h = raw.hex()
    lines = [h[i:i + per_line] for i in range(0, len(h), per_line)]
    return '\n'.join(lines)

emit('%!PS-Adobe-3.0')
emit('%%Pages: 1')
emit('%%BoundingBox: 0 0 %d %d' % (W, H))
emit('%%EndComments')
emit('%%Page: 1 1')
emit('')

# ---------- title + coloured text ----------
emit('% --- title ---')
emit('/Helvetica-Bold findfont 24 scalefont setfont')
emit('0 setgray 40 %d moveto (Kleurtest Phaser 6125N - native driver) show' % (H - 50))
emit('')
emit('% --- coloured text lines ---')
colors = [('1 0 0', 'RED'), ('0 0.6 0', 'GREEN'), ('0 0 1', 'BLUE'),
          ('0 1 1', 'CYAN'), ('1 0 1', 'MAGENTA'), ('0.85 0.65 0', 'YELLOW'),
          ('0 0 0', 'BLACK')]
emit('/Helvetica findfont 18 scalefont setfont')
for i, (c, name) in enumerate(colors):
    y = H - 80 - i * 24   # keeps the BLACK line clear of the bars (top @522+80=602)
    emit('%s setrgbcolor 40 %d moveto (%s) show' % (c, y, name))
emit('')

# ---------- colour bars ----------
emit('% --- 8 colour bars ---')
bars = [(1, 0, 0), (1, 1, 0), (0, 1, 0), (0, 1, 1),
        (0, 0, 1), (1, 0, 1), (0, 0, 0), (1, 1, 1)]
bw = (W - 80) / 8.0
y0, bh = H - 320, 80
for i, (r, g, b) in enumerate(bars):
    emit('%g %g %g setrgbcolor' % (r, g, b))
    emit('%g %d %g %d rectfill' % (40 + i * bw, y0, bw, bh))
# outline the white bar so it is visible
emit('0 setgray 0.5 setlinewidth %d %d %g %d rectstroke' % (40, y0, W - 80, bh))
emit('')

# ---------- gradient image 256x256 ----------
emit('% --- 256x256 rainbow gradient image ---')
N = 256
grad = bytearray()
for yy in range(N):
    for xx in range(N):
        grad += bytes((xx, yy, (xx + yy) // 2))
emit('gsave 40 %d translate 220 220 scale' % (y0 - 260))
emit('%d %d 8 [%d 0 0 -%d 0 %d]' % (N, N, N, N, N))
emit('currentfile /ASCIIHexDecode filter')
emit('false 3 colorimage')
emit(hexdata(bytes(grad)))
emit('>')
emit('grestore')
emit('')

# ---------- noise image 128x128 ----------
emit('% --- 128x128 random noise (worst-case diff encoding) ---')
rng = random.Random(6125)
M = 128
noise = bytes(rng.randrange(256) for _ in range(M * M * 3))
emit('gsave 320 %d translate 220 220 scale' % (y0 - 260))
emit('%d %d 8 [%d 0 0 -%d 0 %d]' % (M, M, M, M, M))
emit('currentfile /ASCIIHexDecode filter')
emit('false 3 colorimage')
emit(hexdata(noise))
emit('>')
emit('grestore')
emit('')

# ---------- meahow the vibe cat (pixel art, 32x32) ----------
emit('% --- meahow the vibe cat (32x32 pixel art) ---')
N = 32
grid = [['.'] * N for _ in range(N)]


def rect(x0, y0, x1, y1, c):
    for yy in range(y0, y1 + 1):
        for xx in range(x0, x1 + 1):
            grid[yy][xx] = c


# ears (orange) + inner ears (pink)
for r, (xl, xr) in enumerate([(8, 9), (7, 10), (6, 11), (6, 11), (6, 11)]):
    rect(xl, 2 + r, xr, 2 + r, 'O')
    rect(31 - xr, 2 + r, 31 - xl, 2 + r, 'O')
rect(8, 3, 9, 3, 'P'); rect(22, 3, 23, 3, 'P')
rect(7, 4, 10, 4, 'P'); rect(21, 4, 24, 4, 'P')
rect(8, 5, 9, 5, 'P'); rect(22, 5, 23, 5, 'P')
# head (rounded)
rect(8, 6, 23, 6, 'O'); rect(6, 7, 25, 7, 'O')
rect(5, 8, 26, 21, 'O')
rect(6, 22, 25, 22, 'O'); rect(8, 23, 23, 23, 'O')
# forehead + side stripes (dark orange)
rect(13, 6, 14, 6, 'D'); rect(17, 6, 18, 6, 'D')
rect(14, 7, 14, 8, 'D'); rect(17, 7, 17, 8, 'D')
rect(5, 12, 6, 14, 'D'); rect(25, 12, 26, 14, 'D')
# eyes (black with white shine)
rect(10, 11, 12, 13, 'K'); rect(19, 11, 21, 13, 'K')
grid[11][10] = 'W'; grid[11][19] = 'W'
# nose (pink) + mouth (black)
rect(15, 16, 16, 16, 'P')
rect(13, 17, 14, 17, 'K'); rect(17, 17, 18, 17, 'K')
# whiskers (black)
rect(1, 14, 5, 14, 'K'); rect(26, 14, 30, 14, 'K')
rect(1, 15, 5, 15, 'K'); rect(26, 15, 30, 15, 'K')
rect(2, 16, 5, 16, 'K'); rect(26, 16, 29, 16, 'K')
# white cheeks
rect(6, 18, 8, 20, 'W'); rect(23, 18, 25, 20, 'W')
# chubby body + white belly
rect(11, 24, 20, 24, 'O')
rect(10, 25, 21, 28, 'O')
rect(13, 25, 18, 27, 'W')
# paws: light gray fill with a black outline (reads well on the white page)
rect(10, 29, 13, 30, 'L'); rect(18, 29, 21, 30, 'L')
grid[30][11] = 'K'; grid[30][12] = 'K'      # toe lines
grid[30][19] = 'K'; grid[30][20] = 'K'
rect(9, 29, 9, 31, 'K'); rect(14, 29, 14, 31, 'K'); rect(9, 31, 14, 31, 'K')   # left paw outline
rect(17, 29, 17, 31, 'K'); rect(22, 29, 22, 31, 'K'); rect(17, 31, 22, 31, 'K') # right paw outline
# tail curling up on the right, dark tip
rect(22, 25, 23, 25, 'O'); rect(23, 26, 24, 26, 'O')
rect(24, 27, 25, 27, 'O'); rect(25, 28, 26, 28, 'O')
rect(25, 29, 26, 30, 'D'); rect(24, 30, 25, 30, 'D')

PAL = {'.': (255, 255, 255), 'O': (255, 165, 0), 'D': (204, 102, 0),
       'K': (30, 30, 30), 'W': (255, 255, 255), 'P': (255, 130, 170),
       'L': (235, 235, 235)}
cat = bytearray()
for row in grid:
    for ch in row:
        cat += bytes(PAL[ch])
emit('gsave 400 630 translate 140 140 scale')
emit('%d %d 8 [%d 0 0 -%d 0 %d]' % (N, N, N, N, N))
emit('currentfile /ASCIIHexDecode filter')
emit('false 3 colorimage')
emit(hexdata(bytes(cat)))
emit('>')
emit('grestore')
emit('/Helvetica findfont 12 scalefont setfont')
emit('0 setgray 452 610 moveto (meahow) show')
emit('')

# ---------- fine checkerboard ----------
emit('% --- 4px checkerboard 64x32 cells ---')
# NB: cell = 1x1 user unit, so scale must be (220/64, 60/32) — NOT (220, 60),
# otherwise every cell is 220x60 pt and the checker covers the whole page.
emit('gsave 40 90 translate 220 64 div 60 32 div scale')
emit('0.5 setlinewidth')
emit('0 1 31 { /iy exch def 0 1 63 { /ix exch def')
emit('  ix 4 idiv 2 mod iy 4 idiv 2 mod add 1 eq')
emit('  { 0 0 0 setrgbcolor } { 1 1 1 setrgbcolor } ifelse')
emit('  ix iy 1 1 rectfill } for } for')
emit('grestore')
emit('')

# ---------- thin hue lines ----------
emit('% --- thin horizontal lines, rotating hue ---')
emit('0.75 setlinewidth')
for i in range(8):
    hue = i / 8.0
    # simple HSV->RGB with s=v=1
    import colorsys
    r, g, b = colorsys.hsv_to_rgb(hue, 1.0, 1.0)
    y = 24 + i * 8   # stays clear of the checkerboard (@90..150) and the 11.28pt margin
    emit('%g %g %g setrgbcolor 40 %d moveto %d %d lineto stroke' % (r, g, b, y, W - 40, y))
emit('')
emit('showpage')
emit('%%EOF')

with open('tests/colorpage.ps', 'w') as f:
    f.write('\n'.join(out) + '\n')
print('wrote tests/colorpage.ps (%d bytes)' % len('\n'.join(out)))
