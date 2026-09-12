#!/usr/bin/env python3
"""Bakes a wind flow map for the gas giant's cloud deck.

Used by import-assets.ps1 after the Jupiter albedo is imported. The surface
shader advects the albedo along this map (two-phase flow-map advection) so
bands shear past each other, the Great Red Spot turns and the turbulent
regions roil, instead of the whole picture wobbling under a noise warp.

The flow has to follow the picture. Advection displaces the albedo by up to
a dozen texels per phase, and wherever the flow crosses a cloud feature the
two phases blend a doubled, smeared copy of it. So the direction comes from
the albedo itself: a structure tensor of the image gives the local contour
direction (the orientation along which the picture changes least), and
every feature slides along its own contour, where a displacement is nearly
invisible. The winds decide the speed and the sign:

  guide field   the measured east-west wind profile against latitude, read
                approximately (within about 15 m/s) from the Cassini profile
                in Porco et al. 2003 (Science 299) and Voyager's in Limaye
                1986 (Icarus 65), plus hand-placed vortices for this albedo
                (the Great Red Spot and the ovals, anticyclones that turn
                anticlockwise in the southern hemisphere). Where the picture
                has no clear contour (a blank zone) the flow is the guide.
  alignment     the contour direction, oriented to agree with the guide, at
                the guide's speed. Around a vortex the contours are its own
                rings, and the shear on either side orients them into the
                right sense of rotation.
  modulation    slow noise varies the speed along the contour so bands do not
                slide as one block, and a little cross-contour curl noise,
                weighted by the shear between jets and the Red Spot's wake,
                makes the turbulent regions roil.

Output: RGBA8 PNG. R and G hold the signed east and south components of the
flow, encoded 0.5 + v * FLOW_SCALE / 2, in texture units per second at real
wind speed; B holds the turbulence weight the shader uses to place its fine
detail; A is 255. The shader decodes with GAS_FLOW_UNIT = 1 / FLOW_SCALE.

With --detail-output, a second map: flow-aligned fine detail for close-ups,
where the 2K albedo goes soft. Three octaves of noise are integrated along
the flow's streamlines (line integral convolution), so the fine structure is
filaments that run with the wind, as cloud detail does, instead of the
isotropic noise the shader used to add. Grayscale, 0.5 + value / 6 with the
value in standard deviations; the shader multiplies it into the albedo at
magnification, advected on the same flow.

Requires Python 3 with numpy and Pillow.
"""
import argparse
import json
import math

import numpy as np
from PIL import Image

FLOW_SCALE = 1.6e6  # 1.0 in a decoded channel is 6.25e-7 texture units per second (281 m/s at the equator)
RADIUS_EQUATORIAL_M = 71492e3
RADIUS_POLAR_M = 66854e3

# Planetographic latitude (degrees, north positive) and eastward wind (m/s).
ZONAL_PROFILE = [
    (-80, 0), (-72, 4), (-66, -8), (-62, 20), (-58, -10), (-54, 28), (-50, -16), (-46, 24), (-43, -18),
    (-40, 34), (-37, -14), (-33, 40), (-30, -22), (-27, 48), (-23, 10), (-20, -60), (-16, -10), (-12, 40),
    (-7, 100), (-3, 125), (0, 105), (3, 120), (7, 140), (12, 30), (17, -30), (21, 40), (24, 170), (27, 20),
    (29, -35), (32, 10), (34, 42), (37, -20), (40, 48), (44, -12), (48, 30), (52, -10), (56, 22), (60, -6),
    (66, 12), (72, 2), (80, 0),
]

# Vortices in this albedo: texture u and v of the centre, semi-axes in degrees
# of longitude and latitude, rim speed in m/s, and the sense of rotation seen
# from above (+1 anticlockwise, the southern anticyclones).
VORTICES = [
    {"name": "Great Red Spot", "u": .3662, "v": .6074, "a": 9.0, "b": 4.6, "speed": 110, "sense": +1},
    {"name": "oval east of the Spot", "u": .4551, "v": .6123, "a": 2.0, "b": 1.6, "speed": 45, "sense": +1},
    # The string of white ovals at 39 S.
    {"name": "white oval 1", "u": .195, "v": .718, "a": 1.8, "b": 1.4, "speed": 50, "sense": +1},
    {"name": "white oval 2", "u": .405, "v": .718, "a": 1.8, "b": 1.4, "speed": 50, "sense": +1},
    {"name": "white oval 3", "u": .562, "v": .716, "a": 1.8, "b": 1.4, "speed": 50, "sense": +1},
    {"name": "white oval 4", "u": .703, "v": .718, "a": 1.8, "b": 1.4, "speed": 50, "sense": +1},
    {"name": "white oval 5", "u": .879, "v": .718, "a": 1.8, "b": 1.4, "speed": 50, "sense": +1},
]

