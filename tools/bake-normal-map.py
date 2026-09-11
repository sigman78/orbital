#!/usr/bin/env python3
"""Bakes a tangent-space normal map (plus height in alpha) from a planetary DEM.

Used by import-assets.ps1 for the Moon (NASA LOLA, 16-bit half-metres) and Mars
(NASA MOLA MEGDR, signed 16-bit metres). The output matches the renderer's
equirectangular tangent basis: red = east, green = south (DirectX style), blue
= up. Alpha holds the height normalised over the map's range so the surface
shader can trace short self-shadows; the range in metres is printed as JSON.

Requires Python 3 with numpy and Pillow. The committed PNGs are canonical;
this only documents and reproduces how they were derived.
"""
import argparse
import json
import math
import sys

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None


def load_heights(path, fmt):
    """Returns heights in metres as a float32 (rows, columns) array."""
    if fmt == "ldem-uint16":
        # NASA SVS CGI Moon Kit: unsigned half-metres relative to a 1727.4 km
        # sphere; the reference radius is 1737.4 km.
        values = np.asarray(Image.open(path), dtype=np.float32)
        return values * 0.5 - 10000.0
    if fmt == "pds-int16":
        # PDS MEGDR: big-endian signed metres relative to the areoid, 2:1 grid.
        values = np.fromfile(path, dtype=">i2")
        rows = int(math.isqrt(values.size // 2))
        if rows * rows * 2 != values.size:
            sys.exit(f"{path}: not a 2:1 grid ({values.size} samples)")
        return values.reshape(rows, rows * 2).astype(np.float32)
    sys.exit(f"unknown format {fmt}")


def resize(heights, width):
    if heights.shape[1] <= width:
        return heights
    height = width // 2
    image = Image.fromarray(heights, mode="F").resize((width, height), Image.BOX)
    return np.asarray(image, dtype=np.float32)


def bake(heights, radius_m, strength):
    rows, columns = heights.shape
    # Central differences, wrapping in longitude and clamping at the poles.
    east = np.roll(heights, -1, axis=1) - np.roll(heights, 1, axis=1)
    south = np.empty_like(heights)
    south[1:-1] = heights[2:] - heights[:-2]
    south[0] = heights[1] - heights[0]
    south[-1] = heights[-1] - heights[-2]
    latitude = (0.5 - (np.arange(rows, dtype=np.float32) + 0.5) / rows) * math.pi
    texel_east = 2.0 * math.pi * radius_m * np.maximum(np.cos(latitude), 0.02) / columns
    texel_south = math.pi * radius_m / rows
    slope_east = east / (2.0 * texel_east[:, None])
    slope_south = south / (2.0 * texel_south)
    nx = -slope_east * strength
    ny = -slope_south * strength
    nz = np.ones_like(heights)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.stack([nx / length, ny / length, nz / length], axis=-1)
    rgb = np.clip(np.rint(normal * 127.5 + 127.5), 0, 255).astype(np.uint8)
    low, high = float(heights.min()), float(heights.max())
    alpha = np.clip(np.rint((heights - low) / max(high - low, 1e-6) * 255.0), 0, 255).astype(np.uint8)
    return np.dstack([rgb, alpha]), low, high


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", required=True)
    parser.add_argument("--format", required=True, choices=["ldem-uint16", "pds-int16"])
    parser.add_argument("--output", required=True)
    parser.add_argument("--radius-km", type=float, required=True, help="body radius the slopes are scaled by")
    parser.add_argument("--strength", type=float, default=1.0, help="slope exaggeration; 1 is physical")
    parser.add_argument("--max-width", type=int, default=4096)
    args = parser.parse_args()

    heights = load_heights(args.source, args.format)
    source_rows, source_columns = heights.shape
    heights = resize(heights, args.max_width)
    pixels, low, high = bake(heights, args.radius_km * 1000.0, args.strength)
    Image.fromarray(pixels, mode="RGBA").save(args.output, format="PNG", optimize=True)
    print(json.dumps({
        "source_width": source_columns,
        "source_height": source_rows,
        "width": int(heights.shape[1]),
        "height": int(heights.shape[0]),
        "height_min_m": round(low, 1),
        "height_max_m": round(high, 1),
        "strength": args.strength,
    }))


if __name__ == "__main__":
    main()
