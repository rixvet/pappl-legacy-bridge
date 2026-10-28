#!/usr/bin/env python3
"""Verify the native foo2sq21hbpl driver output end-to-end.

For a given input + options:
  1. run the native driver -> out.hbpl
  2. parse the HBPL structure (PJL header, A record, per-page C/QR/body/payload/SD,
     final trailer) and check it against the byte-exact template
  3. render the same input with gs standalone (same args the driver uses)
  4. for every page: apply the PM2FXR crop rule, expand to 4-byte pixels,
     pad to a multiple of 8, and compare with the DECODED sq21 payload
     (must be byte-exact: the encoder is lossless)

Colour calibration: with FOO2SQ_CALIBRATE on (the default) the driver applies
the FXM_CC colour transform to every pixel before encoding.  The expected
raster is then computed with the oracle re/calib.c (byte-exact model of the
original 32-bit FXM_CC, proven against its ground truth): color pixels go
through 3Dlut(CST(...)), gray pixels through NTSC(CST(...)).  With
FOO2SQ_CALIBRATE=0 the driver is raw (or gray-averaged) and the expected
raster is the unmodified gs output.

Usage: verify-native.py <input.ps|pdf> "<options string>"
"""
import struct, subprocess, sys, os, math, re, ctypes, time
from concurrent.futures import ThreadPoolExecutor

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, REPO + '/re')
# Fast C decoder/expander (re/sq21dec.c, shared lib built on demand);
# falls back to the pure-Python sq21dec module if it cannot be built/loaded.
_CLIB = None


def _clib():
    global _CLIB
    if _CLIB is not None:
        return _CLIB or None
    path = REPO + '/re/libsq21dec.so'
    if not os.path.exists(path):
        subprocess.run(['gcc', '-O2', '-shared', '-fPIC', '-o', path,
                        REPO + '/re/sq21dec.c'], check=True, capture_output=True)
    try:
        lib = ctypes.CDLL(path)
        lib.sq21_decode.restype = ctypes.c_int
        lib.sq21_decode.argtypes = [ctypes.c_char_p, ctypes.c_size_t,
                                    ctypes.c_int, ctypes.c_int,
                                    ctypes.c_char_p, ctypes.c_size_t,
                                    ctypes.POINTER(ctypes.c_size_t),
                                    ctypes.POINTER(ctypes.c_long)]
        lib.sq21_expand.restype = ctypes.c_int
        lib.sq21_expand.argtypes = ([ctypes.c_char_p] + [ctypes.c_int] * 8 +
                                    [ctypes.c_char_p, ctypes.c_size_t])
        _CLIB = lib
    except OSError:
        _CLIB = False
    return _CLIB or None


def decode_page(payload, W, H):
    """-> (raw W*H*4 bytes, bits consumed, token count)"""
    lib = _clib()
    if lib:
        buf = ctypes.create_string_buffer(W * H * 4)
        bits = ctypes.c_size_t(0)
        ntok = ctypes.c_long(0)
        rc = lib.sq21_decode(payload, len(payload), W, H, buf, W * H * 4,
                             ctypes.byref(bits), ctypes.byref(ntok))
        if rc != 0:
            raise RuntimeError('sq21_decode failed rc=%d' % rc)
        return buf.raw, bits.value, ntok.value
    import sq21dec
    return sq21dec.decode(payload, W, H)


def expand_page(gpix, gw, gh, cx, cy, cw, ch, W4, gray):
    """expected raster: crop + ffRGB expand + pad to W4 (raw ch*W4*4 bytes)"""
    lib = _clib()
    if lib:
        buf = ctypes.create_string_buffer(ch * W4 * 4)
        rc = lib.sq21_expand(gpix, gw, gh, cx, cy, cw, ch, W4,
                             1 if gray else 0, buf, ch * W4 * 4)
        if rc != 0:
            raise RuntimeError('sq21_expand failed rc=%d' % rc)
        return buf.raw
    exp = bytearray()
    for y in range(ch):
        grow = gpix[((cy + y) * gw + cx) * 3: ((cy + y) * gw + cx) * 3 + cw * 3]
        row = bytearray()
        for x in range(cw):
            s = grow[x * 3:x * 3 + 3]
            if gray:
                g = (s[0] + s[1] + s[2]) // 3
                s = bytes([g, g, g])
            row += b'\xff' + s
        for x in range(cw, W4):
            row += b'\xff\xff\xff\xff'
        exp += row
    return bytes(exp)

