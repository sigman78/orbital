"""Build optional BC7/ASTC material caches on demand; defaults < per-file settings < command-line overrides."""
import argparse
import json
import os
import struct
import tempfile
from pathlib import Path
import subprocess


def fnv1a(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return value


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, default=root / "assets/materials")
    parser.add_argument("--config", type=Path, default=root / "assets/materials/compression.json")
    parser.add_argument("--prepare", type=Path, default=root / "build/release" / ("texture_prepare.exe" if os.name == "nt" else "texture_prepare"))
    parser.add_argument("--encoder", type=Path, default=root / "build/release/third_party/astcenc/Source" / ("astcenc-native.exe" if os.name == "nt" else "astcenc-native"))
    parser.add_argument("--bc7-encoder", type=Path, default=root / "build/release" / ("bc7_compress.exe" if os.name == "nt" else "bc7_compress"))
    parser.add_argument("--block", choices=("4x4", "6x6", "8x8", "12x12"))
    parser.add_argument("--only", nargs="+", help="Catalog filenames, e.g. earth_albedo.png earth_normal.png")
    parser.add_argument("--format", choices=("bc7", "astc", "rgba8", "source"))
    parser.add_argument("--quality", choices=("fastest", "fast", "medium", "thorough", "verythorough", "exhaustive"))
    parser.add_argument("--threads", type=int, choices=range(1, 17), default=0)
    parser.add_argument("--weights", nargs=4, type=int, metavar=("R", "G", "B", "A"))
    args = parser.parse_args()
    if not args.prepare.is_file():
        parser.error("Build the texture_tools target first (see docs/TEXTURE_COMPRESSION.md).")
    catalog = subprocess.check_output([str(args.prepare), "--list"], text=True).splitlines()
    try:
        config = json.loads(args.config.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        parser.error(str(error))
    if not isinstance(config, dict) or not isinstance(config.get("defaults", {}), dict) or not isinstance(config.get("textures", {}), dict):
        parser.error("Config defaults and textures must be objects")
    if set(config) - {"defaults", "textures"}:
        parser.error("Unknown compression config section")
    overrides = config.get("textures", {})
    unknown = (set(overrides) | set(args.only or [])) - set(catalog)
    if unknown:
        parser.error(f"Unknown material filenames: {sorted(unknown)}")
    jobs = []
    for name in catalog:
        if args.only and name not in args.only:
            continue
        settings = {"format": "bc7", "quality": "medium", "block": "6x6", "weights": [1, 1, 1, 1]}
        settings.update(config.get("defaults", {}))
        if not isinstance(overrides.get(name, {}), dict):
            parser.error(f"Settings for {name} must be an object")
        settings.update(overrides.get(name, {}))
        for key in ("format", "quality", "block", "weights"):
            value = getattr(args, key)
            if value is not None:
                settings[key] = value
        if set(settings) != {"format", "quality", "block", "weights"}:
            parser.error(f"Unknown settings for {name}")
        if settings["format"] not in ("bc7", "astc", "rgba8", "source") or settings["quality"] not in ("fastest", "fast", "medium", "thorough", "verythorough", "exhaustive") or settings["block"] not in ("4x4", "6x6", "8x8", "12x12"):
            parser.error(f"Invalid format or quality for {name}")
        weights = settings["weights"]
        if not isinstance(weights, list) or len(weights) != 4 or any(type(w) is not int or not 1 <= w <= 128 for w in weights):
            parser.error(f"Weights for {name} must be four integers in 1..128")
        if settings["format"] != "source" and args.only and not (args.directory / name).is_file():
            parser.error(f"Missing source: {args.directory / name}")
        if settings["format"] == "astc" and not args.encoder.is_file():
            parser.error("Build the optional astcenc tool or supply --encoder")
        if settings["format"] == "bc7" and not args.bc7_encoder.is_file():
            parser.error("Build the optional bc7_compress tool or supply --bc7-encoder")
        jobs.append((name, settings))
    # Validate the complete selection before replacing any cache.
    for name, settings in jobs:
        source = args.directory / name
        output = source.with_suffix("." + settings["format"] + ".otex")
        if settings["format"] == "source":
            for suffix in (".otex", ".bc7.otex", ".astc.otex", ".rgba8.otex"):
                source.with_suffix(suffix).unlink(missing_ok=True)
            print(f"{name}: source only", flush=True)
            continue
        if not source.is_file():
            if args.only:
                parser.error(f"Missing source: {source}")
            print(f"{name}: source missing, skipped", flush=True)
            continue
        if settings["format"] == "astc" and not args.encoder.is_file():
            parser.error("Build the optional astcenc tool or supply --encoder")
        with tempfile.TemporaryDirectory(prefix="orbital-texture-") as folder:
            temporary = Path(folder)
            raw = settings["format"] != "astc"
            subprocess.run([str(args.prepare), str(source), folder] + (["--raw"] if raw else []), check=True)
            meta = json.loads((temporary / "metadata.json").read_text())
            payload = bytearray()
            width, height = meta["width"], meta["height"]
            block = int(settings["block"].split("x")[0]) if not raw else 4
            for level in range(meta["levels"]):
                if settings["format"] == "bc7":
                    encoded = temporary / f"{level}.bc7"
                    subprocess.run([str(args.bc7_encoder), str(temporary / f"{level}.rgba"), str(encoded),
                                    str(width), str(height), settings["quality"], str(args.threads),
                                    *map(str, settings["weights"])], check=True)
                    data = encoded.read_bytes()
                    if len(data) != ((width + 3) // 4) * ((height + 3) // 4) * 16:
                        raise ValueError("Truncated BC7 output")
                elif raw:
                    data = (temporary / f"{level}.rgba").read_bytes()
                    if len(data) != width * height * 4:
                        raise ValueError("Invalid prepared RGBA mip")
                else:
                    encoded = temporary / f"{level}.astc"
                    command = [str(args.encoder), "-cl", str(temporary / f"{level}.ktx"), str(encoded),
                               settings["block"], "-" + settings["quality"], "-silent", "-cw", *map(str, settings["weights"])]
                    if args.threads:
                        command += ["-j", str(args.threads)]
                    subprocess.run(command, check=True)
                    astc = encoded.read_bytes()
                    if (len(astc) < 16 or astc[:4] != bytes.fromhex("13aba15c") or
                        astc[4:7] != bytes((block, block, 1)) or
                        int.from_bytes(astc[7:10], "little") != width or
                        int.from_bytes(astc[10:13], "little") != height or
                        int.from_bytes(astc[13:16], "little") != 1):
                        raise ValueError("Unexpected astcenc output header")
                    data = astc[16:]
                    if len(data) != ((width + block - 1) // block) * ((height + block - 1) // block) * 16:
                        raise ValueError("Truncated astcenc output")
                payload.extend(data)
                width, height = max(1, width // 2), max(1, height // 2)
            source_hash = fnv1a(source.read_bytes())
            if source_hash != meta["source_hash"]:
                raise ValueError("Source changed during compression")
            checksum = fnv1a(payload)
            header = struct.pack("<16I", 0x5845544F, 1, meta["width"], meta["height"], meta["levels"],
                                 block, block, {"rgba8": 0, "astc": 1, "bc7": 2}[settings["format"]], meta["flags"], source_hash & 0xffffffff,
                                 source_hash >> 32, checksum & 0xffffffff, checksum >> 32, 0, 0, 0)
            with tempfile.NamedTemporaryFile(dir=output.parent, prefix=output.name + ".", suffix=".tmp", delete=False) as cache:
                staged = Path(cache.name)
                cache.write(header)
                cache.write(payload)
            try:
                os.replace(staged, output)
            finally:
                staged.unlink(missing_ok=True)
            print(f"{name}: {len(header) + len(payload)} bytes, {meta['levels']} mips, {settings['format']} {block}x{block}", flush=True)



if __name__ == "__main__":
    main()
