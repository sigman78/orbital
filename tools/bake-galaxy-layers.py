#!/usr/bin/env python3
"""Fit composable Gaia sky textures with Adam; compare resolutions after actual BC7 encoding.

Requires numpy, scipy, Pillow and torch. The optional texture_tools target supplies
bc7_compress. Outputs PNG fallbacks, BC7 OTEX caches, and a self-contained visual report.
The source is a display-stretched map, not calibrated radiance; fit in its display space
and square colour layers at composition time to retain precision in the faint halo.
"""
import argparse
import base64
import shutil
import hashlib
import io
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import time

import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter, median_filter
import torch
import torch.nn.functional as F

ROOT = Path(__file__).resolve().parent.parent
# All strip textures span latitude +/-45 degrees; the outer 9 degrees fade to zero.
BAND_HALF = .25
PRESETS = {"tiny": (32, 128, 512), "compact": (64, 256, 768), "small": (64, 256, 1024),
           "balanced": (64, 512, 1536), "fine": (128, 512, 2048)}


def fnv(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return value


def save_png(path, values):
    Image.fromarray(np.uint8(np.clip(values.squeeze(axis=2) if values.shape[2] == 1 else values, 0, 1) * 255 + .5)).save(path, optimize=True)


def tensor(values, device):
    return torch.as_tensor(values, dtype=torch.float32, device=device).permute(2, 0, 1)[None]


def array(value):
    return value.detach().cpu().numpy()[0].transpose(1, 2, 0)


def sample(layer, width, height, strip=False):
    # Match GPU normalized coordinates and wrap-U/clamp-V bilinear sampling exactly.
    u = (torch.arange(width, device=layer.device) + .5) / width
    v = (torch.arange(height, device=layer.device) + .5) / height
    if strip:
        v = (v - (.5 - BAND_HALF)) / (2 * BAND_HALF)
    padded = torch.cat((layer[..., -1:], layer, layer[..., :1]), dim=3)
    u = (u * layer.shape[3] + 1) / padded.shape[3]
    yy, xx = torch.meshgrid(v * 2 - 1, u * 2 - 1, indexing="ij")
    return F.grid_sample(padded, torch.stack((xx, yy), dim=-1)[None],
                         mode="bilinear", padding_mode="border", align_corners=False)


def mask(height, device):
    distance = abs((torch.arange(height, device=device) + .5) / height - .5)
    t = ((BAND_HALF - distance) / .05).clamp(0, 1)
    return (t * t * (3 - 2 * t))[None, None, :, None]


def compose(layers, width, height, cloud_gain=1., filament_gain=1.):
    low, clouds, dark = layers
    band = mask(height, low.device)
    light = sample(low, width, height).square() + cloud_gain * band * sample(clouds, width, height, True).square()
    # A bounded extinction mask: zero is neutral; no high-frequency bright residual.
    attenuation = (1 - filament_gain * band * sample(dark, width, height, True)).clamp_min(0)
    return (light * attenuation).clamp_min(1e-8).sqrt()


def prepare(path, width):
    raw = np.asarray(Image.open(path).convert("RGB"), dtype=np.float32) / 255
    raw[:, 0], raw[:, -1] = raw[:, 1], raw[:, -2]
    raw[0], raw[-1] = raw[1], raw[-2]
    # Remove isolated point sources before fitting a diffuse background; catalogue stars remain separate.
    clean = median_filter(np.pad(raw, ((0, 0), (1, 1), (0, 0)), mode="wrap"), size=(3, 3, 1), mode="reflect")[:, 1:-1]
    clean = gaussian_filter(clean, (.45, .45, 0), mode=("reflect", "wrap", "nearest"))
    return np.asarray(Image.fromarray(np.uint8(clean * 255 + .5)).resize((width, width // 2), Image.Resampling.LANCZOS), dtype=np.float32) / 255


def fit(target, sizes, steps, device):
    height, width = target.shape[:2]
    ref = tensor(target, device)
    low_init = gaussian_filter(target, (20 * width / 1800, 20 * width / 1800, 0), mode=("reflect", "wrap", "nearest")) * .8
    low = F.interpolate(tensor(low_init, device), size=(sizes[0] // 2, sizes[0]), mode="area")
    cloud_init = np.sqrt(np.maximum(target ** 2 - low_init ** 2, .0001))
    cloud = F.interpolate(tensor(cloud_init[height // 4:height * 3 // 4], device), size=(sizes[1] // 4, sizes[1]), mode="area")
    dark = torch.full((1, 1, sizes[2] // 4, sizes[2]), .12, device=device)
    params = [torch.nn.Parameter(value.clamp(.001, .999)) for value in (low, cloud, dark)]
    optimizer = torch.optim.Adam(params, lr=.015)
    lat = (torch.arange(height, device=device) + .5) / height
    weights = torch.cos((lat - .5) * math.pi)[None, None, :, None]
    history = []
    for step in range(steps):
        # Quantization-aware straight-through gradients during the final quarter.
        layers = [p + ((p * 255).round() / 255 - p).detach() if step > steps * .75 else p for p in params]
        pred = compose(layers, width, height)
        error = pred - ref
        loss = (weights * error.square()).mean()
        # Relative error keeps faint coloured clouds and the halo relevant beside the bulge.
        loss += .015 * (weights * (torch.log(pred + .025) - torch.log(ref + .025)).square()).mean()
        # Constrain decomposition: global colour stays smooth, clouds mid-frequency,
        # and extinction sparse. A fit alone has ambiguous bright/dark cancellation.
        for p, strength in zip(params[:2], (.015, .002)):
            loss += strength * ((p[..., 1:] - p[..., :-1]).square().mean() + (p[..., 1:, :] - p[..., :-1, :]).square().mean())
        loss += .00008 * params[2].mean()
        optimizer.param_groups[0]["lr"] = .001 + .014 * .5 * (1 + math.cos(math.pi * step / steps))
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()
        with torch.no_grad():
            for p in params:
                p.clamp_(0, 1)
        if step == 0 or (step + 1) % 50 == 0 or step + 1 == steps:
            history.append({"step": step + 1, "loss": float(loss.detach())})
            print(f"  step {step + 1}: {history[-1]['loss']:.7f}", flush=True)
    return [array(p) for p in params], history


def bc7_decode(payload, width, height):
    # DDS DX10 header lets Pillow's independent BC7 decoder evaluate actual encoder loss.
    header = [124, 0x81007, height, width, len(payload), 0, 1] + [0] * 11
    header += [32, 4, int.from_bytes(b"DX10", "little"), 0, 0, 0, 0, 0, 0x1000, 0, 0, 0, 0]
    dds = b"DDS " + struct.pack("<31I", *header) + struct.pack("<5I", 98, 3, 0, 1, 0) + payload
    return np.asarray(Image.open(io.BytesIO(dds)).convert("RGB"), dtype=np.float32) / 255


def encode(path, values, encoder):
    save_png(path, values)
    source_hash = fnv(path.read_bytes())
    image = Image.open(path).convert("RGBA")
    base_width, base_height = image.size
    payload = bytearray()
    levels, decoded = 0, None
    while True:
        raw, blocks = path.with_suffix(".rgba"), path.with_suffix(".blocks")
        raw.write_bytes(image.tobytes())
        subprocess.run([str(encoder), str(raw), str(blocks), str(image.width), str(image.height), "thorough", "8", "1", "1", "1", "1"], check=True)
        data = blocks.read_bytes()
        if len(data) != ((image.width + 3) // 4) * ((image.height + 3) // 4) * 16:
            raise ValueError("BC7 encoder returned incorrect block count")
        if decoded is None:
            decoded = bc7_decode(data, image.width, image.height)
        payload.extend(data)
        levels += 1
        if image.size == (1, 1):
            break
        image = downsample(image)
    raw.unlink()
    blocks.unlink()
    checksum = fnv(payload)
    cache = path.with_suffix(".bc7.otex")
    cache.write_bytes(struct.pack("<16I", 0x5845544f, 1, base_width, base_height, levels, 4, 4, 2, 0,
                                 source_hash & 0xffffffff, source_hash >> 32, checksum & 0xffffffff, checksum >> 32, 0, 0, 0) + payload)
    return decoded, cache.stat().st_size


def metrics(pred, target):
    height = target.shape[0]
    weights = np.cos(((np.arange(height) + .5) / height - .5) * np.pi)[:, None, None]
    mse = float(np.mean(weights * (pred - target) ** 2) / weights.mean())
    return {"psnr_db": -10 * math.log10(max(mse, 1e-12)), "mae_255": float(np.mean(abs(pred - target)) * 255),
            "band_mae_255": float(np.mean(abs(pred[height//3:2*height//3] - target[height//3:2*height//3])) * 255)}


def downsample(image):
    # Match assets::kernels::downsample_rgba8, including odd extents and integer rounding.
    pixels = np.asarray(image, dtype=np.uint16)
    height, width = pixels.shape[:2]
    x = np.arange(max(1, width // 2)) * 2
    y = np.arange(max(1, height // 2)) * 2
    x1, y1 = (x + 1) % width, np.minimum(y + 1, height - 1)
    sums = pixels[y[:, None], x] + pixels[y[:, None], x1] + pixels[y1[:, None], x] + pixels[y1[:, None], x1]
    return Image.fromarray(np.uint8((sums + 2) // 4))


def write_report(folder, report):
    def picture(path):
        encoded = base64.b64encode(path.read_bytes()).decode("ascii")
        return f'<img src="data:image/png;base64,{encoded}">'

    rows = []
    panels = []
    for result in report["results"]:
        name = result["preset"]
        rows.append(f'<tr><td>{name}</td><td>{result["bc7_bytes_with_mips"]/1024:.1f} KiB</td>'
                    f'<td>{result["png_bytes"]/1024:.1f} KiB</td><td>{result["bc7"]["psnr_db"]:.2f}</td>'
                    f'<td>{result["bc7"]["band_mae_255"]:.2f}</td></tr>')
        panels.append(f'<h2>{name}</h2><p>Low structure, then add clouds, then darken with filaments.</p>')
        panels.extend(picture(folder / name / f"{label}.png")
                      for label in ("low-only", "without-filaments", "recomposed"))
    for value in report["single_texture_baselines"]:
        rows.append(f'<tr><td>Single {value["width"]}</td><td>{value["bc7_bytes_with_mips"]/1024:.1f} KiB</td>'
                    f'<td>-</td><td>{value["psnr_db"]:.2f}</td><td>{value["band_mae_255"]:.2f}</td></tr>')
    html = """<!doctype html><meta charset="utf-8"><title>Gaia layer fit</title>
<style>body{background:#10141a;color:#ddd;font:16px system-ui;max-width:1200px;margin:40px auto}
img{width:100%}td,th{padding:10px;text-align:left}table{border-collapse:collapse}
tr{border-bottom:1px solid #444}</style><h1>Gaia: three composable textures</h1>
<p>Actual BC7 decode, bilinear reconstruction. RGB = sqrt((low^2 + band * clouds^2) * (1 - band * filaments)).
PSNR is weighted by spherical area; band error is measured within +/-30 degrees.</p>
<p>ESA/Gaia/DPAC; A. Moitinho, A. F. Silva, M. Barros, C. Barata, H. Savietto.
Derived from Gaia DR2 sky in colour, CC BY-SA 3.0 IGO. The prepared target removes isolated point sources;
these metrics describe fidelity to that target, not calibrated sky radiance.</p>
<table><tr><th>Preset</th><th>BC7 + mips</th><th>PNG</th><th>PSNR dB</th><th>Band MAE /255</th></tr>"""
    html += "".join(rows) + '</table><h2>Prepared reference</h2>' + picture(folder / "target.png") + "".join(panels)
    (folder / "index.html").write_text(html, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / ".tools/asset-sources/esa_gaia_dr2_allsky_brightness_colour_cartesian_2k.jpg")
    parser.add_argument("--output", type=Path, default=ROOT / ".scratch/galaxy-layers")
    parser.add_argument("--presets", nargs="+", choices=PRESETS, default=list(PRESETS))
    parser.add_argument("--install", choices=PRESETS, help="Copy a fitted preset into assets/materials for the demo")
    parser.add_argument("--steps", type=int, default=400)
    parser.add_argument("--width", type=int, default=1800)
    parser.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    parser.add_argument("--encoder", type=Path, default=ROOT / "build/release" / ("bc7_compress.exe" if os.name == "nt" else "bc7_compress"))
    args = parser.parse_args()
    if args.steps < 1 or args.width < 64 or args.width % 4:
        parser.error("steps must be positive; width must be >=64 and divisible by 4")
    if not args.encoder.is_file():
        parser.error("Build texture_tools first or supply --encoder")
    if args.install and args.install not in args.presets:
        parser.error("--install must name one of --presets")
    torch.manual_seed(7)
    torch.set_num_threads(8)
    args.output.mkdir(parents=True, exist_ok=True)
    target = prepare(args.source, args.width)
    save_png(args.output / "target.png", target)
    results = []
    baselines = []
    for width in (512, 768, 1024):
        path = args.output / f"single-{width}.png"
        down = np.asarray(Image.open(args.output / "target.png").resize((width, width // 2), Image.Resampling.LANCZOS), dtype=np.float32) / 255
        decoded, size = encode(path, down, args.encoder)
        pred = array(sample(tensor(decoded, args.device), args.width, args.width // 2))
        baselines.append({"width": width, "bc7_bytes_with_mips": size, **metrics(pred, target)})
    for name in args.presets:
        start = time.monotonic()
        folder = args.output / name
        folder.mkdir(exist_ok=True)
        print(f"Fitting {name}: {PRESETS[name]}", flush=True)
        layers, history = fit(target, PRESETS[name], args.steps, args.device)
        decoded, total, png_bytes = [], 0, 0
        for label, layer in zip(("low", "clouds", "filaments"), layers):
            path = folder / f"galaxy_{label}.png"
            value, size = encode(path, layer, args.encoder)
            decoded.append(tensor(value[..., :1] if label == "filaments" else value, args.device))
            total += size
            png_bytes += path.stat().st_size
        with torch.no_grad():
            pred = array(compose([tensor(v, args.device) for v in layers], args.width, args.width // 2))
            compressed = array(compose(decoded, args.width, args.width // 2))
            low_only = array(compose(decoded, args.width, args.width // 2, 0, 0))
            no_dark = array(compose(decoded, args.width, args.width // 2, 1, 0))
        for label, value in (("recomposed", compressed), ("low-only", low_only), ("without-filaments", no_dark)):
            save_png(folder / f"{label}.png", value)
        result = {"preset": name, "widths": PRESETS[name], "bc7_bytes_with_mips": total, "png_bytes": png_bytes,
                  "uncompressed": metrics(pred, target), "bc7": metrics(compressed, target), "history": history,
                  "seconds": time.monotonic() - start}
        results.append(result)
        print(json.dumps(result, indent=2), flush=True)
        report = {"source": args.source.name, "source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
                  "steps": args.steps, "width": args.width, "single_texture_baselines": baselines, "band_half_uv": BAND_HALF, "results": results}
        (args.output / "report.json").write_text(json.dumps(report, indent=2))
        write_report(args.output, report)

    if args.install:
        destination = ROOT / "assets/materials"
        for path in (args.output / args.install).glob("galaxy_*"):
            if path.suffix in (".png", ".otex"):
                shutil.copy2(path, destination / path.name)
        installed = {"version": 1, "source": report["source"], "source_sha256": report["source_sha256"],
                     "attribution": "ESA/Gaia/DPAC; A. Moitinho, A. F. Silva, M. Barros, C. Barata, H. Savietto",
                     "license": "CC BY-SA 3.0 IGO", "source_url": "https://www.cosmos.esa.int/web/gaia/gaiadr2_gaiaskyincolour",
                     "band_half_uv": BAND_HALF, "fit": next(r for r in results if r["preset"] == args.install)}
        (destination / "galaxy_layers.json").write_text(json.dumps(installed, indent=2), encoding="utf-8")
        print(f"Installed {args.install} galaxy layers; build the demo to synchronize assets.")


if __name__ == "__main__":
    main()
