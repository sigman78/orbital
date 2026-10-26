"""Lossless compression of a baked store's planes, raw against predicted (docs/TILE_COMPRESSION.md).

Usage: python -I tools/tile-compression-study.py [level] [store directory], from the repo root.
zlib stands in for zstd unless the zstandard module is installed; needs numpy."""
import struct, zlib, sys
import numpy as np
try:
    import zstandard
    def comp(b): return len(zstandard.ZstdCompressor(level=9).compress(b)), 'zstd9'
except ImportError:
    def comp(b): return len(zlib.compress(b, 9)), 'zlib9'

store = sys.argv[2] if len(sys.argv) > 2 else 'assets/planets/minor-planet'
SIDE, CSIDE = 33, 65
H_BYTES = SIDE * SIDE * 2
A_OFF, S_OFF = 2180, 19080
C_BYTES = CSIDE * CSIDE * 4

def shard(face, level):
    with open(f'{store}/f{face}_l{level:02}.tiles', 'rb') as f:
        data = f.read()
    (magic, ver, cooker, fc, lv, count, tbytes, res, seed, payload, index) = struct.unpack_from('<8I3Q', data, 0)
    tiles = [data[payload + i * tbytes: payload + (i + 1) * tbytes] for i in range(count)]
    return tiles

def heights(tile):
    return np.frombuffer(tile[:H_BYTES], dtype='<u2').reshape(SIDE, SIDE).astype(np.int32)

def to_bytes(residual):
    # zigzag-free: store as uint16 modulo, LZ sees small values either way
    return (residual & 0xffff).astype('<u2').tobytes()

def pred_left(h):
    r = h.copy(); r[:, 1:] -= h[:, :-1]; return r

def pred_med(h):
    # MED/LOCO-I predictor as in JPEG-LS
    r = h.copy()
    for y in range(SIDE):
        for x in range(SIDE):
            a = h[y, x - 1] if x else 0
            b = h[y - 1, x] if y else 0
            c = h[y - 1, x - 1] if x and y else 0
            if x == 0 and y == 0: p = 0
            elif x == 0: p = b
            elif y == 0: p = a
            else:
                p = min(a, b) if c >= max(a, b) else max(a, b) if c <= min(a, b) else a + b - c
            r[y, x] = h[y, x] - p
    return r

def parent_upsample(parent, quadrant):
    # the child's 33x33 covers one quadrant of the parent's 33x33 (16 parent cells -> 32 child cells)
    qx, qy = quadrant & 1, quadrant >> 1
    sub = parent[qy * 16: qy * 16 + 17, qx * 16: qx * 16 + 17].astype(np.float64)
    up = np.zeros((SIDE, SIDE))
    up[::2, ::2] = sub
    up[1::2, ::2] = (sub[:-1, :] + sub[1:, :]) / 2
    up[::2, 1::2] = (sub[:, :-1] + sub[:, 1:]) / 2
    up[1::2, 1::2] = (sub[:-1, :-1] + sub[1:, 1:] + sub[1:, :-1] + sub[:-1, 1:]) / 4
    return np.rint(up).astype(np.int32)

level = int(sys.argv[1]) if len(sys.argv) > 1 else 5
face = 3
tiles = shard(face, level)
parents = shard(face, level - 1)
n = min(len(tiles), 512)
side = 1 << level
raw = b''.join(t[:H_BYTES] for t in tiles[:n])
left = b''.join(to_bytes(pred_left(heights(t))) for t in tiles[:n])
med = b''.join(to_bytes(pred_med(heights(t))) for t in tiles[:n])
par = []
for i in range(n):
    x, y = i % side, i // side
    p = heights(parents[(y // 2) * (side // 2) + x // 2])
    par.append(to_bytes(heights(tiles[i]) - parent_upsample(p, (x & 1) | (y & 1) << 1)))
par = b''.join(par)
alb = b''.join(t[A_OFF:A_OFF + C_BYTES] for t in tiles[:n])
slp = b''.join(t[S_OFF:S_OFF + C_BYTES] for t in tiles[:n])

name = comp(b'x')[1]
print(f'level {level}, face {face}, {n} tiles, compressor {name}')
for label, b in [('heights raw', raw), ('heights left delta', left), ('heights MED predictor', med),
                 ('heights parent residual', par), ('albedo rgba8 raw', alb), ('slope rg16 raw', slp)]:
    c, _ = comp(b)
    print(f'  {label:26s} {len(b)/n:8.0f} B/tile -> {c/n:8.0f} B/tile  ratio {len(b)/c:5.2f}')
res = np.frombuffer(par, dtype='<u2').astype(np.int32); res = np.where(res > 32767, res - 65536, res)
print(f'  parent residual |max| {np.abs(res).max()} steps, 99.9% {np.percentile(np.abs(res), 99.9):.0f} steps (step = 1.5e-6 radii)')
