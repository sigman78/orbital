# Tile compression: budgets, measurements and proposals

Written 2026-10-26 from a discussion on `evo/planet-forge`, for the day baked tiles need to be
small in memory or on disk. Nothing here is built; phase 4 of docs/PLANET_FORGE.md picks from it.
Numbers come from the seed-20260911 showcase body baked at levels 0..5 with `planet-forge` (raw
planes, detail 1) and from `tools/tile-compression-study.py`, which reads a shard pair straight from
the store and reports the ratios below for any level.

## Terms

- **Shell**: the height band the terrain may occupy around the reference sphere, `BodyShape::heights`
  (`shell_height_min` to `shell_height_max`, -0.05 to +0.05 radii for the minor planet). R16 heights
  are quantized over it, so one **step** is 0.1 / 65535 = 1.53e-6 radii.
- **Level error**: `LevelTable::error[level]`, the measured error of a level's grid against the true
  surface (the CLOD's split threshold). **Relief**: how far a tile's subtree reaches outside its own
  heights. Both in radii; the table below converts them to steps, which is the currency any lossy
  height format has to fit.
- **Tile today**: height R16 33x33 (2,178 B), albedo RGBA8 65x65 (16,900 B), slope RG16 65x65
  (16,900 B); 35,980 B with four-byte plane alignment. Heights are 6 percent of a tile.

## Error budget per level

| Level | Error, radii | Error, steps | Relief, radii | Relief, steps |
| --- | --- | --- | --- | --- |
| 0 | 3.05e-2 | 19,971 | 2.9e-2 | 18,743 |
| 1 | 1.93e-2 | 12,638 | 1.4e-2 | 8,913 |
| 2 | 5.91e-3 | 3,875 | 6.1e-3 | 3,998 |
| 3 | 2.45e-3 | 1,606 | 3.6e-3 | 2,359 |
| 4 | 1.00e-3 | 656 | 1.4e-3 | 917 |
| 5 | 4.57e-4 | 300 | 5.6e-4 | 367 |
| 6 | 1.73e-4 | 113 | 2.6e-4 | 170 |
| 7 | 8.54e-5 | 56 | 1.5e-4 | 98 |
| 8 | 4.49e-5 | 29 | 5.5e-5 | 36 |
| 9 | 2.28e-5 | 15 | 1.6e-5 | 10 |
| 10 | 1.25e-5 | 8 | 4.0e-6 | 3 |
| 11 | 4.57e-6 | 3 | 2.0e-6 | 1 |

R16 spends a third of the finest level's budget on quantization alone. Heights feed the vertex
shader, so any further loss is geometry: terraces along slopes, and parent and child edges that no
longer meet during the morph. Shading does not see height loss, since normals come from the slope
plane.

## Lossless compression, measured

Face 3 of the bake, zlib level 9 standing in for zstd (zstd lands within a few percent of these
ratios at similar levels and decodes several times faster). Predictors: left delta; the JPEG-LS MED
predictor on the 33x33 grid; the residual against the parent tile's bilinear upsample (the parent
is in the store, and resident in the arrays when the child is drawn).

| Plane, lossless | Raw B/tile | Level 3 | Level 4 | Level 5 |
| --- | --- | --- | --- | --- |
| heights raw R16 | 2,178 | 1,941 (1.12) | 1,958 (1.11) | 1,889 (1.15) |
| heights, left delta | 2,178 | 1,751 (1.24) | 1,676 (1.30) | 1,498 (1.45) |
| heights, MED predictor | 2,178 | 1,698 (1.28) | 1,529 (1.42) | 1,238 (1.76) |
| heights, parent residual | 2,178 | 1,344 (1.62) | 1,198 (1.82) | 911 (2.39) |
| albedo RGBA8 raw | 16,900 | 1,617 (10.5) | 1,430 (11.8) | 1,506 (11.2) |
| slope RG16 raw | 16,900 | 14,945 (1.13) | 15,019 (1.13) | 15,133 (1.12) |

Parent residuals: level 3 at most 2,387 steps (99.9 percent under 1,650); level 4 at most 1,230
(596); level 5 at most 514 (230). They shrink with the level like the relief does, so the predictor
gains most where there are most tiles.

What the table says:

- Raw heights barely compress; prediction is what makes them compress. The parent residual is the
  best predictor and costs a second tile read on decode; MED needs only the tile.
- Albedo is flat enough that lossless zstd (1.5 KB) beats BC7 (4.6 KB at 68x68) on disk. In memory
  BC7 still wins, since the array must be a GPU format.
