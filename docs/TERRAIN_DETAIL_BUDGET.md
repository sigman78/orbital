# Minor planet detail budget

The [bump continuity investigation](TERRAIN_BUMP_CONTINUITY.md) fixes diagonal
micro-crater cuts without changing texture sizes or mesh spacing.

The subsequent [low-flight stability investigation](TERRAIN_LOD_STABILITY.md)
adds a streaming neighbour guard and quadrant-local balancing. The spacing study
below predates that change; its allocation figures still apply, but residency and
transient selection counts can differ.

## Implemented screen-spacing policy — 2026-10-25

This section supersedes the earlier recommendation to leave selection defaults unchanged.
The mesh remains 32x32 intervals, height 33x33, bump 65x65 and albedo 65x65:
**1:2:2 linear sampling density**. Ratios count intervals, not sample dimensions
(64/32 = 2, not 65/33). No new detail layers are introduced.

The renderer now defaults to `--terrain-mesh-pixels 8`. Use 16 for economy, 4 for
high density, and 0 to reproduce the former displacement-error-only selection.
These are nominal grid-spacing targets at the split threshold, not promised
triangle lengths everywhere. Leave `--lod-bias 0` for these comparisons: positive
bias refines both constraints and can greatly exceed the intended workload.

### Metric and selection

The old five-pixel budget bounds *estimated geometric displacement error*, not
edge length. Keep it to protect silhouette and relief. For level L, let
`f = height / (2*tan(fov_y/2))`, `s[L] = (2/32)*2^-L` radii and `P` be the
requested spacing. The child split range is now
`max(level_error[L]*f/5, s[L]*f/P) * 2^lod_bias`.
When P is zero the spacing term is disabled. These same ranges drive morphing,
retention and selection; no second incompatible refinement test is introduced.
CPU callers retain a zero default for existing error-only experiments; the
application explicitly supplies its default of eight.

The two-radii face width is a nominal density calibration, not an exact warped
sphere edge bound. Selection uses nearest cell distance, whereas projection uses
view depth. Warp, slopes, diagonals, morphing, grazing incidence, discrete LOD,
missing resident tiles, maximum level and cache/tree ceilings all affect actual
pixel sizes. Refinement often halves the spacing below its threshold. The target
is an upper preference, not a minimum: geometric error can demand smaller triangles.

For an approximately square projected grid with interval p, a triangle covers
p²/2 square pixels and its diagonal is sqrt(2)*p. Thus “8 px triangles” must
specify length versus area. At p=4/8/16, ideal full-screen 1920x1080 coverage takes
259,200 / 64,800 / 16,200 surface triangles, respectively. Those are area estimates,
not draw budgets: skirts, conservative culling, grazing foreshortening and
occluded triangles add work. The footprint tool now reports actual projected
triangle longest edges and areas from both triangles of sampled quads.

### Measurements and recommendation

Fresh deterministic footprint sweep, seed 1007, 900px high, 60° vertical FOV,
radial camera 1.08, renderer-equivalent 2048-slot cache, bias zero:

| View | Target | Grid edge p50 | Triangle longest p50 / p95 | Material major p50 | Submitted triangles | Resident tiles |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Nadir | Legacy | 9.12 | 11.34 / 21.57 | 4.78 | 41,472 | 52 |
| Nadir | 16 | 7.90 | 10.78 / 12.30 | 4.58 | 69,120 | 75 |
| Nadir | **8** | **3.99** | **5.42 / 6.25** | **2.31** | **227,520** | **186** |
| Nadir | 4 | 2.01 | 2.72 / 3.16 | 1.16 | 829,440 | 564 |
| Glancing | Legacy | 6.73 | 9.88 / 17.85 | 4.45 | 73,152 | 71 |
| Glancing | 16 | 6.13 | 8.78 / 15.03 | 3.94 | 92,736 | 87 |
| Glancing | **8** | **3.36** | **4.88 / 8.11** | **2.18** | **285,696** | **218** |
| Glancing | 4 | 1.74 | 2.53 / 4.21 | 1.13 | 1,010,880 | 680 |

Lengths are pixels; material major is pixels/texel. At target 8 the median
triangle areas are 8.19 and 2.37 square pixels. These are unweighted lattice
samples, not visible-pixel percentages. Positions use procedural heights rather
than exact quantized/morphed vertices, and no local ray occlusion is tested.
[Full CSV](data/terrain-spacing-2026-10-25.csv) includes rejection counts,
minor-axis footprints, anisotropy and legacy bias sweeps.

