#!/usr/bin/env python3
"""sq21 bitstream decoder — reverse-engineered from Fuji FXM_ALC (Xerox Phaser 6125N /
DocuPrint C525-A) and cross-checked against Dave Coffin's hbpldecode.c (foo2zjs HBPLv1 "XRC").

The sq21/XRC raster codec (single interleaved MSB-first bitstream):
  tokens (HID[hsel], canonical Huffman, 2 rows):
    k=0..4 : run of `run` pixels copied from predictor order[k] (move-to-front)
    k=5    : diff — 4 byte-diffs (HPE0) added to the left pixel
    k=6    : switch hsel row  (code '11111' + one extra 0 bit)
  run length: 1..17057 (HRN0 short codes / '111'+3-bit group+extra)
  byte diff:  signed value via HPE0 (indexed by (cur-left)&0xff)
  predictor positions (semantic, per pixel):
    0=left(x-1,y)  1=up(x,y-1)  2=up-left(x-1,y-1)  3=up-right(x+1,y-1)  4=up+2right(x+2,y-1)
  out-of-image predictor positions read as 0xff,0xff,0xff,0xff (sq21 pixbuf margins;
  right-edge up-right/up+2right hit the 0xff sentinel slot — sq21 encoder semantics).
  bit bookkeeping: token index emitted = bdir - bit, bit = (prev run % 17057 != 0),
  bit = 0 after a diff and after each 17057-chunk.
"""
import struct
import sys

# HID[hsel][k] = (code, length)
HID = [
    [(0, 1), (6, 3), (28, 5), (29, 5), (30, 5), (2, 2), (31, 5)],
    [(2, 2), (6, 3), (28, 5), (29, 5), (30, 5), (0, 1), (31, 5)],
]
# build decode lookup: (length, code) -> k
HID_INV = []
for row in HID:
    d = {}
    for k, (c, l) in enumerate(row):
        d[(l, c)] = k
    HID_INV.append(d)


def build_hpe0():
    """HPE0[d] = (code, length) for byte-diff d = (cur-left)&0xff.
    Identical to Dave Coffin's put_diff()/get_diff() codebook."""
    tab = []
    groups = [(2, 0b011, 3, 1), (4, 0b100, 3, 2), (8, 0b101, 3, 3),
              (16, 0b110, 3, 5), (48, 0b1110, 4, 5), (80, 0b1111, 4, 6)]
    for d in range(256):
        val = d if d < 128 else d - 256
        if val == 0:
            tab.append((0, 3)); continue
        if val == 1:
            tab.append((1, 3)); continue
        if val == -1:
            tab.append((2, 3)); continue
        sign = 1 if val < 0 else 0
        a = abs(val)
        for base, prefix, plen, ebits in groups:
            if a < base + (1 << ebits):
                code = (prefix << (1 + ebits)) | (sign << ebits) | (a - base)
                tab.append((code, plen + 1 + ebits))
                break
    return tab


HPE0 = build_hpe0()
HPE0_INV = {}
for d, (c, l) in enumerate(HPE0):
    HPE0_INV[(l, c)] = d

RL_GROUP = [26, 34, 42, 50, 66, 98, 162, 674]
RL_ADDBITS = [3, 3, 3, 4, 5, 6, 9, 14]

MARGIN = b'\xff\xff\xff\xff'
# predictor id -> (dx, dy)
PRED = [(-1, 0), (0, -1), (-1, -1), (1, -1), (2, -1)]


class BitReader:
    def __init__(self, data):
        self.data = data
        self.pos = 0  # bit position

    def bit(self):
        b = (self.data[self.pos >> 3] >> (7 - (self.pos & 7))) & 1
        self.pos += 1
        return b

    def bits(self, n):
        v = 0
        for _ in range(n):
            v = (v << 1) | self.bit()
        return v

    def peek_bits_consumed(self):
        return self.pos


def read_token(br, hid):
    code = 0
    length = 0
    inv = HID_INV[hid]
    while True:
        code = (code << 1) | br.bit()
        length += 1
        k = inv.get((length, code))
        if k is not None:
            return k
        if length > 6:
            raise ValueError('bad HID token at bit %d' % br.pos)


