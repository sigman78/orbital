#!/usr/bin/env python3
"""Fits the Milky Way as a cloud of Gaussian splats on the sphere to ESO's all-sky panorama.

The reference (eso0932a, ESO/S. Brunier, CC BY 4.0, a galactic equirectangular
photograph) is first freed of its stars by median filtering at two scales and
of its sky floor by a percentile subtraction, then approximated by a few
hundred anisotropic Gaussians on the sphere: each has a centre direction, two
widths along a rotated tangent frame (in angle, so the shape is right over
the whole sphere), and a signed RGB amplitude, negative ones carving the dust
lanes out of the positive ones. The parameters are fitted by gradient descent
(PyTorch, on the GPU when there is one) on the area-weighted squared log
ratio over a small floor, so the faint outer band counts as much as the bulge
and a lifted dark sky costs as much as a wrong bulge. The shader evaluates the same sum per
sky pixel, so nothing is sampled: no texel lattice at any zoom, no star
residue, and the whole sky costs a few tens of kilobytes.

Output: a record file the renderer reads as the splat buffer, 16-byte header
("SPLT", count, record size 64, table length in uints) then per splat four float4:
  centre.xyz, sigma1 | axis1.xyz, sigma2 | r, g, b, cutoff (cos of the reach) | 0, 0, 0, 0
in galactic xyz (x toward the centre, z the north pole); axis2 = cross(centre, axis1);
then the cell table: the splats bucketed by the cells of a longitude/latitude grid
they reach into, so a pixel sums only its cell's list: cells_lon, cells_lat, then
per cell (offset, count) into the index list that follows. The splats are ordered
by energy so the renderer can draw the first n of them. A preview PNG (target
above, fit below) is written next to it for review; --from-records rebuilds the
table for an existing file without fitting.

Requires numpy, Pillow and torch; eight minutes for the default 1,024 splats on a GTX 1080 Ti,
an hour or so on twenty cores (use --threads and a smaller --batch there).
"""
import argparse
import os
import json
import math
import struct
import time

import numpy as np
import torch
from PIL import Image, ImageFilter

GAMMA = 2.2
SIGMA_MAX = math.radians(45.0)
SIGMA_MIN = math.radians(.3)     # set by --sigma-min; the band's sharpness, and the fit width must resolve it
STAGE_COUNTS = [256, 512, 1024, 1536, 2048, 3072]          # the growth ladder before the final count
STAGE_STEPS = [600, 400, 600, 800, 1000, 1200]              # steps per ladder stage (the final stage's from --steps)
REACH_SIGMAS = 3.5   # the shader skips a splat beyond this many of its larger width
ASPECT_MAX = 2.5     # a splat's widths differ by at most this
LOG_FLOOR = .003     # the loss's floor, a hundredth of the band: below it errors stop counting
CELLS_LON, CELLS_LAT = 48, 24   # the cell table's grid, 7.5 degree cells


