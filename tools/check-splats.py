"""Validate exported records and cell-table integrity without changing them.

Checks that a baked splat (.bin) file's records and cell lookup table are internally
consistent: normalized centre/axis directions, positive and correctly ordered sigmas,
monotonically non-decreasing energy, a reach cutoff matching the stored sigma, and a
cell table whose per-cell member lists include every splat that could actually reach
that cell. Ported from the peer machine's splat-baking experiments (2026-09-13).

Usage: python tools/check-splats.py <records.bin> [<records2.bin> ...]
"""
import json
from pathlib import Path
import struct
import sys

import numpy as np

for filename in sys.argv[1:]:
    path = Path(filename)
    data = path.read_bytes()
    magic, count, stride, table_length = struct.unpack_from('<4sIII', data)
    assert magic == b'SPLT' and stride == 64
    assert len(data) == 16 + count * stride + table_length * 4
    records = np.frombuffer(data, '<f4', count * 16, 16).reshape(count, 16)
    table = np.frombuffer(data, '<u4', table_length, 16 + count * stride)
    assert np.isfinite(records).all()
    centre, axis = records[:, :3], records[:, 4:7]
    sigma = records[:, [3, 7]]
    assert np.allclose(np.linalg.norm(centre, axis=1), 1, atol=1e-5)
    assert np.allclose(np.linalg.norm(axis, axis=1), 1, atol=1e-5)
    assert np.max(np.abs((centre * axis).sum(axis=1))) < 1e-5
    assert (sigma > 0).all()
    assert (records[:, 12:] == 0).all()
    energy = np.abs(records[:, 8:11]).max(axis=1) * sigma.prod(axis=1)
    assert (np.diff(energy) <= 1e-6).all()
    reach = np.cos(np.minimum(3.5 * sigma.max(axis=1), np.pi - 1e-3))
    assert np.allclose(records[:, 11], reach, atol=1e-6)
    nx, ny = map(int, table[:2])
    assert nx > 0 and ny > 0 and nx == 2 * ny
    cells = table[2:2 + 2 * nx * ny].reshape(-1, 2)
    indices = table[2 + 2 * nx * ny:]
    assert (indices < count).all()
    assert cells[0, 0] == 0
    assert np.array_equal(cells[1:, 0], (cells[:, 0] + cells[:, 1])[:-1])
    assert int(cells[-1].sum()) == len(indices)
    half_diagonal = .5 * np.hypot(2 * np.pi / nx, np.pi / ny)
    reach_angle = np.arccos(np.clip(records[:, 11], -1, 1))
    for k, (offset, size) in enumerate(cells):
        lat = (.5 - (k // nx + .5) / ny) * np.pi
        lon = (.5 - (k % nx + .5) / nx) * 2 * np.pi
        direction = np.array([np.cos(lat) * np.cos(lon), np.cos(lat) * np.sin(lon), np.sin(lat)])
        angle = np.arccos(np.clip(centre @ direction, -1, 1))
        members = indices[int(offset):int(offset + size)]
        required = np.flatnonzero(angle < reach_angle + half_diagonal - 1e-6)
        assert np.isin(required, members).all()
    print(json.dumps({'file': path.name, 'validated': True, 'count': count,
                      'bytes': len(data), 'list_entries': len(indices),
                      'cell_mean': round(float(cells[:, 1].mean()), 2),
                      'cell_max': int(cells[:, 1].max()),
                      'sigma_min_deg': round(float(np.degrees(sigma.min())), 4),
                      'aspect_max': round(float((sigma.max(axis=1) / sigma.min(axis=1)).max()), 5)}))