_CALIB = None


def _caliblib():
    """Oracle voor de FXM_CC calibratie (re/calib.c, shared lib)."""
    global _CALIB
    if _CALIB is not None:
        return _CALIB or None
    path = REPO + '/re/libcalib.so'
    if not os.path.exists(path):
        subprocess.run(['gcc', '-O2', '-shared', '-fPIC', '-DCALIB_NO_MAIN',
                        '-o', path, REPO + '/re/calib.c'],
                       check=True, capture_output=True)
    try:
        lib = ctypes.CDLL(path)
        lib.calib_load.restype = ctypes.c_int
        lib.calib_load.argtypes = [ctypes.c_char_p, ctypes.c_int]
        lib.calib_color.restype = None
        lib.calib_color.argtypes = [ctypes.c_char_p, ctypes.c_char_p,
                                    ctypes.c_size_t]
        lib.calib_gray.restype = None
        lib.calib_gray.argtypes = [ctypes.c_char_p, ctypes.c_char_p,
                                   ctypes.c_size_t]
        _CALIB = lib
    except OSError:
        _CALIB = False
    return _CALIB or None


DRIVER = REPO + '/native/foo2sq21hbpl'
PPD = REPO + '/driver/Phaser_6125N.ppd'


def ppd_lookup(path, key, what):
    pat = ('*%s %s/' % (what, key)).encode()
    for line in open(path, 'rb').read().split(b'\n'):
        if not line.startswith(pat):
            continue
        q = line.split(b':', 1)[1]
        s = q.split(b'"')[1]
        return [float(x) for x in s.split()]
    return None


def parse_opts(opts):
    d = {}
    for tok in opts.split():
        if '=' in tok:
            k, v = tok.split('=', 1)
            d[k] = v
    return d


A_REC = bytes([0x41,0x81,0xa1,0x00,0x82,0xa2,0x01,0x00,0x83,0xa2,0x01,0x00])
BODY  = bytes([0x18,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x10,0x32,0x04,0x00,
               0xa1,0x42,0x00,0x00,0x00,0x00,0xff,0x00,0x00,0x00,0x00,0x00])
QR_PFX = bytes([0x51,0x52,0xa3,0xa1,0x00,0xa4,0xb1,0xa4])
C_PFX  = bytes([0x43,0x91,0xa1,0x00,0x92,0xa1])


def parse_hbpl(data):
    e = data.find(b'@PJL ENTER LANGUAGE=HBPL\n')
    assert e >= 0, 'no ENTER LANGUAGE'
    e += len(b'@PJL ENTER LANGUAGE=HBPL\n')
    pjl = data[:e]
    assert data[e:e+12] == A_REC, 'bad A record'
    e += 12
    pages = []
    while data[e:e+6] == C_PFX:
        c = data[e:e+78]
        marg = struct.unpack_from('<HH', c, 24)
        pageno = struct.unpack_from('<I', c, 33)[0]
        w, h = struct.unpack_from('<II', c, 39)
        w2, h2 = struct.unpack_from('<II', c, 70)
        assert (w, h) == (w2, h2), '9a/a2 mismatch'
        assert data[e+78:e+86] == QR_PFX, 'bad QR record'
        total = struct.unpack_from('<I', data, e+78+8)[0]
        body = data[e+90:e+114]
        assert body == BODY, 'bad body'
        payload = data[e+114:e+114+total-24]
        assert len(payload) == total - 24, 'payload truncated'
        assert data[e+114+total-24:e+114+total-24+2] == b'SD', 'missing SD'
        pages.append(dict(pageno=pageno, w=w, h=h, marg=marg, total=total, payload=payload))
        e += 114 + total - 24 + 2
    trailer = data[e:]
    return pjl, pages, trailer


