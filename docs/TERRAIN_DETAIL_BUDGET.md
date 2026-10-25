# Minor planet detail budget

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
   footprint, residency cost and generation budget. A long-axis target around 1–2
   pixels/texel is a useful starting experiment for crisp normals, not a proven
   universal threshold. Prioritize by visible contribution rather than refine every
   channel and every patch globally.
4. Add border-correct material mips and anisotropic sampling, and test motion with
   TAA on and off. Normal filtering should preserve unresolved variation through
   roughness where appropriate; a static sharp screenshot alone cannot establish
   temporal fidelity.

No resolution or quality defaults were changed on the strength of this approximate
footprint report. It supplies the measurements needed to test those choices separately.
