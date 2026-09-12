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
                (Cassini's December 2000 map, PIA07782: the Great Red Spot
                and the white ovals, anticyclones that turn anticlockwise in
                the southern hemisphere). Where the picture
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

With --relief-output, a third map: the cloud deck's relief as its slopes
(red east, green south, the renderer's tangent convention) with the height in
alpha, for shading along the terminator. No height map of Jupiter's clouds
exists to source, so the height is modelled from what is known: zones are
high ammonia cloud and belts are clearings into deeper cloud, with the cloud
tops varying over some 30 km (Galileo NIMS, PIA00489), and the anticyclones
stand proud, the Great Red Spot by about 8 km. Bright is taken as high at the
band scale, storms and swirls ride on that at a smaller amplitude, the
vortices from the list above are added as flat-topped domes, and the
filament detail adds the finest ridges. The slopes are physical, the shader
exaggerates them; they are stored square-root encoded (0.5 + sign * sqrt(|s| /
RELIEF_SLOPE_MAX) / 2) rather than as a normal, because the slopes that matter
are a few thousandths and at grazing light an 8-bit normal's quantisation
draws the texel rows as stripes; the encoding gives them ten times the levels.

With --polar-output and --polar-flow-output, the polar caps: Juno found each
pole holds a central cyclone ringed by circumpolar cyclones near 84 degrees,
eight of 4,000 to 4,600 km in the north and five of 5,600 to 7,000 km in the
south, with winds of 55 to 95 m/s (Adriani et al. 2018, Nature 555). The
Cassini map is oblique and smeared there, so above 74 degrees the shader
blends to these caps instead: an atlas of two azimuthal projections (north
above south) of the cap within 20 degrees of each pole, holding an albedo
synthesised over the map's own polar cloud texture (filaments from
line integral convolution along the cyclones' flow, a darker eye and brighter
ring per cyclone, a few white pop-up ovals) and a flow map of the cyclones in
the cap's own coordinates (0.5 + v * POLAR_FLOW_SCALE / 2, in cap texture
units per second), so the same two-phase advection turns them.

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
    {"name": "Great Red Spot", "u": .3643, "v": .615, "a": 10.0, "b": 4.7, "speed": 110, "sense": +1},
    {"name": "oval east of the Spot", "u": .450, "v": .634, "a": 1.8, "b": 1.4, "speed": 45, "sense": +1},
    # The string of white ovals at 37 S.
    {"name": "white oval 1", "u": .149, "v": .707, "a": 1.6, "b": 1.3, "speed": 50, "sense": +1},
    {"name": "white oval 2", "u": .195, "v": .707, "a": 1.6, "b": 1.3, "speed": 50, "sense": +1},
    {"name": "white oval 3", "u": .261, "v": .707, "a": 1.6, "b": 1.3, "speed": 50, "sense": +1},
    {"name": "white oval 4", "u": .305, "v": .707, "a": 1.6, "b": 1.3, "speed": 50, "sense": +1},
    {"name": "white oval 5", "u": .464, "v": .707, "a": 1.6, "b": 1.3, "speed": 50, "sense": +1},
    {"name": "white oval 6", "u": .557, "v": .707, "a": 1.6, "b": 1.3, "speed": 50, "sense": +1},
    {"name": "white oval 7", "u": .659, "v": .707, "a": 1.6, "b": 1.3, "speed": 50, "sense": +1},
    {"name": "white oval 8", "u": .762, "v": .707, "a": 1.6, "b": 1.3, "speed": 50, "sense": +1},
]