def prepare(path, width, kind, decades=0.0, blur=0.0):
    """Star-free, floor-subtracted linear target at width x width/2, plus a working-width copy for the preview.

    kind "photo": a photograph with point stars and blacked-out patches (ESO's panorama),
    median filtered and masked; "map": an integrated-starlight map such as Gaia's, which
    has neither, so only a light floor is taken and its halo is kept."""
    image = Image.open(path)
    if kind == "map":
        working = max(min(image.width, 4096), width)  # capped: the prep and the preview are in double precision
        if image.mode == "RGBA" and np.asarray(image)[0, 0, 3] == 0:
            raw = unhammer(np.asarray(image.convert("RGBA")).astype(np.float64), working)  # ESA's Hammer ellipse on transparency
        else:
            image = image.convert("RGB")
            raw = np.asarray(image).astype(np.float64)
            # A rendered map's outermost columns and rows are often a border (ESA's cartesian
            # JPEG has a lighter column on each side, mean 35 against 23 beside it); fitted, it
            # becomes a line of splats at the seam. Replace them with their neighbours.
            raw[:, 0] = raw[:, 1]
            raw[:, -1] = raw[:, -2]
            raw[0] = raw[1]
            raw[-1] = raw[-2]
            if raw.shape[1] != working:
                image = Image.fromarray(np.clip(raw, 0, 255).astype(np.uint8)).resize((working, working // 2), Image.LANCZOS)
                raw = np.asarray(image).astype(np.float64)
        valid = np.ones(raw.shape[:2], dtype=bool)
        if decades > 0.0:
            # ESA's map is a logarithmic stretch of the flux: undo it over this many decades,
            # so the star-speckle sky falls to a fraction of a percent of the bulge.
            linear = 10.0 ** (decades * (raw / 255.0 - 1.0))
        else:
            linear = (raw / 255.0) ** GAMMA
        floor = np.percentile(linear.reshape(-1, 3), .5, axis=0)
        linear = np.clip(linear - floor, 0.0, None)
        if blur > 0.0:
            # The map is star speckle; a low-pass at the fit's scale gives the splats a mean to fit
            # instead of stars to chase (its radius in pixels of the working width).
            linear = gaussian_blur(linear, blur / 360.0 * working)
        linear /= np.percentile(linear[..., 1], 99.5)
        shrink = lambda field: np.asarray(Image.fromarray(field.astype(np.float32)).resize((width, width // 2), Image.BOX)).astype(np.float64)
        return np.stack([shrink(linear[..., c]) for c in range(3)], axis=-1), shrink(valid.astype(np.float64)) > .999, linear
    image = image.convert("RGB")
    # Stars are a few pixels across at 6000 wide: a 7-pixel median (0.4 degrees) removes
    # most; the area-average to the working width blurs the rest below a second median,
    # 5 pixels at 1024 wide (1.8 degrees, which also takes the finest lanes) and 3 at 2048
    # (0.5 degrees), so a finer fit keeps what it can resolve.
    working = max(1024, width)
    image = image.filter(ImageFilter.MedianFilter(7)).resize((working, working // 2), Image.BOX)
    image = image.filter(ImageFilter.MedianFilter(5 if working <= 1024 else 3))
    raw = np.asarray(image).astype(np.float64)
    # The panorama has blacked-out patches (the photographer's horizon and gaps): pure
    # black regions, grown by a margin, are left out of the fit and of the floor.
    valid = Image.fromarray((raw.max(axis=-1) > 2.0).astype(np.uint8) * 255).filter(ImageFilter.MinFilter(15))
    valid = np.asarray(valid) > 127
    linear = (raw / 255.0) ** GAMMA
    floor = np.percentile(linear[valid], 3.0, axis=0)
    linear = np.clip(linear - floor, 0.0, None)
    linear /= np.percentile(linear[valid][:, 1], 99.5)
    linear[~valid] = 0.0
    shrink = lambda field: np.asarray(Image.fromarray(field.astype(np.float32)).resize((width, width // 2), Image.BOX)).astype(np.float64)
    small = np.stack([shrink(linear[..., c]) for c in range(3)], axis=-1)
    return small, shrink(valid.astype(np.float64)) > .999, linear


def gaussian_blur(field, sigma_pixels):
    """Separable Gaussian low-pass of an (h, w, channels) float array, periodic in x (longitude), mirrored in y."""
    try:
        from scipy.ndimage import gaussian_filter
        return gaussian_filter(field, sigma=(sigma_pixels, sigma_pixels, 0), mode=("reflect", "wrap", "reflect"))
    except ImportError:
        radius = int(math.ceil(3.0 * sigma_pixels))
        kernel = np.exp(-.5 * (np.arange(-radius, radius + 1) / sigma_pixels) ** 2)
        kernel /= kernel.sum()
        padded = np.concatenate([field[:, -radius:], field, field[:, :radius]], axis=1)
        out = np.stack([np.apply_along_axis(lambda row: np.convolve(row, kernel, mode="valid"), 1, padded[..., c]) for c in range(field.shape[2])], axis=-1)
        padded = np.concatenate([out[radius:0:-1], out, out[-2:-radius - 2:-1]], axis=0)
        return np.stack([np.apply_along_axis(lambda col: np.convolve(col, kernel, mode="valid"), 0, padded[..., c]) for c in range(field.shape[2])], axis=-1)


def unhammer(rgba, width):
    """Resamples ESA's Hammer-projected all-sky image (the ellipse on a transparent
    ground, galactic centre in the middle, longitude increasing to the left) to a
    width x width/2 equirectangular RGB image by the projection's forward formula."""
    height = width // 2
    src_h, src_w = rgba.shape[:2]
    u = (np.arange(width) + .5) / width
    v = (np.arange(height) + .5) / height
    lon = ((.5 - u) * 2.0 * math.pi)[None, :]
    lat = ((.5 - v) * math.pi)[:, None]
    denominator = np.sqrt(1.0 + np.cos(lat) * np.cos(lon / 2.0))
    x = 2.0 * math.sqrt(2.0) * np.cos(lat) * np.sin(lon / 2.0) / denominator   # -2 sqrt 2 .. 2 sqrt 2, east positive
    y = math.sqrt(2.0) * np.sin(lat) / denominator                             # -sqrt 2 .. sqrt 2, north positive
    column = (.5 - x / (4.0 * math.sqrt(2.0))) * src_w - .5                     # longitude increasing to the left
    row = (.5 - y / (2.0 * math.sqrt(2.0))) * src_h - .5
    c0 = np.clip(np.floor(column).astype(int), 0, src_w - 2)
    r0 = np.clip(np.floor(row).astype(int), 0, src_h - 2)
    fc = np.clip(column - c0, 0.0, 1.0)[..., None]
    fr = np.clip(row - r0, 0.0, 1.0)[..., None]
    rgb = rgba[..., :3]
    sample = (rgb[r0, c0] * (1 - fc) * (1 - fr) + rgb[r0, c0 + 1] * fc * (1 - fr)
              + rgb[r0 + 1, c0] * (1 - fc) * fr + rgb[r0 + 1, c0 + 1] * fc * fr)
    return sample


def directions(width, height):
    u = (np.arange(width) + .5) / width
    v = (np.arange(height) + .5) / height
    lon = (.5 - u) * 2.0 * math.pi   # increasing to the left
    lat = (.5 - v) * math.pi
    cos_b, sin_b = np.cos(lat)[:, None], np.sin(lat)[:, None]
    d = np.stack([cos_b * np.cos(lon)[None, :], cos_b * np.sin(lon)[None, :], sin_b * np.ones((1, width))], axis=-1)
    return d.reshape(-1, 3), np.repeat(np.cos(lat), width)


class Splats(torch.nn.Module):
    def __init__(self, count, target, dirs, weights, rng, width, height, init="brightness"):
        super().__init__()
        # The coarse cloud's centres: drawn by the target's brightness (the halo and bulge, the
        # peer machine's proven recipe) or by its gradient (Image-GS; on lanes and edges).
        if init == "gradient":
            p = gradient_probability(target, weights, width, height)
        else:
            luminance = target[:, 1] * weights
            p = luminance / luminance.sum()
        picks = rng.choice(len(p), size=count, replace=True, p=p)
        d = dirs[picks]
        lon = np.arctan2(d[:, 1], d[:, 0]) + rng.normal(0, .02, count)
        lat = np.arcsin(np.clip(d[:, 2], -1, 1)) + rng.normal(0, .02, count)
        sigma = np.exp(rng.uniform(math.log(math.radians(2.0)), math.log(math.radians(12.0)), (count, 2)))
        as_tensor = lambda a: torch.tensor(np.asarray(a), dtype=torch.float32)
        self.lon = torch.nn.Parameter(as_tensor(lon))
        self.lat = torch.nn.Parameter(as_tensor(lat))
        self.log_sigma = torch.nn.Parameter(as_tensor(np.log(sigma)))
        self.theta = torch.nn.Parameter(as_tensor(rng.uniform(0, math.pi, count)))
        self.amplitude = torch.nn.Parameter(as_tensor(target[picks] * .1 + .01))

    @classmethod
    def from_records(cls, centre, sigma, axis1, amplitude):
        """A model holding an existing fit's parameters (for previews of record files)."""
        self = cls.__new__(cls)
        torch.nn.Module.__init__(self)
        lon, lat = np.arctan2(centre[:, 1], centre[:, 0]), np.arcsin(np.clip(centre[:, 2], -1, 1))
        east = np.stack([-np.sin(lon), np.cos(lon), np.zeros_like(lon)], -1)
        north = np.stack([-np.sin(lat) * np.cos(lon), -np.sin(lat) * np.sin(lon), np.cos(lat)], -1)
        theta = np.arctan2((axis1 * north).sum(-1), (axis1 * east).sum(-1))
        as_tensor = lambda a: torch.tensor(np.asarray(a), dtype=torch.float32)
        self.lon, self.lat = torch.nn.Parameter(as_tensor(lon)), torch.nn.Parameter(as_tensor(lat))
        self.log_sigma = torch.nn.Parameter(as_tensor(np.log(np.maximum(sigma, 1e-6))))
        self.theta = torch.nn.Parameter(as_tensor(theta))
        self.amplitude = torch.nn.Parameter(as_tensor(amplitude))
        return self

    @classmethod
    def from_state(cls, state):
        """A model from a saved checkpoint (the .pt written beside every export)."""
        self = cls.__new__(cls)
        torch.nn.Module.__init__(self)
        for name in ("lon", "lat", "log_sigma", "theta", "amplitude"):
            setattr(self, name, torch.nn.Parameter(state[name].clone().float()))
        return self

    def frames(self):
        cos_l, sin_l = torch.cos(self.lon), torch.sin(self.lon)
        cos_b, sin_b = torch.cos(self.lat), torch.sin(self.lat)
        centre = torch.stack([cos_b * cos_l, cos_b * sin_l, sin_b], -1)
        east = torch.stack([-sin_l, cos_l, torch.zeros_like(sin_l)], -1)
        north = torch.stack([-sin_b * cos_l, -sin_b * sin_l, cos_b], -1)
        cos_t, sin_t = torch.cos(self.theta)[:, None], torch.sin(self.theta)[:, None]
        axis1 = cos_t * east + sin_t * north
        axis2 = -sin_t * east + cos_t * north
        # Widths within the limits, the aspect within ASPECT_MAX so no splat is a brush stroke.
        sigma1 = torch.exp(self.log_sigma[:, 0]).clamp(SIGMA_MIN, SIGMA_MAX)
        ratio = torch.exp(self.log_sigma[:, 1] - self.log_sigma[:, 0]).clamp(1.0 / ASPECT_MAX, ASPECT_MAX)
        sigma = torch.stack([sigma1, (sigma1 * ratio).clamp(SIGMA_MIN, SIGMA_MAX)], -1)
        return centre, axis1, axis2, sigma

    def forward(self, dirs):
        centre, axis1, axis2, sigma = self.frames()
        cos_a = (dirs @ centre.T).clamp(-1.0, 1.0)           # pixels x splats
        sin_a = torch.sqrt((1.0 - cos_a * cos_a).clamp_min(1e-12))
        angle = torch.atan2(sin_a, cos_a)
        # Azimuthal equidistant coordinates: the tangent-plane direction scaled to the angle.
        scale = torch.where(sin_a > 1e-4, angle / sin_a, torch.ones_like(angle))
        u = (dirs @ axis1.T) * scale / sigma[:, 0]
        v = (dirs @ axis2.T) * scale / sigma[:, 1]
        return torch.exp(-.5 * (u * u + v * v)) @ self.amplitude

    @torch.no_grad()
    def project_widths(self):
        """Latent widths back to the represented (clamped) values, so a splat at a clamp can leave it."""
        self.log_sigma.copy_(self.frames()[3].log())

    @torch.no_grad()
    def grow(self, count, target_t, dirs_t, width, rng):
        """Adds splats up to the count at coherent residuals (the peer machine's adaptive recipe):
        the signed log residual is averaged over 4x4 blocks before squaring, so isolated pixels
        and speckle count for little and lanes for much; new splats start narrow (1.25 to 2.75
        floors, mildly elliptical) with zero amplitude, so growing preserves the image and the
        optimiser learns their sign."""
        prediction = predict(self, dirs_t)
        log_residual = torch.log(target_t + LOG_FLOOR) - torch.log(prediction.clamp_min(-.9 * LOG_FLOOR) + LOG_FLOOR)
        field = log_residual.reshape(width // 2, width, 3).permute(2, 0, 1)[None]
        pooled = torch.nn.functional.avg_pool2d(field, 4, 4)[0]
        score = pooled.square().mean(0).cpu().numpy()
        seed_dirs, seed_weights = directions(width // 4, width // 8)
        score = score.reshape(-1) * seed_weights
        score = np.minimum(score, np.percentile(score, 99.7)) + 1e-12
        add = count - len(self.lon)
        picks = rng.choice(len(score), size=add, replace=False, p=score / score.sum())
        chosen = seed_dirs[picks]
        narrow = SIGMA_MIN * np.exp(rng.uniform(math.log(1.25), math.log(2.75), add))
        major = narrow * rng.uniform(1.1, 1.8, add)
        extra = {"lon": np.arctan2(chosen[:, 1], chosen[:, 0]), "lat": np.arcsin(np.clip(chosen[:, 2], -1, 1)),
                 "log_sigma": np.log(np.stack([major, narrow], axis=1)),
                 "theta": rng.uniform(-math.pi / 2, math.pi / 2, add), "amplitude": np.zeros((add, 3))}
        for name, value in extra.items():
            old = getattr(self, name)
            new = torch.as_tensor(value, dtype=old.dtype, device=old.device)
            setattr(self, name, torch.nn.Parameter(torch.cat([old.detach(), new])))


@torch.no_grad()
def predict(model, dirs_t, chunk=16384):
    return torch.cat([model(dirs_t[i:i + chunk]) for i in range(0, len(dirs_t), chunk)])


def gradient_probability(target, weights, width, height, uniform=.3):
    """Sampling probability per direction: the target's gradient magnitude (linear domain) mixed
    with a uniform share, area weighted (Image-GS's initialisation)."""
    field = target.reshape(height, width, 3)[..., 1]
    gy, gx = np.gradient(field)
    magnitude = np.hypot(gx, gy).reshape(-1) * weights
    magnitude /= magnitude.sum()
    even = weights / weights.sum()
    p = (1.0 - uniform) * magnitude + uniform * even
    return p / p.sum()


def log_ratio(prediction, log_target):
    """The error as a log ratio over a floor: relative inside the band, and a lifted dark sky
    (a broad splat's tail) costs as much as a wrong bulge, where a squared or square-root
    error would forgive a haze of a hundredth over the whole sky."""
    return torch.log(prediction.clamp_min(-.9 * LOG_FLOOR) + LOG_FLOOR) - log_target


def train_stage(model, target_t, dirs_t, weights_t, steps, batch, seed, coarse, log):
    """Adam on the area-weighted log-ratio loss for one stage. The coarse stage (the first cloud)
    moves fast; every later stage refines all splats together at a fifth of the rates, and the
    latent widths are projected back into range after each step."""
    generator = torch.Generator().manual_seed(seed)
    log_target = torch.log(target_t + LOG_FLOOR)
    lr = (3e-3, 1e-2, 1e-2) if coarse else (5e-4, 3e-3, 1e-3)
    optimizer = torch.optim.Adam([
        {"params": [model.lon, model.lat], "lr": lr[0]},
        {"params": [model.log_sigma, model.theta], "lr": lr[1]},
        {"params": [model.amplitude], "lr": lr[2]},
    ])
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, steps, eta_min=5e-5)
    started = time.time()
    for step in range(steps):
        pick = torch.randint(0, len(dirs_t), (batch,), generator=generator).to(dirs_t.device)
        error = log_ratio(model(dirs_t[pick]), log_target[pick])
        loss = (weights_t[pick, None] * error * error).mean()
        if not torch.isfinite(loss):
            raise RuntimeError("non-finite loss")
        optimizer.zero_grad(set_to_none=True)
        loss.backward()
        optimizer.step()
        model.project_widths()
        scheduler.step()
        if step % 100 == 0 or step == steps - 1:
            log(f"count {len(model.lon)} step {step}/{steps} loss {loss.item():.6f} ({time.time() - started:.0f} s)")
    prediction = predict(model, dirs_t)
    error = log_ratio(prediction, log_target)
    return (weights_t[:, None] * error * error).mean().item(), prediction


def cell_table(centre, reach):
    """Per grid cell, the splats whose reach cone meets the cell's bounding cone."""
    cell_lon = 2.0 * math.pi / CELLS_LON
    cell_lat = math.pi / CELLS_LAT
    half_diagonal = .5 * math.hypot(cell_lon, cell_lat)
    reach_angle = np.arccos(np.clip(reach, -1.0, 1.0))
    entries, lists, total = [], [], 0
    for j in range(CELLS_LAT):
        lat = (.5 - (j + .5) / CELLS_LAT) * math.pi
        for i in range(CELLS_LON):
            lon = (.5 - (i + .5) / CELLS_LON) * 2.0 * math.pi   # the map's layout: longitude increasing to the left
            cell = np.array([math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon), math.sin(lat)])
            # Near the poles the cell's own extent in longitude shrinks, so the bound is wide there; that is safe.
            angle = np.arccos(np.clip(centre @ cell, -1.0, 1.0))
            members = np.nonzero(angle < reach_angle + half_diagonal)[0]
            entries.append((total, len(members)))
            lists.append(members)
            total += len(members)
    table = [CELLS_LON, CELLS_LAT] + [v for e in entries for v in e] + [int(k) for l in lists for k in l]
    return np.array(table, dtype=np.uint32)


def write_records(path, centre, sigma, axis1, amplitude, reach):
    # Ordered by energy (amplitude times area), so the renderer can draw the first n and keep what matters.
    energy = np.abs(amplitude).max(axis=1) * sigma[:, 0] * sigma[:, 1]
    order = np.argsort(-energy)
    centre, sigma, axis1, amplitude, reach = centre[order], sigma[order], axis1[order], amplitude[order], reach[order]
    table = cell_table(centre, reach)
    with open(path, "wb") as f:
        f.write(struct.pack("<4sIII", b"SPLT", len(centre), 64, len(table)))
        for k in range(len(centre)):
            f.write(struct.pack("<16f", *centre[k], sigma[k, 0], *axis1[k], sigma[k, 1], *amplitude[k], reach[k], 0.0, 0.0, 0.0, 0.0))
        f.write(table.tobytes())
    return len(table)


def read_records(path):
    with open(path, "rb") as f:
        magic, count, record, _ = struct.unpack("<4sIII", f.read(16))
        assert magic == b"SPLT" and record == 64
        raw = np.frombuffer(f.read(count * 64), dtype=np.float32).reshape(count, 16)
    return raw[:, 0:3], raw[:, [3, 7]], raw[:, 4:7], raw[:, 8:11], raw[:, 11]


PREVIEW_WIDTH = 2048


def write_preview(model, target_full, path, device):
    """Target above, fit below, at most PREVIEW_WIDTH wide (the fit is evaluated at that size)."""
    full_h, full_w = target_full.shape[:2]
    if full_w > PREVIEW_WIDTH:
        shrink = lambda field: np.asarray(Image.fromarray(field.astype(np.float32)).resize((PREVIEW_WIDTH, PREVIEW_WIDTH // 2), Image.BOX))
        target_full = np.stack([shrink(target_full[..., c]) for c in range(3)], axis=-1)
        full_h, full_w = target_full.shape[:2]
    dirs, _ = directions(full_w, full_h)
    with torch.no_grad():
        dirs_t = torch.tensor(dirs, dtype=torch.float32, device=device)
        fit = torch.cat([model(dirs_t[i:i + 65536]) for i in range(0, len(dirs), 65536)]).cpu().numpy().reshape(full_h, full_w, 3)
    encode = lambda x: (np.clip(x, 0, 1) ** (1.0 / GAMMA) * 255).astype(np.uint8)
    Image.fromarray(np.concatenate([encode(target_full), encode(fit)], axis=0)).save(path)


def main():
    global CELLS_LON, CELLS_LAT
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", help="the reference image (not needed with --from-records)")
    parser.add_argument("--kind", choices=["photo", "map"], default="map", help="photo: a panorama with point stars to remove (ESO); map: integrated starlight (Gaia)")
    parser.add_argument("--decades", type=float, default=0.0, help="map only: undo a logarithmic stretch over this many decades of flux (0: treat as gamma 2.2)")
    parser.add_argument("--blur", type=float, default=0.0, help="map only: Gaussian low-pass of the map in degrees before fitting, so the fit chases the mean and not the star speckle; use about 1.5 times the width floor, or stars stay fittable")
    parser.add_argument("--output", required=True, help="the splat record file")
    parser.add_argument("--preview", help="PNG of target and fit for review")
    parser.add_argument("--count", type=int, default=4096, help="splats in the final cloud")
    parser.add_argument("--counts", help="explicit stage counts, ascending, e.g. 256,512,1024,1536,2048 (default: the standard ladder up to --count)")
    parser.add_argument("--stage-steps", help="steps per stage, matching --counts (default: the standard ladder's)")
    parser.add_argument("--resume", help="a .pt checkpoint to continue from; stages below its count are skipped")
    parser.add_argument("--init", choices=["brightness", "gradient"], default="brightness", help="the coarse cloud's placement")
    parser.add_argument("--core-weight", action="store_true", help="weight the loss by the square root of brightness to hold the bright core")
    parser.add_argument("--compact", action="store_true", help="also write a -g32 file with a 32x16 cell table")
    parser.add_argument("--steps", type=int, default=1500, help="steps of the final stage (plain fit: all steps)")
    parser.add_argument("--batch", type=int, default=32768, help="directions per step; 32768 keeps 4096 splats under 9 GB")
    parser.add_argument("--width", type=int, default=2048, help="fit resolution (height is half); about three pixels per sigma-min")
    parser.add_argument("--sigma-min", type=float, default=.3, help="narrowest splat width in degrees")
    parser.add_argument("--aspect-max", type=float, default=2.5, help="a splat's widths differ by at most this; lower for rounder, less streaky splats")
    parser.add_argument("--progressive", action=argparse.BooleanOptionalAction, default=True, help="grow the cloud in stages at the coherent residuals (the adaptive recipe); --no-progressive fits all splats at once")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--threads", type=int, default=0)
    parser.add_argument("--from-records", help="rebuild the cell table for this record file instead of fitting")
    args = parser.parse_args()
    if args.from_records:
        centre, sigma, axis1, amplitude, reach = read_records(args.from_records)
        table_length = write_records(args.output, centre, sigma, axis1, amplitude, reach)
        if args.preview and args.source:  # the preview of an existing fit against its source
            device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
            model = Splats.from_records(centre, sigma, axis1, amplitude).to(device)
            _, _, target_full = prepare(args.source, args.width, args.kind, args.decades, args.blur)
            write_preview(model, target_full, args.preview, device)
        print(json.dumps({"count": len(centre), "table_uints": int(table_length), "list_entries": int(table_length - 2 - 2 * CELLS_LON * CELLS_LAT),
                          "sigma_min_deg": round(float(np.degrees(sigma.min())), 2), "sigma_max_deg": round(float(np.degrees(sigma.max())), 2)}))
        return
    if not args.source:
        parser.error("--source is required to fit")
    if args.threads:
        torch.set_num_threads(args.threads)
    global SIGMA_MIN, ASPECT_MAX
    SIGMA_MIN = math.radians(args.sigma_min)
    ASPECT_MAX = args.aspect_max
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    log = lambda s: print(s, flush=True)
    log(f"fitting on {device}")

    target_small, valid, target_full = prepare(args.source, args.width, args.kind, args.decades, args.blur)
    height = args.width // 2
    dirs, weights = directions(args.width, height)
    weights *= valid.reshape(-1)
    target = target_small.reshape(-1, 3)
    rng = np.random.default_rng(args.seed)
    as_tensor = lambda a: torch.tensor(np.asarray(a), dtype=torch.float32, device=device)
    target_t, dirs_t, weights_t = as_tensor(target), as_tensor(dirs), as_tensor(weights)
    if args.core_weight:
        # The bright core is a compact patch of a few pixels' area that the area weight alone lets the
        # fit halve; weighting by the square root of the brightness (bulge 1, band 0.5, halo 0.2) holds it.
        weights_t = weights_t * torch.sqrt(target_t[:, 1].clamp_min(.01))
    # The stages: counts and steps, the cloud grown between them; plain fitting is one stage.
    if args.progressive:
        counts = [int(c) for c in args.counts.split(",")] if args.counts else [c for c in STAGE_COUNTS if c < args.count] + [args.count]
        stage_steps = [int(n) for n in args.stage_steps.split(",")] if args.stage_steps else STAGE_STEPS[:len(counts) - 1] + [args.steps]
    else:
        counts, stage_steps = [args.count], [args.steps]
    assert len(counts) == len(stage_steps) and counts == sorted(set(counts)), "counts and stage steps must match, ascending"
    resumed = 0
    if args.resume:
        state = torch.load(args.resume, map_location="cpu", weights_only=True)
        model = Splats.from_state(state).to(device)
        resumed = len(model.lon)
        counts = [c for c in counts if c >= resumed] or [resumed]
        stage_steps = stage_steps[-len(counts):]
        log(f"resumed {resumed} splats from {args.resume}")
    else:
        model = Splats(counts[0], target, dirs, weights, rng, args.width, height, args.init).to(device)
    started = time.time()
    for stage, (count, steps) in enumerate(zip(counts, stage_steps)):
        if count > len(model.lon):
            model.grow(count, target_t, dirs_t, args.width, rng)
            log(f"grew to {count} splats at the coherent residuals")
        if device.type == "cuda":
            torch.cuda.empty_cache()  # a grown population leaves incompatible cached blocks behind
        final_loss, prediction = train_stage(model, target_t, dirs_t, weights_t, steps, args.batch, args.seed + stage,
                                             stage == 0 and not args.resume, log)
        log(f"stage {stage}: {count} splats, loss {final_loss:.6f} ({time.time() - started:.0f} s)")
    prediction = prediction.cpu().numpy()
    torch.save({k: v.detach().cpu() for k, v in model.state_dict().items()}, os.path.splitext(args.output)[0] + ".pt")
    with torch.no_grad():
        centre, axis1, axis2, sigma = [t.cpu().numpy() for t in model.frames()]
        amplitude = model.amplitude.cpu().numpy()
    reach = np.cos(np.minimum(REACH_SIGMAS * sigma.max(axis=1), math.pi - 1e-3))
    table_length = write_records(args.output, centre, sigma, axis1, amplitude, reach)
    if args.compact:  # the same records with a 32x16 table: a third fewer bytes for a fifth more work per pixel
        CELLS_LON, CELLS_LAT = 32, 16
        write_records(os.path.splitext(args.output)[0] + "-g32.bin", centre, sigma, axis1, amplitude, reach)
        CELLS_LON, CELLS_LAT = 48, 24

    if args.preview:
        write_preview(model, target_full, args.preview, device)

    print(json.dumps({"count": len(centre), "stages": counts, "stage_steps": stage_steps, "resume": args.resume, "loss": round(final_loss, 6),
                      "list_entries": int(table_length - 2 - 2 * CELLS_LON * CELLS_LAT),
                      "sigma_min_deg": round(float(np.degrees(sigma.min())), 2), "sigma_floor_deg": args.sigma_min, "aspect_max": args.aspect_max, "decades": args.decades, "blur_deg": args.blur, "progressive": args.progressive, "init": args.init, "core_weight": args.core_weight, "batch": args.batch, "width": args.width, "sigma_max_deg": round(float(np.degrees(sigma.max())), 2),
                      "negative": int((amplitude.mean(axis=1) < 0).sum())}))


if __name__ == "__main__":
    main()