- Slope is the micro-relief noise at detail 1 and does not compress losslessly at all. It is 47
  percent of the tile; BC5 is its only lever, and BC5 is also where the normal-error risk sits
  (IDEAS.md: 8-bit endpoints bring a rim block back near the old RGBA8 normal error, 0.18 degrees
  rms).

## Lossy height formats in the arrays, assessed

The GPU has no 16-bit single-channel block format. The candidates, against the budget above:

| Scheme | Worst error | Verdict |
| --- | --- | --- |
| BC4 on absolute heights | endpoints 8-bit: 1/255 of the shell = 257 steps; inside a block 1/7 of the block's range | fails from level 5 down |
| BC6H (half float) on absolute heights | 3 or 4 index bits: 8 to 16 levels between a block's endpoints; a rim block spanning a tenth of the shell terraces at about 400 steps; half precision itself is 20 steps near the shell's top | fails at every level |
| Hi and lo bytes in two 8-bit channels (BC5 or two BC4) | the low byte wraps every 256 steps and is noise to the encoder; errors up to a full low byte, 256 steps | fails |
| Hierarchical: parent tile plus a BC4 residual with a per-tile scale and bias | residual range per tile is about the relief: 1,000 steps at level 5, 1 step at level 11; BC4 error about range / 7 inside a block: about 40 steps at level 5, under 1 at level 11 | fits the budget at every level |

The hierarchical scheme is sound and has one structural merit: the child becomes the parent as the
residual fades, so the morph is exact by construction, and the parent tile is already bound for the
material's coarse end. Its costs: a second height fetch per vertex, roots that need absolute
heights, a per-tile scale and bias in the patch record, and 4-aligned 36x36 residual planes. It
saves about 1.5 KB per tile, 3 MB of the arrays at 2,048 slots. Heights are 6 percent of a tile
today and 19 percent after BC7 and BC5, so this is the design to keep for the day height memory
matters, not something to build now.

## Colour and slope in the arrays (phase 4, unchanged)

| Plane | Today, 65 texels | Proposed | Bytes per tile |
| --- | --- | --- | --- |
| albedo | RGBA8, 16,900 | BC7 68x68 (BC1 at half) | 4,624 (2,312) |
| slope | RG16, 16,900 | BC5 68x68 | 4,624 |
| height | R16 33x33, 2,178 | unchanged | 2,178 |
| total | 35,980 | | 11,426 |

68 is the first 4-aligned side with a two-texel border; the border width is a measurement item for
bilinear plus one mip. At 2,048 slots the arrays drop from 70 MiB to about 22 MiB.

## Proposal for the store (disk and delivery)

Lossless, per shard, with plane-specific preprocessing done by the forge on write and undone on the
worker before upload, so the arrays stay exactly what the shaders read today:

| Plane | Store form | Expected B/tile (level 5) |
| --- | --- | --- |
| height | parent residual (MED for roots and when the parent read is unwanted), then zstd | 900 to 1,250 |
| albedo | raw RGBA8 + zstd while the arrays are RGBA8; BC7 with RDO + zstd once they are BC7 | 1,500 raw; BC7 about 3,000 with RDO |
| slope | BC5 (+ RDO) + zstd; raw gains nothing | 4,624, less with RDO |

- zstd per shard, not per tile, as Kiln's KTX2 does; keep it only where it saves enough.
- Morton or block reordering matters less than prediction for the raw planes, and the BCn planes
  are already 4x4 block ordered; for them the lever is RDO (`bc7enc_rdo` ships it), which trades a
  measured quality loss for LZ-friendly blocks. Measure RDO slopes against the normal-error table
  before adopting it.
- The index already carries each tile's height range; the residual's scale and bias would join it
  if the hierarchical array scheme is ever built, so the two designs share the store.

## Portability of baked tiles

A store of BC7 and BC5 blocks is a desktop Vulkan artefact. If baked tiles are shipped rather than
regenerated, either the blocks ship supercompressed per GPU family, or the store holds one
transcodable form: Basis Universal UASTC (near-lossless, transcodes to BC7, ASTC or ETC2 on the
worker) or ETC1S (smaller, visibly lossy). The decision precedes vendoring the first encoder. The
heights stay R16 either way: nothing in those formats holds them.

## Reproducing the numbers

```
planet-forge --out assets/planets/minor-planet --levels 0..5 --check
python -I tools/tile-compression-study.py 5     # one level, face 3, 512 tiles; needs numpy
```

The script uses zlib unless the `zstandard` module is installed, and prints the parent-residual
range alongside the ratios.
