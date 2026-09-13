"""Summarize where a bake spends its records and lookup-table bytes.

Reports per-file byte breakdowns (record bytes vs. cell-table bytes), narrow-axis
width percentiles, the share of splats sitting at the width floor or the aspect cap,
and a bucketed breakdown of splat count and index-list bytes by major-axis width.
Ported from the peer machine's splat-baking experiments (2026-09-13).

Usage: python tools/analyze-splats.py <records.bin> [<records2.bin> ...]
"""
import json
from pathlib import Path
import struct
import sys
import numpy as np

for filename in sys.argv[1:]:
    path = Path(filename)
    data = path.read_bytes()
    _, count, stride, length = struct.unpack_from('<4sIII', data)
    raw = np.frombuffer(data, '<f4', count * 16, 16).reshape(count, 16)
    table = np.frombuffer(data, '<u4', length, 16 + count * stride)
    nx, ny = map(int, table[:2])
    members = table[2 + nx * ny * 2:]
    uses = np.bincount(members, minlength=count)
    widths = np.degrees(raw[:, [3, 7]])
    narrow, broad = widths.min(axis=1), widths.max(axis=1)
    aspect = broad / narrow
    result = {'file': path.name, 'count': count, 'bytes': len(data),
              'record_bytes': count * stride, 'table_bytes': length * 4,
              'table_percent': round(length * 400 / len(data), 2),
              'narrow_width_percentiles_deg': dict(zip(['min','p10','p25','p50','p75','p90','max'],
                  np.round(np.percentile(narrow, [0,10,25,50,75,90,100]), 3).tolist())),
              'narrow_at_floor_percent': round(float(np.mean(narrow <= narrow.min() * 1.01) * 100), 2),
              'aspect_at_cap_percent': round(float(np.mean(aspect >= aspect.max() * .99) * 100), 2),
              'width_groups': []}
    for lo, hi in [(0,1),(1,2),(2,5),(5,10),(10,46)]:
        mask = (broad >= lo) & (broad < hi)
        result['width_groups'].append({'major_width_deg': [lo,hi], 'splats': int(mask.sum()),
                                       'index_bytes': int(uses[mask].sum() * 4)})
    print(json.dumps(result, indent=2))
