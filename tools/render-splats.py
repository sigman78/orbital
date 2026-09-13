"""Render exported SPLT records with their stored reach cutoff to an unfolded map.

Loads a baked splat (.bin) file, evaluates every splat's Gaussian falloff against a
grid of directions using the shared fitter's geometry helpers (`read_records`,
`directions`, `GAMMA`), and writes an equirectangular PNG next to the input, refusing
to overwrite one that already exists. Ported from the peer machine's splat-baking
experiments (2026-09-13).

Usage: python tools/render-splats.py <records.bin> [--width 2048]
"""
import argparse
import importlib.util
from pathlib import Path

import numpy as np
from PIL import Image
import torch

parser = argparse.ArgumentParser()
parser.add_argument('records', type=Path)
parser.add_argument('--width', type=int, default=2048)
args = parser.parse_args()
output = args.records.with_suffix('.render.png')
if output.exists():
    raise SystemExit(f'Refusing to overwrite {output}')
script = Path(__file__).with_name('bake-splats.py')
spec = importlib.util.spec_from_file_location('baker', script)
baker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(baker)
device = 'cuda' if torch.cuda.is_available() else 'cpu'
centre, sigma, axis, amplitude, reach = [torch.tensor(x.copy(), device=device) for x in baker.read_records(args.records)]
axis2 = torch.linalg.cross(centre, axis)
directions, _ = baker.directions(args.width, args.width // 2)
pixels = []
with torch.no_grad():
    for start in range(0, len(directions), 8192):
        d = torch.tensor(directions[start:start + 8192], dtype=torch.float32, device=device)
        cosine = (d @ centre.T).clamp(-1, 1)
        sine = (1 - cosine.square()).clamp_min(1e-12).sqrt()
        angle = torch.atan2(sine, cosine)
        scale = torch.where(sine > 1e-4, angle / sine, torch.ones_like(angle))
        u = (d @ axis.T) * scale / sigma[:, 0]
        v = (d @ axis2.T) * scale / sigma[:, 1]
        weight = torch.exp(-.5 * (u.square() + v.square()))
        weight *= cosine >= reach
        pixels.append((weight @ amplitude).cpu().numpy())
linear = np.concatenate(pixels).reshape(args.width // 2, args.width, 3)
encoded = (np.clip(linear, 0, 1) ** (1 / baker.GAMMA) * 255).astype(np.uint8)
Image.fromarray(encoded).save(output)
print(output)
preview = args.records.with_suffix('.png')
review = args.records.with_suffix('.review.png')
if preview.exists() and not review.exists():
    with Image.open(preview) as original:
        original.thumbnail((2048, 2048), Image.Resampling.LANCZOS)
        original.save(review)
    print(review)
