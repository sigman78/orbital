#!/usr/bin/env python3
"""Packs a scanned normal map and its displacement map into one RGBA PNG.

Used by import-assets.ps1 for the Poly Haven rock sets. The normal is
converted from the OpenGL convention (green up) to the DirectX convention the
renderer uses everywhere (green points down the image, +v). Alpha holds the
displacement normalised over its range so the surface shader can run parallax
occlusion from the same texture fetch as the normal.

Requires Python 3 with numpy and Pillow.
"""
import argparse
import json

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--normal", required=True, help="OpenGL-style normal map (green up)")
    parser.add_argument("--height", required=True, help="displacement map, 8 or 16 bit")
    parser.add_argument("--output", required=True)
    parser.add_argument("--max-width", type=int, default=2048)
    args = parser.parse_args()

    normal = Image.open(args.normal).convert("RGB")
    height = Image.open(args.height)
    source_size = normal.size
    if normal.size[0] > args.max_width:
        size = (args.max_width, args.max_width * normal.size[1] // normal.size[0])
        normal = normal.resize(size, Image.LANCZOS)
    if height.size != normal.size:
        height = height.resize(normal.size, Image.LANCZOS)

    rgb = np.asarray(normal, dtype=np.uint8).copy()
    rgb[..., 1] = 255 - rgb[..., 1]  # OpenGL green-up to DirectX green-down

    heights = np.asarray(height, dtype=np.float32)
    if heights.ndim == 3:
        heights = heights[..., 0]
    low, high = float(heights.min()), float(heights.max())
    alpha = np.clip(np.rint((heights - low) / max(high - low, 1e-6) * 255.0), 0, 255).astype(np.uint8)

    Image.fromarray(np.dstack([rgb, alpha]), mode="RGBA").save(args.output, format="PNG", optimize=True)
    print(json.dumps({
        "source_width": source_size[0],
        "source_height": source_size[1],
        "width": normal.size[0],
        "height": normal.size[1],
    }))


if __name__ == "__main__":
    main()
