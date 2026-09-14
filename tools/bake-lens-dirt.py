#!/usr/bin/env python3
"""Bakes the dirty-glass mask for the composite's cockpit glass effect.

Used by import-assets.ps1. The composite stretches the map over the frame as
a film on a convex pane in front of the camera: behind the marks the view is
blurred, and where the sun grazes the pane the marks brighten. Everything is
modelled, there is no photographed source, and it is all low frequency: no
scratches or prints, since they read as a picture on the glass rather than as
residue. The map is a sum of

  dust      soft specks a few pixels wide, sparse
  residue   the uneven film a cleaning leaves: broad noise patches with soft
            thresholds, and the arcs of the strokes with more residue at
            their edges, their density varying along the stroke
  smears    elongated soft patches at random angles

Output: grayscale PNG, 16:9, linear coverage 0..1, lightly blurred so nothing
in it is sharper than a couple of texels. Deterministic for a seed.
"""

import argparse
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


def noise_field(rng, shape, scale, octaves=4, gain=0.5):
    """Value fBm: octaves of bicubically upsampled random lattices, zero mean."""
    height, width = shape
    total = np.zeros(shape, dtype=np.float32)
    amplitude, weight_sum = 1.0, 0.0
    cell = scale
    for _ in range(octaves):
        cells_x = max(2, int(math.ceil(width / cell)) + 1)
        cells_y = max(2, int(math.ceil(height / cell)) + 1)
        lattice = rng.random((cells_y, cells_x), dtype=np.float32)
        image = Image.fromarray((lattice * 255).astype(np.uint8)).resize(
            (int(cells_x * cell), int(cells_y * cell)), Image.BICUBIC)
        layer = np.asarray(image, dtype=np.float32)[:height, :width] / 255.0
        total += (layer - 0.5) * amplitude
        weight_sum += amplitude
        amplitude *= gain
        cell = max(2.0, cell / 2.0)
    return total / weight_sum


def stamp(canvas, y, x, patch):
    """Adds a patch centred at (y, x), clipped to the canvas."""
    ph, pw = patch.shape
    y0, x0 = int(round(y - ph / 2)), int(round(x - pw / 2))
    y1, x1 = y0 + ph, x0 + pw
    cy0, cx0 = max(y0, 0), max(x0, 0)
    cy1, cx1 = min(y1, canvas.shape[0]), min(x1, canvas.shape[1])
    if cy1 <= cy0 or cx1 <= cx0:
        return
    canvas[cy0:cy1, cx0:cx1] += patch[cy0 - y0:cy1 - y0, cx0 - x0:cx1 - x0]


def bake_dust(rng, shape, count):
    layer = np.zeros(shape, dtype=np.float32)
    for _ in range(count):
        radius = rng.uniform(1.5, 4.0)
        size = int(math.ceil(radius * 3))
        ys, xs = np.mgrid[:size, :size].astype(np.float32)
        r = np.hypot(ys - size / 2 + 0.5, xs - size / 2 + 0.5) / radius
        patch = np.exp(-r * r * 2.0) * rng.uniform(0.3, 0.8)
        stamp(layer, rng.uniform(0, shape[0]), rng.uniform(0, shape[1]), patch)
    return layer


def bake_residue(rng, shape):
    # Broad uneven film: two noise fields, one soft-thresholded into patches and
    # one varying the density inside them.
    patches = noise_field(rng, shape, 420, octaves=4, gain=0.55)
    density = noise_field(rng, shape, 90, octaves=3) + 0.5
    film = np.clip((patches - 0.02) / 0.22, 0.0, 1.0) * (0.55 + 0.45 * density) * 0.25
    return film


def bake_strokes(rng, shape, count):
    # The arcs of the cleaning strokes: residue gathers at a stroke's edges,
    # and its density varies along the stroke.
    layer = np.zeros(shape, dtype=np.float32)
    ys, xs = np.mgrid[:shape[0], :shape[1]].astype(np.float32)
    along_noise = noise_field(rng, shape, 160, octaves=3) + 0.5
    for _ in range(count):
        angle = rng.uniform(0, math.pi)
        cy, cx = rng.uniform(0, shape[0]), rng.uniform(0, shape[1])
        along = (xs - cx) * math.cos(angle) + (ys - cy) * math.sin(angle)
        across = -(xs - cx) * math.sin(angle) + (ys - cy) * math.cos(angle)
        curvature = rng.uniform(-1.0, 1.0) / rng.uniform(700, 2000)
        across = across + curvature * along * along
        length = rng.uniform(250, 600)
        width = rng.uniform(50, 130)
        body = np.exp(-((across / width) ** 4)) * 0.5
        edges = np.exp(-(((np.abs(across) - width) / (width * 0.3)) ** 2)) * 0.5
        extent = np.exp(-((along / length) ** 2))
        layer += (body + edges) * extent * (0.4 + 0.6 * along_noise) * rng.uniform(0.15, 0.3)
    return layer


def bake_smears(rng, shape, count):
    layer = np.zeros(shape, dtype=np.float32)
    for _ in range(count):
        major, minor = rng.uniform(70, 180), rng.uniform(22, 60)
        angle = rng.uniform(0, math.pi)
        size = int(major * 3)
        ys, xs = np.mgrid[:size, :size].astype(np.float32)
        ys, xs = ys - size / 2, xs - size / 2
        u = (xs * math.cos(angle) + ys * math.sin(angle)) / major
        v = (-xs * math.sin(angle) + ys * math.cos(angle)) / minor
        texture = noise_field(rng, (size, size), size / 4, octaves=3) + 0.5
        patch = np.exp(-(u * u + v * v)) * (0.6 + 0.4 * texture) * rng.uniform(0.2, 0.45)
        stamp(layer, rng.uniform(0, shape[0]), rng.uniform(0, shape[1]), patch)
    return layer


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=2048)
    parser.add_argument("--height", type=int, default=1152)
    parser.add_argument("--seed", type=int, default=7)
    args = parser.parse_args()
    rng = np.random.default_rng(args.seed)
    shape = (args.height, args.width)
    mask = np.zeros(shape, dtype=np.float32)
    mask += bake_residue(rng, shape)
    mask += bake_strokes(rng, shape, 6)
    mask += bake_smears(rng, shape, 10)
    mask += bake_dust(rng, shape, 200)
    mask = np.clip(mask, 0.0, 1.0)
    image = Image.fromarray((mask * 255 + 0.5).astype(np.uint8), "L").filter(ImageFilter.GaussianBlur(2.0))
    image.save(args.output, optimize=True)
    print(json.dumps({"width": args.width, "height": args.height, "mean": float(mask.mean()), "seed": args.seed}))


if __name__ == "__main__":
    main()