# Turbulent wakes: texture centre, half-size in texture units, weight.
WAKES = [
    {"name": "Red Spot wake", "u": .295, "v": .612, "ru": .045, "rv": .020, "weight": .9},
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
    x0 = np.mod(np.floor(x).astype(np.int32), width)  # float32 mod can land exactly on the width
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
    return result


def encode_detail(detail):
    return np.clip(np.rint((0.5 + detail / 6.0) * 255.0), 0, 255).astype(np.uint8)


# Cloud-top relief amplitudes, km. The band term spans the zone-to-belt step,
# the swirl term the storms and streaks within a band, the vortex domes are
# the anticyclones (the Spot at its measured 8 km, the ovals assumed at half),
# and the filaments the finest ridges.
RELIEF_BAND_KM = 6.0      # per standard deviation of the band-scale brightness
RELIEF_SWIRL_KM = 2.5     # per standard deviation of the within-band brightness
RELIEF_FILAMENT_KM = .7   # per standard deviation of the filament detail
RELIEF_SPOT_KM = 8.0
RELIEF_OVAL_KM = 4.0


RELIEF_SLOPE_MAX = .1  # slope at the encoding's full scale; the physical slopes peak near .05

# --- Polar caps -------------------------------------------------------------------

POLAR_CAP_DEG = 20.0          # colatitude the cap covers; the shader blends it in above 74 degrees
POLAR_FLOW_SCALE = 4e5        # 1.0 in a decoded channel is 2.5e-6 cap texture units per second (122 m/s)
# Circumpolar cyclones after Adriani et al. 2018: count, diameter, the central cyclone's, the ring's latitude.
POLES = [
    {"name": "north", "count": 8, "diameter_km": 4300.0, "central_km": 4000.0, "ring_lat": 84.0, "phase": .1},
    {"name": "south", "count": 5, "diameter_km": 6300.0, "central_km": 5800.0, "ring_lat": 84.0, "phase": .4},
]
CYCLONE_SPEED_MS = 95.0       # peak tangential wind at a cyclone's radius


def bake_polar(albedo_path, size, rng):
    """Albedo and flow atlases of the two polar caps, north above south."""
    cap = math.sin(math.radians(POLAR_CAP_DEG))
    radius_km = RADIUS_EQUATORIAL_M / 1000.0
    source = np.asarray(Image.open(albedo_path).convert("RGB"), dtype=np.float64) / 255.0
    rows = source.shape[0]
    u = (np.arange(size) + .5) / size
    xx, zz = np.meshgrid((u - .5) * 2.0 * cap, (u - .5) * 2.0 * cap)  # unit-sphere x and z, rows run along z
    rho = np.sqrt(xx * xx + zz * zz)
    colat = np.degrees(np.arcsin(np.clip(rho, 0.0, 1.0)))
    albedo_atlas = np.zeros((2 * size, size, 4), dtype=np.uint8)
    flow_atlas = np.zeros((size, size // 2, 4), dtype=np.uint8)
    for index, pole in enumerate(POLES):
        # Cyclones: the central one and the ring; each a Rankine vortex tapered beyond its radius,
        # turning with the planet (from +x toward +z), which is cyclonic at both poles.
        centres = [(0.0, 0.0, pole["central_km"] / 2.0)]
        ring = math.sin(math.radians(90.0 - pole["ring_lat"]))
        for k in range(pole["count"]):
            angle = 2.0 * math.pi * (k + pole["phase"]) / pole["count"]
            centres.append((ring * math.cos(angle), ring * math.sin(angle), pole["diameter_km"] / 2.0))
        vx = np.zeros_like(xx)
        vz = np.zeros_like(xx)
        structure = np.zeros_like(xx)  # eye and ring shading
        for cx, cz, radius_c in centres:
            dx, dz = xx - cx, zz - cz
            r = np.maximum(np.sqrt(dx * dx + dz * dz), 1e-9) * radius_km
            inside = r < radius_c
            speed = np.where(inside, CYCLONE_SPEED_MS * r / radius_c,
                             CYCLONE_SPEED_MS * radius_c / r * np.exp(-((r - radius_c) / (1.6 * radius_c)) ** 2))
            vx += speed * (-dz) / (r / radius_km)
            vz += speed * dx / (r / radius_km)
            structure += -.14 * np.exp(-(r / (.35 * radius_c)) ** 2) + .07 * np.exp(-((r - .7 * radius_c) / (.18 * radius_c)) ** 2)
        # The folded filamentary region between and beyond the cyclones: a slow
        # circumpolar drift with large, slow eddies folded into it.
        drift = 8.0 * smoothstep(4.0, 12.0, colat)
        vx += drift * (-zz) / np.maximum(rho, 1e-9)
        vz += drift * xx / np.maximum(rho, 1e-9)
        potential = periodic_noise(size, size, 7, rng) + .5 * periodic_noise(size, size, 16, rng) + .2 * periodic_noise(size, size, 40, rng)
        dpz, dpx = np.gradient(potential, 1.0 / size, 1.0 / size)
        turb_x, turb_z = -dpz, dpx
        rms = math.sqrt(float(np.mean(turb_x ** 2 + turb_z ** 2)))
        vx += turb_x / rms * 10.0
        vz += turb_z / rms * 10.0
        flow_u = vx / (2.0 * cap * RADIUS_EQUATORIAL_M)
        flow_v = vz / (2.0 * cap * RADIUS_EQUATORIAL_M)

        # Colour: the map itself beyond 15 degrees from the pole, so the blend margin joins
        # identical pixels. Nearer the pole the map is smeared into spokes, so the inner disc
        # is a planar projection of a clean patch of the map's own polar cloud texture, from
        # the 67 to 77 degree band, at physical scale times 1.5 (like a triplanar projection,
        # but for one pole), crossfaded between 11 and 15 degrees; with a hint of the
        # blue-grey JunoCam shows toward the pole.
        channels = [blur(source[..., c], .7) for c in range(3)]

        def map_colour(px, py):
            return np.stack([sample_bilinear(channel, px, np.clip(py, 0.0, rows - 1.001)) for channel in channels], axis=-1)

        lon_u = np.arctan2(zz, xx) / (2.0 * math.pi) + .5
        lat = 90.0 - colat if pole["name"] == "north" else colat - 90.0
        direct = map_colour(lon_u * source.shape[1] - .5, (.5 - lat / 180.0) * rows - .5)
        patch_lat = 72.0 if pole["name"] == "north" else -72.0
        patch_u = .25 if pole["name"] == "north" else .65
        scale = 1.5
        patch_px = (patch_u + xx / (2.0 * math.pi * math.cos(math.radians(patch_lat))) / scale) * source.shape[1] - .5
        patch_py = (.5 - (patch_lat + np.degrees(zz) / scale) / 180.0) * rows - .5
        planar = map_colour(patch_px, patch_py)
        inner = smoothstep(15.0, 11.0, colat)[..., None]
        base = direct * (1.0 - inner) + planar * inner
        luminance = base @ np.array([.2126, .7152, .0722])
        polar_tint = np.array([.88, .92, 1.0]) * luminance[..., None]
        toward_pole = (.12 * smoothstep(12.0, 4.0, colat))[..., None]
        colour = base * (1.0 - toward_pole) + polar_tint * toward_pole
        # Filaments belong to the cyclones; between them the map's own clouds carry the texture.
        cyclone_mask = np.zeros_like(xx)
        for cx, cz, radius_c in centres:
            cyclone_mask = np.maximum(cyclone_mask, np.exp(-(((xx - cx) ** 2 + (zz - cz) ** 2) * radius_km ** 2) / (1.4 * radius_c) ** 2))

        # Filaments along the flow, two octaves, plus a darker eye and brighter ring per cyclone and white pop-up ovals.
        speed = np.sqrt(flow_u ** 2 + flow_v ** 2)
        du = (flow_u / np.maximum(speed, 1e-20)).astype(np.float32)
        dv = (flow_v / np.maximum(speed, 1e-20)).astype(np.float32)
        norm = np.maximum(np.sqrt((du * size) ** 2 + (dv * size) ** 2), 1e-9)
        du, dv = du / norm, dv / norm
        shading = np.zeros_like(xx)
        for sigma, length, weight in ((.8, 18, .06), (2.4, 45, .05)):
            noise = blur(rng.standard_normal((size, size)).astype(np.float32), sigma).astype(np.float32)
            streaks = lic(du, dv, noise, length)
            streaks = (streaks - streaks.mean()) / max(float(streaks.std()), 1e-9)
            shading += weight * streaks
        shading *= .25 + .75 * cyclone_mask  # filaments belong to the cyclones; between them the map carries the texture
        shading += structure
        shading *= smoothstep(16.0, 9.0, colat)  # nothing added where the map takes over
        for _ in range(4):
            angle, distance = rng.uniform(0, 2 * math.pi), rng.uniform(.5, 1.6) * ring
            r_oval = rng.uniform(250.0, 450.0) / radius_km
            dx, dz = xx - distance * math.cos(angle), zz - distance * math.sin(angle)
            shading += .2 * np.exp(-(dx * dx + dz * dz) / (2.0 * r_oval * r_oval))
        rgb = np.clip(colour * (1.0 + shading)[..., None], 0.0, 1.0)
        albedo_atlas[index * size:(index + 1) * size, :, :3] = np.rint(rgb * 255.0).astype(np.uint8)
        albedo_atlas[index * size:(index + 1) * size, :, 3] = 255
        half = size // 2
        fu = np.asarray(Image.fromarray(flow_u.astype(np.float32), mode="F").resize((half, half), Image.BOX), dtype=np.float64)
        fv = np.asarray(Image.fromarray(flow_v.astype(np.float32), mode="F").resize((half, half), Image.BOX), dtype=np.float64)
        flow_atlas[index * half:(index + 1) * half, :, 0] = np.clip(np.rint((0.5 + fu * POLAR_FLOW_SCALE * 0.5) * 255.0), 0, 255)
        flow_atlas[index * half:(index + 1) * half, :, 1] = np.clip(np.rint((0.5 + fv * POLAR_FLOW_SCALE * 0.5) * 255.0), 0, 255)
        flow_atlas[index * half:(index + 1) * half, :, 2] = 0
        flow_atlas[index * half:(index + 1) * half, :, 3] = 255
    stats = {"polar_size": size, "polar_cap_deg": POLAR_CAP_DEG, "polar_flow_scale": POLAR_FLOW_SCALE,
             "polar_max_speed_ms": round(float(np.sqrt(vx ** 2 + vz ** 2).max()), 1)}
    return albedo_atlas, flow_atlas, stats


def bake_relief(albedo_path, detail, width):
    """Slope+height map of the cloud deck: bright is high at the band scale, vortices are domes, filaments ridges."""
    height_px = width // 2
    albedo = Image.open(albedo_path).convert("RGB").resize((width, height_px), Image.LANCZOS)
    luminance = np.asarray(albedo, dtype=np.float64) @ np.array([.2126, .7152, .0722]) / 255.0

    def standardised(field):
        return np.clip((field - field.mean()) / max(float(field.std()), 1e-9), -2.5, 2.5)

    band_sigma = width / 400.0  # about 900 km at the equator
    band = blur(luminance, band_sigma)
    swirl = blur(luminance, band_sigma / 5.0) - band
    heights = RELIEF_BAND_KM * standardised(band) + RELIEF_SWIRL_KM * standardised(swirl)

    u = (np.arange(width) + .5) / width
    v = (np.arange(height_px) + .5) / height_px
    uu, vv = np.meshgrid(u, v)
    lat = (0.5 - vv) * 180.0
    lon = (uu - 0.5) * 360.0
    for vortex in VORTICES:
        centre_lat = (0.5 - vortex["v"]) * 180.0
        centre_lon = (vortex["u"] - 0.5) * 360.0
        dlon = (lon - centre_lon + 180.0) % 360.0 - 180.0
        dlat = lat - centre_lat
        r = np.sqrt((dlon / vortex["a"]) ** 2 + (dlat / vortex["b"]) ** 2)
        top = RELIEF_SPOT_KM if vortex["a"] > 5.0 else RELIEF_OVAL_KM
        heights += top * np.exp(-r ** 4)  # flat top, edge at the rim

    fine = np.asarray(Image.fromarray(detail.astype(np.float32), mode="F").resize((width, height_px), Image.BILINEAR),
                      dtype=np.float64)
    heights += RELIEF_FILAMENT_KM * fine
    heights *= 1000.0  # metres

    # Central differences, wrapping in longitude and clamping at the poles; slopes in radii per radii.
    east = np.roll(heights, -1, axis=1) - np.roll(heights, 1, axis=1)
    south = np.empty_like(heights)
    south[1:-1] = heights[2:] - heights[:-2]
    south[0] = heights[1] - heights[0]
    south[-1] = heights[-1] - heights[-2]
    latitude = np.radians((0.5 - (np.arange(height_px) + 0.5) / height_px) * 180.0)
    texel_east = 2.0 * math.pi * RADIUS_EQUATORIAL_M * np.maximum(np.cos(latitude), 0.02) / width
    texel_south = math.pi * RADIUS_POLAR_M / height_px
    slope_east = east / (2.0 * texel_east[:, None])
    slope_south = south / (2.0 * texel_south)
    # The map is oblique Cassini data above 80 degrees and the east texel shrinks to nothing: fade the relief out there.
    polar = smoothstep(88.0, 78.0, np.abs(np.degrees(latitude)))[:, None]
    slope_east *= polar
    slope_south *= polar

    def encode(slope):
        magnitude = np.sqrt(np.clip(np.abs(slope) / RELIEF_SLOPE_MAX, 0.0, 1.0))
        return np.clip(np.rint((0.5 + np.sign(slope) * magnitude * 0.5) * 255.0), 0, 255).astype(np.uint8)

    low, high = float(heights.min()), float(heights.max())
    alpha = np.clip(np.rint((heights - low) / max(high - low, 1e-6) * 255.0), 0, 255).astype(np.uint8)
    pixels = np.dstack([encode(slope_east), encode(slope_south), np.full_like(alpha, 255), alpha])
    stats = {
        "relief_width": width, "relief_height": height_px,
        "relief_height_min_m": round(low, 1), "relief_height_max_m": round(high, 1),
        "relief_slope_max": RELIEF_SLOPE_MAX,
        "relief_max_slope": round(float(np.max(np.sqrt(slope_east ** 2 + slope_south ** 2))), 4),
    }
    return pixels, stats


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--albedo", required=True, help="the imported gas giant albedo")
    parser.add_argument("--output", required=True)
    parser.add_argument("--width", type=int, default=1024)
    parser.add_argument("--turbulence", type=float, default=8.0, help="rms cross-contour speed, m/s, where the weight is 1")
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--detail-output", help="also bake the flow-aligned detail map here")
    parser.add_argument("--detail-width", type=int, default=4096)
    parser.add_argument("--relief-output", help="also bake the cloud-top relief (normal+height) map here; needs the detail")
    parser.add_argument("--relief-width", type=int, default=2048)
    parser.add_argument("--polar-output", help="also bake the polar cap albedo atlas here")
    parser.add_argument("--polar-flow-output", help="and the polar cap flow atlas here")
    parser.add_argument("--polar-size", type=int, default=1024)
    args = parser.parse_args()
    if args.relief_output and not args.detail_output:
        parser.error("--relief-output needs --detail-output")
    if bool(args.polar_output) != bool(args.polar_flow_output):
        parser.error("--polar-output and --polar-flow-output go together")

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
    relief_stats = {}
    if args.detail_output:
        detail = bake_detail(flow_u, flow_v, args.detail_width, rng)
        Image.fromarray(encode_detail(detail), mode="L").save(args.detail_output, optimize=True)
        detail_size = detail.shape[::-1]
        if args.relief_output:
            relief, relief_stats = bake_relief(args.albedo, detail, args.relief_width)
            Image.fromarray(relief, mode="RGBA").save(args.relief_output, optimize=True)
    polar_stats = {}
    if args.polar_output:
        polar_albedo, polar_flow, polar_stats = bake_polar(args.albedo, args.polar_size, np.random.default_rng(args.seed + 1))
        Image.fromarray(polar_albedo, mode="RGBA").save(args.polar_output, optimize=True)
        Image.fromarray(polar_flow, mode="RGBA").save(args.polar_flow_output, optimize=True)

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
        **relief_stats,
        **polar_stats,
    }))


if __name__ == "__main__":
    main()