**Recommend 8 for the requested 4–8 px triangle scale, 16 for economy.** The 4
setting is an optional supersampling budget; these views do not justify it as
the default. At target 8, bump and albedo samples span about two pixels along
the major axis. This is a useful compromise for broad procedural color and
relief lighting, but not one-pixel material detail. Two samples per interval do
not mean two independently resolved features: Nyquist requires at least two
samples per cycle, and central-difference bump derivation and bilinear filtering
attenuate detail further. Perception depends on contrast, lighting, motion and
feature frequency, not density alone.

The generator already evaluates a bordered 67x67 relief field for central-difference
slopes, with a tile-scale Nyquist input to procedural micro-relief. Albedo uses
height and slope tint as well as regional crater data; it is not purely a smooth
macro map. Keep its current 2x density instead of reducing it to 1x. Height stays
matched to mesh vertices. Larger height maps alone cannot add geometry.

| Height / bump / albedo samples | Linear ratios | 2048-slot allocation |
| --- | --- | ---: |
| **33 / 65 / 65 (implemented)** | **1:2:2** | **70.27 MiB** |
| 33 / 65 / 129 (possible future albedo experiment) | 1:2:4 | 167.27 MiB |
| 33 / 129 / 129 | 1:4:4 | 264.27 MiB |

The latter two are calculations, not implemented presets. Increasing albedo to
4x costs another 97 MiB before mips and does not sharpen bump lighting. It is
not supported as a default by the present generator's color content. For future
high-contrast color assets, 4x albedo would put a 4px interval at roughly one
pixel/texel. Shared albedo/slope generation currently also makes independent
channel resolution a structural change, not a safe constant adjustment.