def gs_render(ppd_dims, dpi, inp):
    r = '%d' % dpi
    wpt = '-dDEVICEWIDTHPOINTS=%d' % int(ppd_dims[0])
    hpt = '-dDEVICEHEIGHTPOINTS=%d' % int(ppd_dims[1])
    p = subprocess.run(['gs', '-dQUIET', '-dPARANOIDSAFER', '-dNOPAUSE', '-dBATCH',
                        '-dNOMEDIAATTRS', '-sDEVICE=ppmraw', '-sstdout=%stderr',
                        '-sOUTPUTFILE=%stdout', '-r' + r, wpt, hpt, inp],
                       capture_output=True)
    assert p.returncode == 0, 'gs failed: %s' % p.stderr[:300]
    # split into P6 images
    imgs = []
    d = p.stdout
    i = 0
    while i < len(d):
        assert d[i:i+2] == b'P6', 'bad P6 at %d' % i
        j = i + 2
        vals = []
        while len(vals) < 3:
            while j < len(d) and d[j:j+1].isspace():
                j += 1
            if d[j:j+1] == b'#':
                while d[j] != 0x0a:
                    j += 1
                continue
            k = j
            while k < len(d) and not d[k:k+1].isspace():
                k += 1
            vals.append(int(d[j:k]))
            j = k
        j += 1
        w, h, mx = vals
        imgs.append((w, h, d[j:j+w*h*3]))
        i = j + w*h*3
    return imgs


