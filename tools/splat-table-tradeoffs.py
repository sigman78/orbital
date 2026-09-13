"""Measure grid-size tradeoffs without modifying any bake or the fitter.

For each input .bin file, rebuilds the cell lookup table at several candidate grid
resolutions (using the shared fitter's `read_records` and `cell_table`) and reports
the resulting total size, table size, and area-weighted mean and max splats-per-cell,
to help pick a grid size before baking. Ported from the peer machine's splat-baking
experiments (2026-09-13).

Usage: python tools/splat-table-tradeoffs.py <records.bin> [<records2.bin> ...]
"""
import importlib.util
import json
from pathlib import Path
import sys
import numpy as np

script = Path(__file__).with_name('bake-splats.py')
spec = importlib.util.spec_from_file_location('baker', script)
baker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(baker)
for filename in sys.argv[1:]:
    centre, sigma, axis, amplitude, reach = baker.read_records(filename)
    for nx, ny in [(16,8),(24,12),(32,16),(48,24),(64,32),(96,48)]:
        baker.CELLS_LON, baker.CELLS_LAT = nx, ny
        table = baker.cell_table(centre, reach)
        counts = table[2:2 + 2 * nx * ny].reshape(-1,2)[:,1].reshape(ny,nx)
        latitude_weight = np.cos((.5 - (np.arange(ny) + .5) / ny) * np.pi)
        print(json.dumps({'file': Path(filename).name, 'grid': [nx,ny],
                          'total_bytes': 16 + len(centre) * 64 + table.nbytes,
                          'table_bytes': table.nbytes,
                          'area_weighted_mean_splats_per_pixel': round(float(np.average(counts.mean(axis=1), weights=latitude_weight)), 2),
                          'max_splats_per_pixel': int(counts.max())}))