def read_run(br):
    b1 = br.bit()
    if b1 == 0:
        if br.bit() == 0:
            return 1
        return 2 + br.bit()
    b2 = br.bit()
    if b2 == 0:
        if br.bit() == 0:
            return 4 + br.bit()
        return 6 + br.bits(2)
    if br.bit() == 0:
        return 10 + br.bits(4)
    g = br.bits(3)
    n = RL_ADDBITS[g]
    return RL_GROUP[g] + (br.bits(n) if n else 0)


def read_diff(br):
    code = 0
    length = 0
    while True:
        code = (code << 1) | br.bit()
        length += 1
        d = HPE0_INV.get((length, code))
        if d is not None:
            return d if d < 128 else d - 256
        if length > 11:
            raise ValueError('bad HPE0 code at bit %d' % br.pos)


def decode(payload, W, H):
    """Decode a single-stream sq21 bitstream into W*H 4-byte pixels (bytes)."""
    br = BitReader(payload)
    # rows: prev/cur as lists indexed x+1 (index 0 = left margin, W+1/W+2 = right sentinel/margin)
    prev = [MARGIN] * (W + 4)
    cur = [MARGIN] * (W + 4)
    img = bytearray(W * H * 4)
    hid = 0
    bit = 0
    order = [0, 1, 2, 3, 4]
    x = 0
    y = 0
    npix = 0
    ntokens = 0
    while npix < W * H:
        k = read_token(br, hid)
        ntokens += 1
        if k == 6:
            hid ^= 1
            br.bit()  # the extra 0 bit
            continue
        if k == 5:
            left = cur[x] if x > 0 else MARGIN
            px = bytearray(4)
            for j in range(4):
                px[j] = (left[j] + read_diff(br)) & 0xff
            cur[x + 1] = bytes(px)
            o = (y * W + x) * 4
            img[o:o + 4] = px
            x += 1
            npix += 1
            bit = 0
        else:
            run = read_run(br)
            bdir = k + bit
            bit = 1 if run < 17057 else 0
            pid = order[bdir]
            # move-to-front
            if bdir:
                order.pop(bdir)
                order.insert(0, pid)
            dx, dy = PRED[pid]
            for _ in range(run):
                if npix >= W * H:
                    break
                if dy == 0:
                    p = cur[x] if x > 0 else MARGIN
                else:
                    px_ = x + dx
                    p = prev[px_ + 1] if 0 <= px_ < W else MARGIN
                cur[x + 1] = p
                o = (y * W + x) * 4
                img[o:o + 4] = p
                x += 1
                npix += 1
                if x == W:
                    # row end: swap; right sentinel/margin = 0xff (sq21 semantics)
                    prev, cur = cur, [MARGIN] * (W + 4)
                    prev[W + 1] = MARGIN  # sentinel slot (fire_sentinel writes 0xff byte)
                    prev[W + 2] = MARGIN  # right margin
                    x = 0
                    y += 1
    return bytes(img), br.pos, ntokens


def main():
    if len(sys.argv) < 4:
        print('usage: sq21dec.py <fxr-or-raw-sq21> <W> <H> [offset] [out.raw]')
        print('  if the file starts with a 44-byte FXR header (f9==2), it is skipped')
        sys.exit(1)
    fn = sys.argv[1]
    W = int(sys.argv[2]); H = int(sys.argv[3])
    off = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    out = sys.argv[5] if len(sys.argv) > 5 else None
    d = open(fn, 'rb').read()
    if d[:4] != b'\x00\x00\x00\x00' and len(d) > 44:
        # heuristics: FXR header = 11 uint32 LE, f9 (index 9) == 2 → sq21
        hdr = struct.unpack_from('<11I', d, 0)
        if hdr[9] == 2:
            off = 44
            if W == 0:
                W = hdr[0]
            if H == 0:
                H = hdr[1]
            print('FXR header: w=%d h=%d bpp=%d f9=%d f10=%d -> payload at 44' % (hdr[0], hdr[1], hdr[2], hdr[9], hdr[10]))
    payload = d[off:]
    img, bits, ntok = decode(payload, W, H)
    print('decoded %d pixels (%d bytes) from %d payload bytes, %d tokens, %d bits consumed'
          % (W * H, len(img), len(payload), ntok, bits))
    if out:
        open(out, 'wb').write(img)
        print('wrote', out)
    # quick stats
    from collections import Counter
    c = Counter(img[i:i + 4] for i in range(0, min(len(img), 4096), 4))
    print('first pixels:', [img[i:i + 4].hex() for i in range(0, 16, 4)])
    print('most common:', c.most_common(5))


if __name__ == '__main__':
    main()