At target 8 the glancing view still has p90 anisotropy of 24:1. Current explicit
mip-zero sampling cannot resolve that by adding texels. Border-correct mipmaps,
anisotropic sampling and slope-variance handling are separate future filtering
work. No temporal-quality or universally optimal ratio claim follows from these
static measurements. The distinction between geometry density and finer normal
sampling is consistent with [NVIDIA's geometry clipmap implementation](https://developer.nvidia.com/gpugems/gpugems2/part-i-geometric-complexity/chapter-2-terrain-rendering-using-gpu-based-geometry).

### Render checks and reproduction

GTX 1080 Ti, bookmark 9, time 0, 1280x900, TAA off, 1200 frames; final 200-frame
windows all have zero pending tiles and zero evictions:

| Target | Drawn patches | Resident tiles | Submitted triangles | Body pass median ms |
| --- | ---: | ---: | ---: | ---: |
| 0 | 36 | 81 | 64,512 | 0.360 |
| 16 | 36 | 81 | 64,512 | 0.361 |
| 8 | 104 | 183 | 214,272 | 0.488 |
| 4 | 358 | 534 | 764,352 | 0.946 |

[Recorded windows](data/terrain-spacing-render-2026-10-25.json). These single-run
GPU timings are observations, not statistically isolated speed comparisons;
some runs overlapped CPU build/benchmark work. This camera differs from the
footprint sweep. Captures at 0 and 8 show visibly finer relief shading at 8;
that change includes finer material sampling, not solely better geometry.

The allocation stays 70.27 MiB; finer selection increases resident use and generation
work, not array size. The new default costs 3.32x the triangles and 2.26x the
resident tiles of legacy at this bookmark. Target 4 adds 3.57x triangles over 8.
Keep the fixed 2048 slots and bounded admission; budget pressure can prevent a
requested spacing from being achieved.

Build opt-in targets `terrain_footprint` and `terrain_budget`. Run the former
without arguments; run `terrain_budget 1007 1000` for CPU generation timing.
[Fresh generator report](data/terrain-generation-spacing-2026-10-25.csv) records
unchanged tile sizes/checksums and excludes renderer/upload scheduling. The isolated
GCC run measured 1.30–1.35 ms per height tile and 14.23–19.62 ms per material pair
across levels 3/5/7/9/11. Material generation remains the dominant streaming cost;
refinement increases the number of these jobs rather than making each job cheaper.

```text
orbital --headless --bookmark 9 --time 0 --tier-activate 100
  --terrain-mesh-pixels 8 --lod-bias 0 --taa 0 --frames 1200
  --width 1280 --height 900 --benchmark captures/terrain-spacing-8.csv
  --capture captures/terrain-spacing-8.png
```

The Release renderer builds and all 18 MSVC tests pass. Regression coverage
checks both constraints, morph-band ordering, resolution scaling, bias scaling,
CLI validation and actual boundary continuity under the new default spacing.
All 17 GCC tests passed, followed by passing reruns of the two amended suites.
A 300-frame pan with TAA and wireframe enabled completed; this is an integration
smoke check, not a temporal image-quality measurement.

## Earlier assessment

Assessment on 2026-10-25, after selection/cache fix `9a026b4`. Target: crisp close
flight with lower generation and GPU cost. Measurements use an i7-9700K and GTX
1080 Ti. This is a measured starting point, not a universal optimal ratio.

## What the current ratio buys

| Resource per patch | Resolution / work | GPU bytes |
| --- | --- | ---: |
| Displaced geometry | 32x32 quads: 2048 surface triangles, 256 outer-skirt triangles | Shared grid |
| Height | 33x33, R16_UNORM | 2178 |
| Albedo | 65x65, RGBA8_UNORM | 16900 |
| Tangent slopes | 65x65, RG16_UNORM | 16900 |
| Total tile | Two material intervals per mesh interval | 35978 |

Staging aligns the height plane to four bytes: 35980 bytes per ring slot.
The 2048-slot GPU cache is 70.27 MiB, excluding alignment/driver overhead; 94% is
material data. Residency does not change the allocated size. Ancestors occupy slots
for fallback and material morphing, so resident tiles exceed drawn patches.

Geometry carries silhouette, parallax and occlusion. Slope maps carry small relief
through lighting; they do not displace the silhouette. Height values are samples
for mesh vertices, not a separately useful high-resolution displacement texture.
Reducing the mesh without recalibrating `level_error` would invalidate the present
screen-error budget and can affect morph/seam behavior.

Each material tile evaluates a 67x67 bordered height/detail field and 4225 albedo
samples. With the separate height plane this is 5578 height calls, 4489 micro-detail
calls and 4225 albedo calls per generated patch. Those costs are paid when streaming,
not every frame once resident.

## Quality versus LOD

Fixed bookmark 9, time 0, 1280x900, TAA off, 1200 frames per run. Each final
200-frame window had zero pending tiles. The approximate geometric error targets
below follow the existing calibrated distance ranges; they are not certified
per-pixel bounds. This capture table predates the subsequent glancing-view quadrant
cull documented in [TERRAIN_LOD_STABILITY.md](TERRAIN_LOD_STABILITY.md).

| LOD bias | Approximate error target | Drawn patches | Resident tiles | Required surface + skirt triangles |
| --- | ---: | ---: | ---: | ---: |
| 0 | 5 px | 41 | 81 | 89856 |
| +1 | 2.5 px | 71 | 134 | 158400 |
| +2 | 1.25 px | 154 | 257 | 345024 |

Bias +2 is visibly sharper in this view, but costs 3.84 times the selected triangles
and 3.17 times the resident tiles of bias 0. Bias +1 is a more moderate close-flight
choice, not a free quality increase. The present control refines geometry and
material together. The extra crispness cannot be attributed to geometry alone.

## Implemented adjustments

**Regional albedo generation.** Height generation already restricted crater tests to
the tile's spherical region, but albedo tested all 420 craters at every texel. Albedo
now uses the same region, preserving crater order and the existing support cutoff.
The full-sphere API remains available for the global map bake.

Sequential GCC Release CPU runs, seed 1007, four fixed cells per level and at least
1000 ms per row (milliseconds per material tile):

| Level | Original | Regional albedo | Reduction |
| --- | ---: | ---: | ---: |
| 3 | 23.38 | 14.58 | 37.6% |
| 5 | 32.82 | 20.29 | 38.2% |
| 7 | 28.12 | 19.42 | 31.0% |
| 9 | 28.39 | 19.53 | 31.2% |
| 11 | 29.81 | 19.42 | 34.9% |

All height/albedo/slope output checksums match. Heights remain approximately
1.3 ms/tile. This improves streaming throughput, not the steady-state GPU cost.
Raw reports: [before](data/terrain-generation-before-2026-10-25.csv) and
[after](data/terrain-generation-after-2026-10-25.csv). Timing varies with clock/load;
the percentages describe these runs. The baseline executable uses the committed
pre-optimization tile generator and the same compiler/libraries as the new benchmark.

**Only submit selected quadrants.** Previously every patch submitted 2304 triangles,
then the vertex shader dropped unused quadrants. The renderer now emits one record
per active quadrant and uses two instanced draws: a 512-triangle surface grid and a
64-triangle outer-skirt grid. Translating surfaces preserves their original diagonals;
only skirt ribbons rotate onto each quadrant's outer edges. Geometry, morphing,
height and material resolution are unchanged.

At the measured bookmark the old submissions were 94464 / 163584 / 354816 triangles
for bias 0 / +1 / +2; actual selected submissions are now 89856 / 158400 / 345024:
4.9% / 3.2% / 2.8% less. A patch with one selected quadrant saves 75%, but that is
not representative of this mostly-full-patch view. This is a primitive-budget
improvement; GPU body-pass timings are close enough that no frame-rate improvement
is claimed for this view.

This trades record traffic for primitive work. Records grow from one per patch to
one per selected quadrant (48 bytes each, at most four times as many), and use two
draws per terrain pass rather than one. GPU record capacity grows by 288 KiB and
two-frame mapped staging by 576 KiB; smaller shared meshes save about 211 KiB.
The dominant 70.27 MiB tile arrays are unchanged.

Two experiments were not retained: caching the direction lattice produced identical
outputs but no convincing latency gain and needed extra temporary storage; batching
15 distinct quadrant masks saved triangles but added enough draw overhead to lose
the intended benefit. The final two-draw path avoids that draw proliferation.

Validation: all 18 Release CTest cases pass. Regional albedo is compared exactly
against the full crater list across three seeds, six faces, multiple levels and
boundary samples. Mesh tests compare each quadrant's ordered triangles with the
original surface/skirt topology. Settled before/after captures at bias 0 and +1
are pixel-identical; bias +2 differs at one pixel by one 8-bit code value. The
300-frame wireframe/pan/TAA smoke completed with the Vulkan validation layer
enabled and no reported validation findings. The per-view render measurements
are recorded [here](data/terrain-render-budget-2026-10-25.json).

## Allocation recommendation

Keep 32 mesh intervals, 33 height samples and 65 material samples for the current
balanced mode. Keep the 2048-slot capacity: one view using few slots does not establish
a safe capacity for flights, wide FOVs or high bias. Do not raise global LOD merely to
sharpen material, or shrink the calibrated mesh merely to reduce triangle counts.

For comparison, keeping geometry/height fixed while changing both material maps:

| Material size | Linear material:mesh ratio | 2048-slot tile arrays |
| --- | ---: | ---: |
| 33x33 | 1:1 | 21.27 MiB |
| 65x65 | 2:1 | 70.27 MiB |
| 129x129 | 4:1 | 264.27 MiB |

These are allocation calculations, not tested quality presets. Halving material
resolution risks losing the very normal detail that makes close flight crisp;
doubling it quadruples almost all storage and generation work. Neither is a justified
default change from this assessment.

The next architectural improvement would be independent material refinement based
on projected texel footprint, retaining geometry refinement based on displacement
error. It needs separate residency/fallback and transition rules; simply changing
the ratio or applying a global bias does not implement that. At grazing angles,
filtering matters as much as density: current patch sampling explicitly uses mip 0,
so more texels alone can introduce shimmer. Mips/anisotropy require tile-border and
parent-transition validation before claiming temporal crispness.

## Reproducing measurements

The follow-up `terrain_footprint` target measures projected sample spacing rather
than CPU time. Run it without arguments. Its CSV includes sample counts, rejection
counts and percentile distributions. See the screen-space assessment below.

Build the opt-in `terrain_budget` target and run `terrain_budget 1007 1000` for CPU
generation measurements, separate height/material timings and output checksums.
It does not include worker scheduling, upload or rendering. The final argument is
the minimum measurement time in milliseconds per row, not a repeat count.

GPU captures use:

```text
orbital --headless --bookmark 9 --time 0 --tier-activate 100 --lod-bias 1
  --taa 0 --frames 1200 --width 1280 --height 900
  --benchmark captures/terrain-budget/run.csv --capture captures/terrain-budget/run.png
```

Use identical arguments for before/after runs; compare settled windows and decoded
image pixels. `terrain_submitted_triangles` counts the selected terrain geometry
once per view, not multiplied by scene/shadow/motion passes. `terrain_covered_quadrants`
records the selected area at each patch's resolution. GPU timings fluctuate; a lower
triangle count alone does not prove lower frame time.

## Screen-space relationship: follow-up measurements

There is no universal best texel-to-triangle ratio. The budgets have distinct jobs:

| Channel | Select detail from | What more resolution cannot repair |
| --- | --- | --- |
| Mesh | Projected displacement/curvature error, especially silhouette and occlusion | Missing high-frequency shading detail |
| Height samples | Vertices and morph samples actually consumed by the mesh | Silhouette detail without corresponding geometry |
| Albedo | Projected footprint and color variation | Shape or geometric occlusion |
| Bump/slope | Projected footprint and variation of the lighting normal | Silhouette or true displacement |

For perspective scale `f = screen_height / (2*tan(vertical_fov/2))`, a small
view-transverse geometric error `e` at view depth `z` projects to roughly `f*e/z`
pixels. This is the relevant geometry budget, not triangle edge length alone:
a long triangle on flat ground can be accurate while a shorter one on a crater
rim can miss an important silhouette. Height quantization is already much finer
than the present geometry target: the 0.1-radius R16 range has steps of about
1.53e-6 radii, or about 0.015 pixels per step at depth 0.08, 900px and 60 degrees.
Increasing height precision or height texture size is not the first intervention
for that camera.

`terrain_footprint` projects finite differences in the material lattice and reports
the singular values of that two-dimensional mapping: the long and short axes of
a texel's projected footprint, in pixels per texel. A long axis above one means
magnification along that direction; a short axis below one means minification
along the other. Both can happen simultaneously at glancing incidence.

At radius 1.08, 60-degree FOV and 900px height, seed 1007:

| View / bias | Median mesh edge (px) | Median material long axis (px/texel) | Median short axis | 90th-percentile anisotropy |
| --- | ---: | ---: | ---: | ---: |
| Nadir / 0 | 9.12 | 4.78 | 3.79 | 1.37 |
| Nadir / +1 | 7.85 | 4.56 | 3.55 | 1.43 |
| Nadir / +2 | 3.92 | 2.28 | 1.77 | 1.43 |
| Glancing / 0 | 6.73 | 4.45 | 0.93 | 28.18 |
| Glancing / +1 | 4.05 | 2.64 | 0.57 | 27.08 |
| Glancing / +2 | 2.32 | 1.50 | 0.27 | 28.36 |

These are unweighted samples from selected quadrants, not visible-pixel percentages.
They use direct procedural heights rather than the exact morphed/quantized mesh,
reject samples outside the frustum, and do not ray-test local occlusion. The glancing
camera stands 18 degrees around the body from its target; this is not an 18-degree
surface incidence angle. The camera differs from bookmark 9, so its counts should
not be compared directly with the earlier capture table. Full distributions are in
[the footprint CSV](data/terrain-footprint-2026-10-25.csv).

The current 2:1 ratio thus still magnifies material samples over several pixels.
That indicates potential shading-detail limitations, not a measured image error:
smooth albedo can remain accurate at that spacing. At glancing angles, higher
resolution alone cannot fix the simultaneous minification across the compressed
axis; mip/anisotropic filtering must accompany material refinement.

The concrete next experiment should separate the material channels and selection:

1. Retain the calibrated mesh and height grid. Measure displacement error before
   changing its density; do not force subpixel triangles just to sharpen bumps.
2. Test 33x33 albedo with 65x65 slopes against the existing 65/65 pair. This would
   reduce tile-array allocation from 70.27 to 45.77 MiB at 2048 slots, providing a
   budget for finer slopes where needed. This is **not yet quality-validated**:
   albedo currently includes a slope-dependent tint, so it is not purely low-frequency.
3. Select finer slope/material tiles independently from mesh LOD, using the projected
   footprint, residency cost and generation budget. A long-axis target around 1â€“2
   pixels/texel is a useful starting experiment for crisp normals, not a proven
   universal threshold. Prioritize by visible contribution rather than refine every
   channel and every patch globally.
4. Add border-correct material mips and anisotropic sampling, and test motion with
   TAA on and off. Normal filtering should preserve unresolved variation through
   roughness where appropriate; a static sharp screenshot alone cannot establish
   temporal fidelity.

No resolution or quality defaults were changed on the strength of this approximate
footprint report. It supplies the measurements needed to test those choices separately.