def main():
    inp = sys.argv[1]
    opts = sys.argv[2] if len(sys.argv) > 2 else \
        'PageSize=A4 FXOutputMode=600x600 FXColorMode=Color InstalledMemory=64Meg Option1=1Tray-S FXInputSlot=AutoSelect'
    o = parse_opts(opts)
    pagesize = o.get('PageSize', o.get('PageRegion', 'A4'))
    dpi = int(o.get('FXOutputMode', '600x600').split('x')[0])
    gray = o.get('FXColorMode', 'Color') != 'Color'

    # colour calibration — mirrors the driver (FOO2SQ_CALIBRATE, default
    # aan; FXDocumentType -> FXDLUT block code uit de PPD setpagedevice)
    ce = os.environ.get('FOO2SQ_CALIBRATE', '')
    calibrate = not (ce == '0' or ce.lower() in ('no', 'off'))
    fxdlut = 24   # FXDocumentType Normal
    dt = o.get('FXDocumentType')
    if dt == 'Photo':
        fxdlut = 22
    elif dt == 'Presentation':
        fxdlut = 26
    elif dt == 'Web':
        fxdlut = 23
    elif dt == 'POP':
        fxdlut = 32

    pd = ppd_lookup(PPD, pagesize, 'PaperDimension')
    ia = ppd_lookup(PPD, pagesize, 'ImageableArea')  # left bottom right top (pt)
    r = dpi / 72.0
    x0 = round(ia[0] * r); x1 = round(ia[2] * r)
    img_w = x1 - x0

    # 1. run the driver
    env = dict(os.environ, PPD=PPD)
    env.setdefault('FOO2SQ_DLUT', REPO + '/driver/Phaser_6125N.dlut')
    p = subprocess.run([DRIVER, '1', 'rick', 'VerifyTest', '1', opts, inp],
                       capture_output=True, env=env)
    assert p.returncode == 0, 'driver failed: %s' % p.stderr[:500]
    out = p.stdout
    print('driver: %d bytes, %d pages, stderr=%r' % (len(out), out.count(b'\x99\xa4'), p.stderr[:100]))

    # 2. structure
    pjl, pages, trailer = parse_hbpl(out)
    assert trailer == b'B\x1b%-12345X@PJL EOJ\n', 'bad trailer %r' % trailer
    assert b'@PJL SET RESOLUTION=%d\n' % dpi in pjl, 'bad RESOLUTION'
    assert b'@PJL SET JOBATTR="@DRDM=RASTER"' in pjl, 'bad DRDM'
    assert b'@PJL SET RENDERMODE=COLOR' in pjl, 'bad RENDERMODE'
    print('structure: PJL %d bytes, %d pages, trailer OK' % (len(pjl), len(pages)))

    # 3. gs reference render
    imgs = gs_render(pd, dpi, inp)
    assert len(imgs) == len(pages), 'page count %d != gs pages %d' % (len(pages), len(imgs))

    # 4. per page: crop + expand + pad, compare with decoded payload.
    #    Decode + expand run in C (libsq21dec); every page is an independent
    #    sq21 stream, so pages are verified in parallel threads (the ctypes
    #    calls release the GIL).
    plans = []
    for n, (pg, (gw, gh, gpix)) in enumerate(zip(pages, imgs)):
        y0 = gh - round(ia[3] * r)
        y1 = gh - round(ia[1] * r)
        img_h = y1 - y0
        if gw <= img_w and gh <= img_h:
            cx, cy, cw, ch = 0, 0, gw, gh
        else:
            cx = min(max(x0, 0), gw); cy = min(max(y0, 0), gh)
            cw = min(img_w, gw - cx); ch = min(img_h, gh - cy)
        W4 = (cw + 7) & ~7
        assert (pg['w'], pg['h']) == (W4, ch), \
            'page %d dims %dx%d != expected %dx%d' % (n, pg['w'], pg['h'], W4, ch)
        assert pg['pageno'] == n + 1, 'pageno %d != %d' % (pg['pageno'], n + 1)
        plans.append((n, pg, gw, gh, gpix, cx, cy, cw, ch, W4))

    # oracle voor de gekalibreerde verwachte raster (eenmalig laden; de
    # LUT is read-only dus thread-safe voor de pool hieronder)
    calib = None
    if calibrate:
        calib = _caliblib()
        if calib is None:
            print('WAARSCHUWING: re/libcalib.so niet beschikbaar — '
                  'gekakalibreerde pixels worden NIET geverifieerd')
        elif calib.calib_load(env['FOO2SQ_DLUT'].encode(), fxdlut) != 0 and not gray:
            print('WAARSCHUWING: oracle dlut %s niet bruikbaar (CST-only)' %
                  env['FOO2SQ_DLUT'])
    if calibrate:
        mode = ('gray NTSC(CST)' if gray else
                'color 3Dlut(CST) fxdlut=%d' % fxdlut)
    else:
        mode = 'gray average' if gray else 'raw'
    print('calibration: %s' % mode)

    def work(pl):
        n, pg, gw, gh, gpix, cx, cy, cw, ch, W4 = pl
        dec, bits, ntok = decode_page(pg['payload'], W4, ch)
        if calibrate and calib is not None:
            npix = gw * gh
            if gray:
                # driver gray: NTSC(CST(in)) -> [g,g,g] per pixel
                gbuf = ctypes.create_string_buffer(npix)
                calib.calib_gray(gpix, gbuf, npix)
                trip = bytearray(npix * 3)
                trip[0::3] = gbuf.raw
                trip[1::3] = gbuf.raw
                trip[2::3] = gbuf.raw
                exp = expand_page(bytes(trip), gw, gh, cx, cy, cw, ch, W4, 0)
            else:
                # driver color: 3Dlut(CST(in))
                cbuf = ctypes.create_string_buffer(npix * 3)
                calib.calib_color(gpix, cbuf, npix)
                exp = expand_page(cbuf.raw, gw, gh, cx, cy, cw, ch, W4, 0)
        else:
            exp = expand_page(gpix, gw, gh, cx, cy, cw, ch, W4, gray)
        return n, exp == dec, ntok, len(pg['payload']), dec, exp, (cx, cy, cw, ch, W4)

    t0 = time.time()
    with ThreadPoolExecutor(max_workers=min(4, len(plans))) as ex:
        results = sorted(ex.map(work, plans))
    for n, ok, ntok, plen, dec, exp, (cx, cy, cw, ch, W4) in results:
        print('  page %d: %dx%d (crop @(%d,%d) %dx%d) payload %d bytes, %d tokens, '
              'roundtrip %s' % (n + 1, W4, ch, cx, cy, cw, ch, plen, ntok,
                                'EXACT' if ok else 'MISMATCH'))
        if not ok:
            bad = [i for i in range(0, len(dec), 4) if dec[i:i+4] != exp[i:i+4]]
            print('    %d mismatching pixels, first at (%d,%d)' %
                  (len(bad), (bad[0] // 4) % W4, (bad[0] // 4) // W4))
            return 1
    print('  (decode+compare took %.1fs)' % (time.time() - t0))
    print('ALL OK: structure + lossless round-trip verified for %d page(s)' % len(pages))
    return 0


if __name__ == '__main__':
    sys.exit(main())
