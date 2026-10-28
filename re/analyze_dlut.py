#!/usr/bin/env python3
"""
analyze_dlut.py — parse driver/Phaser_6125N.dlut (Xerox/Fuji colour-calibration
table shipped with the Windows driver) and report the structure of every block.

Format (all big-endian), reverse engineered — see RE-NOTES.md §9:
  header : magic "DLUT", u32 count, count x u32 block-offsets,
           then metadata: u32 nvals, nvals x u32 values (until first block)
  block  : magic "dlt4", u32(0), 3 x 256-byte 1D curves (R,G,B),
           17*17*17 x 4-byte 3D LUT nodes (4913 nodes)

Usage: python3 re/analyze_dlut.py [path-to.dlut]
"""
import struct, sys, os, hashlib

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DLUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, 'driver', 'Phaser_6125N.dlut')

data = open(DLUT, 'rb').read()
print('file: %s (%d bytes)' % (DLUT, len(data)))
assert data[:4] == b'DLUT', 'bad magic'
count = struct.unpack('>I', data[4:8])[0]
offs = list(struct.unpack('>%dI' % count, data[8:8 + 4 * count]))
print('blocks: %d' % count)
print('offsets: %s' % offs)
print('block size: %d (uniform: %s)' % (offs[1] - offs[0],
      all(offs[i + 1] - offs[i] == offs[1] - offs[0] for i in range(count - 1))))
print('last block ends at %d (file %d, tail %d bytes)' %
      (offs[-1] + (offs[1] - offs[0]), len(data), len(data) - (offs[-1] + offs[1] - offs[0])))

meta = data[8 + 4 * count:offs[0]]
nvals = struct.unpack('>I', meta[:4])[0]
vals = list(struct.unpack('>%dI' % ((len(meta) - 4) // 4), meta[4:4 + (len(meta) - 4) // 4 * 4]))
print('metadata: %d bytes, nvals=%d, %d values follow' % (len(meta), nvals, len(vals)))
print('meta values: %s' % vals)

BLOCK = offs[1] - offs[0]
LUT_OFF = 8 + 3 * 256
LUT_N = 17 * 17 * 17 * 4

def node(lut, i, j, k):
    o = ((i * 17 + j) * 17 + k) * 4
    return lut[o:o + 4]

hashes = {}
for bi in range(count):
    blk = data[offs[bi]:offs[bi] + BLOCK]
    magic = blk[:4]
    zero = struct.unpack('>I', blk[4:8])[0]
    curves = [blk[8 + c * 256:8 + (c + 1) * 256] for c in range(3)]
    ident = [curves[c] == bytes(range(256)) for c in range(3)]
    lut = blk[LUT_OFF:LUT_OFF + LUT_N]
    h = hashlib.sha1(lut).hexdigest()[:10]
    hashes.setdefault(h, []).append(bi)

    # per byte-position stats
    stats = []
    for p in range(4):
        b = lut[p::4]
        stats.append((min(b), max(b), sum(b) / len(b)))
    # deviation from identity, assuming node(i,j,k) should be ~(16i,16j,16k,pad)
    dev = [0.0, 0.0, 0.0]
    for i in range(17):
        for j in range(17):
            for k in range(17):
                n = node(lut, i, j, k)
                dev[0] += abs(n[0] - min(16 * i, 255))
                dev[1] += abs(n[1] - min(16 * j, 255))
                dev[2] += abs(n[2] - min(16 * k, 255))
    dev = [d / 4913 for d in dev]

    # which byte position tracks which grid axis? (vary one axis, fix others mid)
    track = []
    for p in range(4):
        row = []
        for a in range(3):
            vals_a = []
            for c in range(17):
                idx = [8, 8, 8]
                idx[a] = c
                vals_a.append(node(lut, *idx)[p])
            # does byte p depend only on axis a? check constancy when other axes vary
            dep_only = True
            base = None
            for i in range(17):
                for j in range(17):
                    for k in range(17):
                        idx = (i, j, k)
                        v = node(lut, *idx)[p]
                        c = idx[a]
                        if base is None:
                            pass
            # simpler: correlation between byte p and 16*coord_a over all nodes
            sxy = sxx = syy = sx = sy = 0.0
            for i in range(17):
                for j in range(17):
                    for k in range(17):
                        x = 16 * (i, j, k)[a]
                        y = node(lut, i, j, k)[p]
                        sxy += x * y; sxx += x * x; syy += y * y; sx += x; sy += y
            n = 4913.0
            denom = (n * sxx - sx * sx) * (n * syy - sy * sy)
            r = (n * sxy - sx * sy) / denom ** 0.5 if denom > 0 else 0.0
            row.append(round(r, 3))
        track.append(row)

    print('block %2d: magic=%s zero=%d curves identity=%s lut-sha=%s' %
          (bi, magic.decode('latin1'), zero, ident, h))
    print('         byte[min,max,mean]: ' +
          '  '.join('b%d[%d,%d,%.1f]' % (p, stats[p][0], stats[p][1], stats[p][2]) for p in range(4)))
    print('         mean|dev from identity| per byte: %s' % [round(d, 2) for d in dev])
    print('         corr(byte p, 16*axis a) p\\a: ' +
          '  '.join('b%d:%s' % (p, track[p]) for p in range(4)))
    if bi in (0, 6, 11):
        print('         node(0,0,0)=%s node(16,0,0)=%s node(0,16,0)=%s node(0,0,16)=%s node(16,16,16)=%s'
              % (tuple(node(lut, 0, 0, 0)), tuple(node(lut, 16, 0, 0)),
                 tuple(node(lut, 0, 16, 0)), tuple(node(lut, 0, 0, 16)),
                 tuple(node(lut, 16, 16, 16))))

print('identical 3D LUT groups (sha -> blocks):')
for h, blocks in hashes.items():
    if len(blocks) > 1:
        print('  %s -> blocks %s' % (h, blocks))
print('distinct 3D LUTs: %d of %d' % (len(hashes), count))
