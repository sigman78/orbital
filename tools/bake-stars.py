#!/usr/bin/env python3
"""Bakes the Yale Bright Star Catalogue into the renderer's star records.

Used by import-assets.ps1. The catalogue (BSC5, Hoffleit and Warren 1991, public
domain, as served by the Harvard-Smithsonian Telescope Data Center) lists the
9,110 stars to visual magnitude about 6.5 with J2000 positions, V magnitudes
and B-V colour indices. Each star becomes a 32-byte record the star pass reads
as a Vertex: position = direction xyz in the renderer's sky frame (x toward the
vernal equinox, y toward the celestial north pole, z completing the right-handed
set) and linear flux relative to magnitude 0 (10^(-0.4 V)); normal = linear RGB
of the star's colour at unit luminance, from the colour index through
Ballesteros' temperature formula and a blackbody fit, and the magnitude.

Output: a little-endian binary file, 16-byte header ("STAR", uint32 count,
uint32 record size 32, uint32 reserved) then the records. Stars without a
position or magnitude (14 of the 9,110, novae and duplicates) are skipped.

Requires Python 3 with numpy.
"""
import argparse
import gzip
import json
import math
import struct

import numpy as np


def blackbody_rgb(kelvin):
    """Linear sRGB of a blackbody, normalised to unit luminance (Tanner Helland's fit)."""
    t = min(max(kelvin, 1000.0), 40000.0) / 100.0
    if t <= 66:
        r = 255.0
        g = 99.4708025861 * math.log(t) - 161.1195681661
        b = 0.0 if t <= 19 else 138.5177312231 * math.log(t - 10.0) - 305.0447927307
    else:
        r = 329.698727446 * (t - 60.0) ** -0.1332047592
        g = 288.1221695283 * (t - 60.0) ** -0.0755148492
        b = 255.0
    srgb = np.clip(np.array([r, g, b]) / 255.0, 0.0, 1.0)
    linear = np.where(srgb <= .04045, srgb / 12.92, ((srgb + .055) / 1.055) ** 2.4)
    luminance = float(linear @ np.array([.2126, .7152, .0722]))
    return linear / max(luminance, 1e-6)


def temperature(b_v):
    """Ballesteros 2012: effective temperature from B-V."""
    return 4600.0 * (1.0 / (0.92 * b_v + 1.7) + 1.0 / (0.92 * b_v + 0.62))


def parse(line):
    try:
        ra_h, ra_m, ra_s = int(line[75:77]), int(line[77:79]), float(line[79:83])
        sign = line[83]
        de_d, de_m, de_s = int(line[84:86]), int(line[86:88]), int(line[88:90])
        magnitude = float(line[102:107])
    except ValueError:
        return None
    colour = line[109:114].strip()
    b_v = float(colour) if colour else 0.65  # the Sun's, for the few without one
    ra = math.radians((ra_h + ra_m / 60.0 + ra_s / 3600.0) * 15.0)
    dec = math.radians((de_d + de_m / 60.0 + de_s / 3600.0) * (-1.0 if sign == "-" else 1.0))
    direction = (math.cos(dec) * math.cos(ra), math.sin(dec), math.cos(dec) * math.sin(ra))
    return direction, magnitude, b_v


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", required=True, help="bsc5.dat or bsc5.dat.gz")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    opener = gzip.open if args.source.endswith(".gz") else open
    with opener(args.source, "rt", encoding="latin-1") as file:
        lines = [line for line in file.read().split("\n") if len(line) > 110]
    stars = [star for star in map(parse, lines) if star]
    stars.sort(key=lambda star: star[1])  # brightest first, so a truncated draw keeps the bright ones

    records = bytearray()
    for direction, magnitude, b_v in stars:
        rgb = blackbody_rgb(temperature(b_v))
        flux = 10.0 ** (-0.4 * magnitude)
        records += struct.pack("<8f", direction[0], direction[1], direction[2], flux, rgb[0], rgb[1], rgb[2], magnitude)
    header = struct.pack("<4sIII", b"STAR", len(stars), 32, 0)
    with open(args.output, "wb") as file:
        file.write(header + records)

    magnitudes = np.array([star[1] for star in stars])
    counts = {str(m): int(((magnitudes >= m) & (magnitudes < m + 1)).sum()) for m in range(-2, 8)}
    print(json.dumps({
        "count": len(stars),
        "skipped": len(lines) - len(stars),
        "brightest_magnitude": round(float(magnitudes.min()), 2),
        "faintest_magnitude": round(float(magnitudes.max()), 2),
        "count_per_magnitude": counts,
        "bytes": len(header) + len(records),
    }))


if __name__ == "__main__":
    main()
