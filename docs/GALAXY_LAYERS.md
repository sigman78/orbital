# Gaia texture-layer experiment

An optional alternative to the Gaussian-splat background. Select **Sky → Galaxy
background → Texture layers**, or launch with `--galaxy 1`. Splats remain the default.
The three gains let you inspect the global structure, add clouds, then darken with
filaments. Texture mode samples at frame resolution and skips the splat pass and
its procedural dust modulation.

```powershell
./build/release/orbital.exe --galaxy 1 --galaxy-view 0 --time 0 --exposure 5
```

`--galaxy-view -180..180` points a camera outside the solar system along the galactic
plane at that longitude. Zero is the centre; 180 tests the longitude seam. Omit it
for normal navigation.

## Original texture comparison

Select **Sky → Galaxy background → Original full resolution**, or `--galaxy 2`.
This samples `assets/materials/galaxy_original.png`: a lossless RGB conversion of
that same original 1800×900 JPEG, with no resizing, source cleanup, fitting or BC7
compression. It retains the original point sources and border pixels. The renderer
uses ordinary mip filtering, the same galactic mapping, squared display values,
brightness and contrast as the layers. Layer gains and procedural dust do not apply.
Switch modes in place to preserve the camera and all shared settings.
The source credit and license below also apply to this comparison image.

## Research closeout — 2026-09-13

Research is complete for this comparison; the production approach is **not yet
chosen**. Splats remain the default, with layers and the original available for
review. The installed layer budget is compact, not a claim of visual equivalence
to the full-resolution source.

Matched GTX 1080 Ti captures at 960×540, time 0, exposure 5, after 60 frames show
that the compact layers preserve the broad band, colour and major dark lanes at
the galactic centre. The original retains visibly sharper lane edges, finer cloud
texture and additional small bright detail; the layers look smoother. At longitude
180° the broad faint structure remains similar, with finer differences harder to
judge at this exposure. These are visual observations, not a user preference or
a new numerical quality score. The original's unrepaired border remains part of
that reference; it is not evidence of a longitude-mapping fault.

The raw-original comparison includes preparation differences: the layer fit uses
the cleaned `target.png`, whereas the original retains point sources and border
pixels. Both runtime views also draw the same catalogue/procedural stars and
post-processing. Use the prepared atlas report to isolate fitting/compression
error. The 216.4 versus 384.1 KiB result below concerns comparable **band error**
against the prepared target, not full-resolution fidelity or total frame speed.

### What is implemented

- Adam fitting, five budget presets, single-map BC7 baselines, quantized fitting,
  composition previews, a report, and checks for wrapping, convergence and encoding.
- Three installed layer PNGs and provenance JSON; optional BC7 caches; raw-UNORM
  loading and complete mip chains. A missing layer set falls back to splats.
- Shared galactic coordinates, direct full-frame layer sampling, independent
  structure/cloud/filament gains, and bypass of the splat pass and procedural dust
  for texture modes.
- An uncompressed 1800×900 original PNG that matches the decoded source exactly,
  loaded independently of the layer caches. A missing original falls back to splats.
- A three-way Sky selector, `--galaxy 0|1|2`, and `--galaxy-view` for repeatable
  views. All alternatives are loaded at startup, so current memory/startup costs
  include research assets even when another mode is selected.
- Four added texture slots (50 total), matching conventional-backend descriptors
  and compatibility patch, and shader-helper checks for descriptor-count drift.

### Evidence and reproduction

Local, ignored artifacts live in `.scratch/galaxy-layers-final/`: `index.html`,
`report.json`, `target.png`, five candidate folders, single-map baselines, and
`original-compare-mode{1,2}-lon{0,180}.png` with logs and timings. Earlier sweeps
are in `.scratch/galaxy-layers/` and `.scratch/galaxy-layers-round2/`; splat research
is under `.scratch/milky-way/`. These artifacts are not included in the commit;
the table and observations here preserve the conclusions.

Repeat a capture with either mode, and use longitude 180 for the seam view:

```powershell
./build/release/orbital.exe --galaxy 2 --galaxy-view 0 --frames 60 --time 0 --exposure 5 --width 960 --height 540 --vsync 0 --no-hud --capture original.png
```

### Cleanup after choosing an approach

| Choice | Keep | Remove or retire |
| --- | --- | --- |
| Splats | Splat asset, baker, pass and dust controls | Layer PNGs/caches/JSON, original PNG, both texture loaders and shader paths, four texture slots, layer gains, and layer-only fitting/check tools |
| Texture layers | Chosen layer set, baker, provenance, loader, composition and useful gains | Original PNG/loader/sample function/slot; splat asset, loader, buffer, render target, pipeline/pass, resize work and splat/dust controls if no longer needed as fallback |
| Single texture | A deliberately chosen source preparation, resolution/encoding, loader and sampling path | Three-layer assets/caches/JSON, gains, composition and layer-only tools; splat resources and controls if no longer needed as fallback. The raw original is currently a reference, not an optimized shipping asset |

For every choice, remove the comparison selector and unused mode/frame fields,
then update CLI help, defaults, resource slots, descriptor count, compatibility
patch and ABI documentation together. Decide explicitly whether `--galaxy-view`
and a splat fallback remain useful. Keep shared galactic helpers where still used,
the independent star catalogue, general material/cache support, attribution and
this research record. Archive or delete ignored captures, rejected fits and caches
only after preserving any desired evidence. No alternative is removed by this
research closeout.

## Decomposition and fitting

The source is `.tools/asset-sources/esa_gaia_dr2_allsky_brightness_colour_cartesian_2k.jpg`,
actually **1800×900**, SHA-256
`58536692c0f4261becbd865d9438a9a2044d6f60715fb918b5e3d4596c386224`.
It is a display-stretched integrated-starlight map, not calibrated radiance. A 3×3
median and mild blur remove isolated point sources; catalogue stars remain separate.
Border pixels are repaired before filtering, and longitude wraps throughout.