# Turbulent wakes: texture centre, half-size in texture units, weight.
WAKES = [
    {"name": "Red Spot wake", "u": .325, "v": .597, "ru": .040, "rv": .022, "weight": .9},
]


def smoothstep(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def blur(field, sigma):
    """Separable Gaussian blur of a float field, periodic across the width, clamped at the poles."""
    radius = int(math.ceil(3.0 * sigma))
    kernel = np.exp(-.5 * (np.arange(-radius, radius + 1) / sigma) ** 2)
    kernel /= kernel.sum()
    rows = np.pad(field, ((0, 0), (radius, radius)), mode="wrap")
    rows = np.apply_along_axis(lambda row: np.convolve(row, kernel, mode="valid"), 1, rows)
    columns = np.pad(rows, ((radius, radius), (0, 0)), mode="edge")
    return np.apply_along_axis(lambda column: np.convolve(column, kernel, mode="valid"), 0, columns)


def periodic_noise(width, height, cells_x, rng):
    """Smooth random field, periodic across the width, from a coarse grid upsampled bicubically."""
    cells_y = max(2, cells_x // 2)
    grid = rng.standard_normal((cells_y, cells_x)).astype(np.float32)
    tiled = np.tile(grid, (1, 3))
    image = Image.fromarray(tiled, mode="F").resize((width * 3, height), Image.BICUBIC)
    return np.asarray(image, dtype=np.float64)[:, width:width * 2]


def gradient_wrapped(field, spacing_y, spacing_x):
    padded = np.concatenate([field[:, -1:], field, field[:, :1]], axis=1)
    dy, dx = np.gradient(padded, spacing_y, spacing_x)
    return dy[:, 1:-1], dx[:, 1:-1]


def sample_bilinear(field, x, y):
    """Bilinear read of a field at pixel positions, periodic across the width, clamped at the poles."""
    height, width = field.shape
    x = np.mod(x, width)
    y = np.clip(y, 0.0, height - 1.001)
    x0 = np.floor(x).astype(np.int32)
    y0 = np.floor(y).astype(np.int32)
    fx = (x - x0).astype(np.float32)
    fy = (y - y0).astype(np.float32)
    x1 = np.mod(x0 + 1, width)
    y1 = np.minimum(y0 + 1, height - 1)
    return ((field[y0, x0] * (1.0 - fx) + field[y0, x1] * fx) * (1.0 - fy)
            + (field[y1, x0] * (1.0 - fx) + field[y1, x1] * fx) * fy)


def lic(direction_u, direction_v, noise, length):
    """Line integral convolution: each pixel averages the noise along its streamline, both ways, Hann-weighted."""
    height, width = noise.shape
    ys, xs = np.mgrid[0:height, 0:width].astype(np.float32)
    total = noise.copy()
    weight_sum = np.ones_like(noise)
    for sign in (1.0, -1.0):
        x, y = xs.copy(), ys.copy()
        for step in range(1, length + 1):
            du = sample_bilinear(direction_u, x, y)
            dv = sample_bilinear(direction_v, x, y)
            x += sign * du * width
            y += sign * dv * height
            w = np.float32(.5 * (1.0 + math.cos(math.pi * step / (length + 1))))
            total += sample_bilinear(noise, x, y) * w
            weight_sum += w
    return total / weight_sum


def bake_detail(flow_u, flow_v, detail_width, rng):
    """Flow-aligned filament detail: three octaves of LIC noise, each at the resolution its scale needs."""
    speed = np.sqrt(flow_u ** 2 + flow_v ** 2)
    unit_u = (flow_u / np.maximum(speed, 1e-20)).astype(np.float32)
    unit_v = (flow_v / np.maximum(speed, 1e-20)).astype(np.float32)
    # Unit direction in texture units per pixel step of a map with the given width: one pixel along the streamline.
    def direction_for(width):
        height = width // 2
        du = np.asarray(Image.fromarray(unit_u, mode="F").resize((width, height), Image.BILINEAR), dtype=np.float32)
        dv = np.asarray(Image.fromarray(unit_v, mode="F").resize((width, height), Image.BILINEAR), dtype=np.float32)
        norm = np.maximum(np.sqrt((du * width) ** 2 + (dv * height) ** 2), 1e-9)
        return du / norm, dv / norm

    result = np.zeros((detail_width // 2, detail_width), dtype=np.float32)
    # (resolution divisor, noise blur sigma in pixels, streamline half-length in pixels, weight)
    for divisor, sigma, length, weight in ((1, .7, 14, .45), (2, 1.6, 30, .35), (4, 3.0, 60, .20)):
        width = detail_width // divisor
        height = width // 2
        noise = rng.standard_normal((height, width)).astype(np.float32)
        noise = blur(noise, sigma).astype(np.float32)
        du, dv = direction_for(width)
        streaks = lic(du, dv, noise, length)
        streaks = (streaks - streaks.mean()) / max(float(streaks.std()), 1e-9)
        if divisor > 1:
            streaks = np.asarray(Image.fromarray(streaks, mode="F").resize((detail_width, detail_width // 2), Image.BICUBIC), dtype=np.float32)
        result += streaks * weight
    result /= max(float(result.std()), 1e-9)
    return np.clip(np.rint((0.5 + result / 6.0) * 255.0), 0, 255).astype(np.uint8)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--albedo", required=True, help="the imported gas giant albedo")
    parser.add_argument("--output", required=True)
    parser.add_argument("--width", type=int, default=1024)
    parser.add_argument("--turbulence", type=float, default=8.0, help="rms cross-contour speed, m/s, where the weight is 1")
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--detail-output", help="also bake the flow-aligned detail map here")
    parser.add_argument("--detail-width", type=int, default=4096)
    args = parser.parse_args()

    width, height = args.width, args.width // 2
    albedo = Image.open(args.albedo).convert("RGB").resize((width, height), Image.LANCZOS)
    luminance = np.asarray(albedo, dtype=np.float64) @ np.array([.2126, .7152, .0722]) / 255.0

    u = (np.arange(width) + .5) / width
    v = (np.arange(height) + .5) / height
    uu, vv = np.meshgrid(u, v)
    lat = (0.5 - vv) * 180.0  # degrees, north positive (v = 0 is the north pole)
    lon = (uu - 0.5) * 360.0
    cos_lat = np.maximum(np.cos(np.radians(lat)), math.cos(math.radians(80)))

    # --- Guide field, m/s in (east, north): zonal jets with the vortices cut in.
    lats = np.array([p[0] for p in ZONAL_PROFILE], dtype=np.float64)
    winds = np.array([p[1] for p in ZONAL_PROFILE], dtype=np.float64)
    east = np.interp(lat, lats, winds)
    north = np.zeros_like(east)

    shear = np.abs(np.gradient(east, axis=0)) / (180.0 / height)  # m/s per degree
    kernel = np.exp(-np.linspace(-2.0, 2.0, 13) ** 2)  # about two degrees of latitude
    kernel /= kernel.sum()
    shear = np.apply_along_axis(lambda column: np.convolve(column, kernel, mode="same"), 0, shear)
    band_weight = smoothstep(75.0, 60.0, np.abs(lat))  # nothing at the poles
    weight = np.maximum(np.clip(shear / 25.0, 0.0, 1.0), .15) * band_weight

    for vortex in VORTICES:
        centre_lat = (0.5 - vortex["v"]) * 180.0
        centre_lon = (vortex["u"] - 0.5) * 360.0
        dlon = (lon - centre_lon + 180.0) % 360.0 - 180.0
        dlat = lat - centre_lat
        r = np.sqrt((dlon / vortex["a"]) ** 2 + (dlat / vortex["b"]) ** 2)
        speed = vortex["speed"] * r * np.exp((1.0 - r * r) * .5)  # rim speed, quiet centre
        tx, ty = -dlat / vortex["b"] ** 2, dlon / vortex["a"] ** 2  # tangent to the ellipse, anticlockwise
        norm = np.maximum(np.sqrt(tx * tx + ty * ty), 1e-9)
        tx, ty = tx / norm * vortex["sense"], ty / norm * vortex["sense"]
        inside = 1.0 - smoothstep(.9, 1.5, r)
        east = east * (1.0 - inside) + speed * tx
        north = north + speed * ty
        weight = np.maximum(weight, .5 * smoothstep(1.6, 1.0, r) * smoothstep(.6, 1.0, r))

    for wake in WAKES:
        du = (uu - wake["u"] + .5) % 1.0 - .5
        dv = vv - wake["v"]
        r = np.sqrt((du / wake["ru"]) ** 2 + (dv / wake["rv"]) ** 2)
        weight = np.maximum(weight, wake["weight"] * smoothstep(1.3, .6, r))

    # Guide in texture units per second (v runs south), and its unit direction.
    guide_u = east / (2.0 * math.pi * RADIUS_EQUATORIAL_M * cos_lat)
    guide_v = -north / (math.pi * RADIUS_POLAR_M)
    guide_speed = np.sqrt(guide_u ** 2 + guide_v ** 2)
    # Orientation reference: the guide, smoothed so the sign is stable where a jet crosses zero.
    ref_u, ref_v = blur(guide_u, 3.0), blur(guide_v, 3.0)

    # --- Contour direction from the structure tensor of the picture.
    smooth = blur(luminance, 1.2)
    gy, gx = gradient_wrapped(smooth, 1.0 / height, 1.0 / width)  # per texture unit, so the aspect is right
    tensor_sigma = 3.0
    jxx, jxy, jyy = blur(gx * gx, tensor_sigma), blur(gx * gy, tensor_sigma), blur(gy * gy, tensor_sigma)
    trace = jxx + jyy
    root = np.sqrt(((jxx - jyy) * .5) ** 2 + jxy ** 2)
    coherence = np.where(trace > 1e-12, (2.0 * root / np.maximum(trace, 1e-12)) ** 2, 0.0)
    coherence = smoothstep(.15, .7, coherence)
    # Dominant gradient direction, then its perpendicular: the contour.
    angle = .5 * np.arctan2(2.0 * jxy, jxx - jyy)
    tangent_u, tangent_v = -np.sin(angle), np.cos(angle)
    sign = np.where(tangent_u * ref_u + tangent_v * ref_v < 0.0, -1.0, 1.0)
    tangent_u, tangent_v = tangent_u * sign, tangent_v * sign

    # --- The flow: guide speed along the contour where there is one, the guide itself where there is not.
    guide_dir_u = np.where(guide_speed > 0, guide_u / np.maximum(guide_speed, 1e-20), 0.0)
    guide_dir_v = np.where(guide_speed > 0, guide_v / np.maximum(guide_speed, 1e-20), 0.0)
    dir_u = coherence * tangent_u + (1.0 - coherence) * guide_dir_u
    dir_v = coherence * tangent_v + (1.0 - coherence) * guide_dir_v
    norm = np.maximum(np.sqrt(dir_u ** 2 + dir_v ** 2), 1e-9)
    dir_u, dir_v = dir_u / norm, dir_v / norm

    rng = np.random.default_rng(args.seed)
    modulation = 1.0 + .35 * np.tanh(periodic_noise(width, height, 32, rng) * .8)  # speed varies along the band
    flow_u = guide_speed * modulation * dir_u
    flow_v = guide_speed * modulation * dir_v

    # Cross-contour roil: curl of a multi-octave potential, divergence-free, small, where the weight says.
    potential = np.zeros((height, width), dtype=np.float64)
    for octave, cells in enumerate((48, 96, 192)):
        potential += periodic_noise(width, height, cells, rng) * (.6 ** octave)
    dpsi_dv, dpsi_du = gradient_wrapped(potential, 1.0 / height, 1.0 / width)
    turb_east, turb_north = -dpsi_dv, -dpsi_du
    rms = math.sqrt(float(np.mean(turb_east ** 2 + turb_north ** 2)))
    turb_east *= args.turbulence / rms
    turb_north *= args.turbulence / rms
    flow_u += turb_east * weight / (2.0 * math.pi * RADIUS_EQUATORIAL_M * cos_lat)
    flow_v += -turb_north * weight / (math.pi * RADIUS_POLAR_M)

    # Settle pixel-level jitter in the direction field.
    flow_u, flow_v = blur(flow_u, 1.0), blur(flow_v, 1.0)

    encoded = np.zeros((height, width, 4), dtype=np.uint8)
    encoded[..., 0] = np.clip(np.rint((0.5 + flow_u * FLOW_SCALE * 0.5) * 255.0), 0, 255)
    encoded[..., 1] = np.clip(np.rint((0.5 + flow_v * FLOW_SCALE * 0.5) * 255.0), 0, 255)
    encoded[..., 2] = np.clip(np.rint(weight * 255.0), 0, 255)
    encoded[..., 3] = 255
    Image.fromarray(encoded, mode="RGBA").save(args.output, optimize=True)

    detail_size = None
    if args.detail_output:
        detail = bake_detail(flow_u, flow_v, args.detail_width, rng)
        Image.fromarray(detail, mode="L").save(args.detail_output, optimize=True)
        detail_size = detail.shape[::-1]

    speed_ms = np.sqrt((flow_u * 2.0 * math.pi * RADIUS_EQUATORIAL_M * cos_lat) ** 2
                       + (flow_v * math.pi * RADIUS_POLAR_M) ** 2)
    print(json.dumps({
        "width": width, "height": height,
        "detail_width": detail_size[0] if detail_size else 0, "detail_height": detail_size[1] if detail_size else 0,
        "max_speed_ms": round(float(speed_ms.max()), 1),
        "mean_speed_ms": round(float(speed_ms.mean()), 1),
        "aligned_fraction": round(float(np.mean(coherence)), 3),
        "flow_scale": FLOW_SCALE,
        "clipped": int(np.sum((np.abs(flow_u) > 1.0 / FLOW_SCALE) | (np.abs(flow_v) > 1.0 / FLOW_SCALE))),
    }))


if __name__ == "__main__":
    main()
