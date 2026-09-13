# Optional texture compression

The vendored Arm **astcenc** CLI and **bc7enc** compressor run only on demand.
Neither encoder is linked into the demo. Normal builds require neither encoder nor
Python, and never compress assets. BC7 is the desktop default, including GTX 1080 Ti;
ASTC remains available for platforms that support it.

## Build and run

Windows:

```powershell
./tools/build.ps1 -Target texture_tools
python tools/compress-textures.py
./tools/build.ps1
```

Linux:

```sh
cmake --preset linux-release -DORBITAL_BUILD_TEXTURE_TOOLS=ON
cmake --build --preset linux-release --target texture_tools
python3 tools/compress-textures.py --prepare build/linux-release/texture_prepare --bc7-encoder build/linux-release/bc7_compress --encoder build/linux-release/third_party/astcenc/Source/astcenc-native
cmake --build --preset linux-release
```

The final build copies the generated caches beside the demo's source materials.
`--directory` can instead target an existing build's `assets/materials` directory.
Use `--encoder` to select a separately built astcenc executable and `--bc7-encoder`
to select the optional BC7 wrapper. The optional target
also works with `ORBITAL_BUILD_DEMO=OFF`, without Vulkan or a GPU.

## Defaults and overrides

`assets/materials/compression.json` supplies defaults and exact-filename overrides.
The batch covers the shared material catalog; missing optional source maps are skipped.
Command-line settings override the file's settings:

```powershell
python tools/compress-textures.py --only earth_albedo.png --format astc --block 8x8 --quality thorough
python tools/compress-textures.py --only rock_normal.png --block 4x4 --weights 2 2 2 4
python tools/compress-textures.py --only gas_relief.png --format rgba8
python tools/compress-textures.py --only earth_clouds.png --format source
```

- Default: BC7 4x4, medium effort. ASTC supports 4x4, 6x6, 8x8 and 12x12
  (6x6 by default). `block` applies only to ASTC; BC7 always uses 4x4.
- Normal maps: 4x4, thorough effort. All four channels are preserved, including packed height.
- Flow/detail/relief maps: lossless RGBA8 cache by default.
- `--format source` removes all format variants and the legacy cache for the selected texture so the demo uses its PNG again.
- `--threads 1..16` limits encoder workers; omitted uses automatic selection
  (capped at 16 for BC7).
- `--quality` accepts fastest, fast, medium, thorough, verythorough and exhaustive.
  ASTC uses upstream presets; BC7 uses increasing partition search and refinement
  effort (uber levels 0, 0, 1, 2, 3, 4). The names are effort tiers, not equivalent quality across encoders.
- `--weights R G B A` sets channel error weights (integers 1..128).

Preparation reuses the runtime's material descriptions, sRGB-to-linear conversion,
cloud-mask conversion and normal-aware mip filtering. Each prepared mip is passed to
astcenc as uncompressed KTX or to the BC7 wrapper as raw RGBA, avoiding an extra
PNG encode/decode. BC7 preserves all four channels and clamps partial edge blocks.
The `-cl` profile encodes those already-linear RGBA bytes. Normal-map swizzles are
not enabled: the current shaders expect RGB normals and may use alpha for height.

## Loading and distribution

Generated `name.bc7.otex`, `name.astc.otex` and `name.rgba8.otex` files contain the
complete mip chain and can coexist. Supply whichever variants suit the deployment;
none are required. The demo tries supported BC7, supported ASTC, then RGBA8 caches,
followed by legacy `name.otex` files. Unsupported formats are skipped silently.
Stale, corrupt or mislabeled caches are skipped in favor of the next candidate,
then the source PNG. Material settings, source fingerprint (when the PNG exists),
and payload checksum must match.

A valid cache can be used without its PNG, but keep PNGs when distributing to GPUs
that might not support the supplied formats. No runtime compression or transcoding
is performed. To create an ASTC distribution while retaining per-texture lossless
exceptions, copy the config, change its default format to `astc`, and pass `--config`.
An explicit `--format astc` overrides even those per-texture exceptions. Generating
one format leaves the other variants intact; when both BC7 and ASTC are supported,
BC7 wins regardless of which was generated most recently.

Caches are ignored by Git, copied with assets during builds, and included by asset
installation. Removing a cache in the source tree does not remove a previously copied
file from an existing build: run the source-only command with `--directory` pointing
at that build, or clean its asset directory. Source changes invalidate old caches by
content, not timestamps. Changing compression settings requires rerunning the command.

The cache is an internal container, not a DDS or KTX file: 64 little-endian header
bytes followed by tightly packed mip payloads. Header words are magic `OTEX`, version,
width, height, mip count, block width/height, format (0 RGBA8, 1 ASTC, 2 BC7), material flags,
source FNV-1a-64 low/high, payload FNV-1a-64 low/high, and three reserved zeros. Each
dimension halves to at least one; compressed block counts round up. Version changes invalidate
incompatible preparation rules. Fingerprints detect stale/corrupt local caches and
are not security signatures.