The installed `compact` fit contains:

| Layer | Dimensions | Meaning |
| --- | --- | --- |
| `galaxy_low.png` | 64×32 RGB | Smooth global colour and broad halo |
| `galaxy_clouds.png` | 256×64 RGB | Additive cloud structure within latitude ±45° |
| `galaxy_filaments.png` | 768×192 grayscale | Higher-frequency darkening within the same strip |

Let `b` fade smoothly from one at ±36° to zero at ±45°. The linear composition is
`(low² + b × clouds²) × (1 − b × filaments)`. Colour textures hold square-root
radiance for precision in faint areas; they must load as raw UNORM, without sRGB
conversion. The display-space preview takes the square root of this composition.
The runtime applies the existing brightness and contrast settings afterward.

Adam jointly adjusts bounded texels, with a decaying learning rate, spherical-area
weighted colour and log losses, smoothness penalties on the two colour layers,
and a sparsity penalty on extinction. The final quarter uses straight-through
8-bit quantization. This is a constrained factorization; it does not identify
physical dust depth uniquely. BC7 itself is not differentiable here: every fitted
candidate is actually compressed and independently decoded with Pillow before
quality is measured and a resolution budget is selected.

## Reproduce and iterate

The offline fitter needs Python with **numpy, scipy, Pillow and torch**, plus the
optional `texture_tools` build. CUDA is used when available; `--device cpu` works
as well. None of these Python dependencies are needed by the demo.

```powershell
./tools/build.ps1 -Target texture_tools
python tools/bake-galaxy-layers.py --steps 500 --output .scratch/galaxy-layers-final --install compact
./tools/build.ps1
```

The default sweep compares five budgets plus three ordinary downsampled BC7 maps.
`--presets compact` fits just that budget. `--source`, `--output`, `--width`,
`--encoder`, `--steps` and `--device` override the input, output and fitting setup.
Only `--install` copies the selected set to `assets/materials`; the experiment
directory retains every candidate, composition previews, loss histories,
`report.json`, and a standalone `index.html` with embedded images.

The existing compression command also recognizes these three PNGs, so caches can
be regenerated without refitting. BC7 caches are optional and ignored by Git;
PNGs and `galaxy_layers.json` record the installed fit. A missing/unsupported cache
falls back to PNG. A complete cache-only set works on a supporting GPU. Missing or
invalid layers fall back to the splat/procedural background as a set. The JSON is
provenance, not runtime configuration; the strip mapping is fixed by the shader.

## Measured tradeoffs

500 Adam steps per candidate, full 1800×900 prepared reference, actual BC7 decode.
Sizes include complete mip chains and cache headers. PSNR is spherical-area
weighted; band MAE is measured within ±30° in 8-bit display levels.

| Fit | BC7 KiB | PNG KiB | PSNR dB | Band MAE |
| --- | ---: | ---: | ---: | ---: |
| Tiny | 91.7 | 46.6 | 41.28 | 1.60 |
| **Compact (installed)** | **216.4** | **116.3** | **44.24** | **1.12** |
| Small | 365.7 | 210.4 | 45.32 | 0.93 |
| Balanced | 856.4 | 479.3 | 47.07 | 0.66 |
| Fine | 1461.7 | 729.2 | 48.83 | 0.59 |
| Single 512×256 | 170.8 | — | 41.98 | 1.55 |
| Single 768×384 | 384.1 | — | 45.25 | 1.11 |
| Single 1024×512 | 682.8 | — | 47.80 | 0.85 |

The compact fit uses 44% fewer BC7 bytes than the single 768×384 map at nearly
equal band error, while the single map better preserves the outer sky. Low-only
structure intentionally softens small off-plane features, including the Magellanic
Clouds. Tiny visibly softens dust lanes; larger budgets retain more fine structure.
These are fidelity/size comparisons, not a claim that layers always beat a single map.

The first sweep used fixed learning rates and RGB PNGs for the scalar mask. The
second introduced the intermediate budget; the final sweep decayed the learning
rate and stored the mask as grayscale, cutting compact PNG storage from 167 to
116 KiB while slightly improving compressed error. BC7 storage still uses a full
block for the scalar mask; a single-channel GPU format is a possible later experiment.

## Validation

`python tools/check-galaxy-layers.py` checks composition constraints, wrapped
bilinear sampling, Adam convergence, BC7 round-trip and source/cache fingerprints.
It also compares Python mip bytes with the C++ material preparer at odd dimensions.
`python tools/check-shaders.py` checks independent shader compilation and
the conventional backend's descriptor count against the shared slot definitions.

The release build and all nine CTest suites pass, and all 27 shader helpers compile
independently. GTX 1080 Ti captures cover splats and layers at longitudes 0°, 90°
and 180°, plus the original-versus-layers comparison at 0° and 180°.
Cache-only rendering matched the normal cached
render exactly; missing layers matched the splat fallback exactly. PNG versus BC7
at the centre differed by 0.133 display levels on average at the tested exposure.
Runtime screen captures include stars and post-processing; the atlas report is
the cleaner place to inspect fitting error.

Source credit: **ESA/Gaia/DPAC; A. Moitinho, A. F. Silva, M. Barros, C. Barata,
H. Savietto**, [Gaia DR2 sky in colour](https://www.cosmos.esa.int/web/gaia/gaiadr2_gaiaskyincolour).
The derived textures retain **CC BY-SA 3.0 IGO**, as recorded for this source in
the existing asset manifest. Keep that credit and license with redistributed layers.
