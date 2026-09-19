# Ideas under discussion

Not commitments and not committed: directions talked through on 16 September
2026, kept here so the reasoning is not lost. Items that get decided move to
DECISIONS.md; items that get scheduled move to the plans file.

## New-generation Vulkan features with a 1080 Ti floor

No second backend. Vulkan is one API with optional features, so the split is per
feature, not per device generation. Pascal already takes timeline semaphores,
buffer device addresses, descriptor indexing and dynamic rendering through
current drivers; what it lacks is mesh shaders, ray queries and the fast fp16 and
int8 paths.

- A capability struct filled at device creation; each feature that matters gets
  two implementations of one pass, chosen once at start-up, never per frame.
  Candidates: a mesh-shader belt draw against the cull-and-indirect path, ray
  queried body shadows against the shadow maps.
- Both paths produce the same image to the quick check's tolerance, so the check
  runs against both and the fallback stays honest. A feature that cannot meet
  that is a quality tier and shows in the panel as one.
- Instance records, frame constants and heap layouts stay common; only pass
  code differs. The high path must not leak into shared data layouts.

The abstraction as it stands (commands, passes, heaps, PSOs) is nearly enough;
what is missing is the feature table and the discipline of pairs. Decide once a
specific feature is wanted.

## CPU-owned belt: spatial structure

Chunk by the motion, not by a grid. Most rocks are on near-Keplerian orbits, so
sort by orbital band and angular sector (plan L.4); a sector is then a
contiguous range in the arrays, which is what the SIMD integration sweep and a
coarse cull both want.

- Sectors drift as a whole, so a sector's bounds come from its own sweep once a
  frame; the well-behaved rocks never re-sort.
- Rogues (post-collision, fast, unpredictable) get their own small list with a
  generic integrator and a per-rock CPU cull; a few hundred against a frustum is
  nothing. A rock becomes a rogue at the collision and returns to a sector once
  its orbit settles, by a periodic re-bin that walks only the rogue list.
- Spawns and deaths are appends and holes in the sector arrays, with slack, so
  nothing reorders.
- No per-rock hierarchy: the GPU refines per rock at 0.19 ms already, so the
  CPU's job is coarse rejection and state production, and the sector sort gives
  locality and cull granularity in one structure.

## Procedural planets for close flybys and atmospheric flight

A geosphere with an equirectangular map gives up near the ground. Cube sphere
with a quadtree per face, which fits the existing tiers:

- Geometry: six faces, each a quadtree of patches subdivided by projected size
  with the body levels' hysteresis; patches displaced by the height field on the
  GPU; skirts or edge stitching for the seams between levels.
- Mapping: each face has its own texture space, so no polar singularity and
  near-uniform texel density. Content is generated per patch into a tile cache
  (height and material tiles at the patch's resolution); the cache is the whole
  memory story.
- Detail: a noise pipeline evaluated at rising octaves as the patch level rises,
  over a baked coarse base for the bodies that should look like the real ones.
  The top levels read the existing maps, the lower levels add procedural relief.
- Precision: camera-relative doubles for patch origins (already done for
  bodies); reversed-Z (plan L.10) becomes mandatory rather than deferred.
- Atmospheric flight: the per-pixel march works from inside the shell; the
  terrain patches then need aerial perspective, one extra term in the surface
  shaders fed by the same scattering functions.

The distant tiers stay as they are: the cube sphere switches on only above the
size where today's sphere mesh is at its finest level, so far views and their
checks do not change.

Suggested order across the three: belt (on the plan already), then planets (the
largest new piece, gates the depth changes), then the feature split (decide per
feature when one is wanted).

## Self shadowing in the belt

Two problems under one name.

**A rock on itself** (craters, concavities), cheap to dear:

- Baked ambient occlusion per vertex or texel on the sixteen library shapes: a
  one-time bake of seconds, one attribute, darkens cavities under sky and fill.
- Bent normals from the same bake: the diffuse normal tilted toward the open
  sky, so crater floors lose direct light under a grazing sun with no trace.
- Horizon maps for the direct term: per texel the terrain's horizon angle in a
  handful of directions, the sun's elevation compared against it; one texture,
  a few instructions, a real shadow edge that moves with the sun. The middle
  distance technique.
- A short height-field trace along the sun, as the Moon's crater walls do:
  per pixel and dear, so only for rocks large on screen. The parallax-occlusion
  path in surface_rock.slang already marches the height field for the view; the
  sun march is the same machinery in another direction at the same cut-off.

Split: AO and bent normals on everything; horizon or sun trace only above the
near cut-off. Below it the shadowing is sub-pixel and nothing is missed.

**Rock on rock** (the belt occluding itself). The belt is thin and sparse: the
chance a rock sits in another's umbra is tiny outside clumps, while the aggregate
dimming toward the sun through dust and small rocks is real and smooth.

- The aggregate term is the belt transmittance map splatted from the sun's
  direction, which exists (F3 light map, F4 extinction): statistically right,
  smooth, correct for every rock at every distance.
- Discrete shadows matter only between near, large rocks in dense views: a
  sun-aligned shadow map over the culled near list only (a few thousand rocks),
  low resolution, a large filter. No contact hardening.
- Screen-space contact shadows, a short depth march toward the sun, catch two
  rocks touching or one resting in another's shade for a few pixels; cheap, and
  it is the case the eye notices most.

Safe to approximate: everything below a few pixels of radius gets the aggregate
term only (the splats already carry the belt map's attenuation); penumbrae are
ignored, since the sun is a fraction of a degree and shadows at rock scale are
near hard. Not to approximate: each rock's terminator, which is how a rock is
read and comes from the normal and fill light already in place.

For this renderer: baked AO and bent normals on the sixteen shapes first (every
rock, every size, near free), then the sun march on the near path, and
rock-on-rock left to the belt map plus a contact-shadow march if a dense view
asks for it.

## The rock state upload: what it costs and how to make it small

Measured 16 Sep on the 1080 Ti with the float4 (x, y, z, phase) state: the sweep itself is
0.05 ms on seven workers at 280k rocks (0.11 at 520k); the write into the host-visible staging
heap streams at about 6 GB/s whatever the thread count, so 4.5 MB is 0.7 ms and the high tier's
8.3 MB is 1.4 ms, and the GPU side of the copy is 0.02 to 0.03 ms in the cull scope. Today the
whole write lands on the frame time because the frame loop is serial: wait for the previous GPU
frame, prepare, submit. Ranked by what they buy:

- **Overlap first** (done 16 Sep for the rock state: two staging slots by frame parity, written before the wait; moving frames now equal standing ones). Prepare frame N while the GPU is on N-1, then wait, then submit. Needs a
  second copy of everything the CPU writes per frame (the 4 MiB mapped region and the rock
  staging, alternating by frame) and the cull readback taken one frame later. No added latency.
  Every GPU frame is longer than the preparation, so the write costs nothing after this; the
  rest of the list is for when the population grows or physics arrives.
- **Send only what changed.** Rocks on their closed-form orbits need no per-frame state: the GPU
  can keep rotating them by the band angle as it did before, and the CPU uploads only the
  rogues, the rocks whose motion diverged after a collision or a kick (the rogue list above).
  Upload then scales with the rocks doing something interesting, not with the population. The
  sector partition (L.4) makes the bookkeeping cheap. Until physics perturbs rocks, the honest
  minimum upload for the belt is zero.
- **Fewer bytes per rock.** Positions relative to a sector origin fit three 16-bit integers at
  sub-unit precision and the phase one more: 8 bytes instead of 16, 2.2 MB instead of 4.5 per
  frame. Dropping y while it does not move gives 12 bytes with no loss (the (x, z) pair was
  measured at 0.35 ms). Half floats do not fit absolute positions at radius 1000, the precision
  is half a unit there.
- **Update far rocks less often.** Sub-pixel rocks drawn as splats have no motion vectors to keep
  consistent, so their state can be refreshed every fourth frame with a four-step increment; with
  sectors the CPU knows which sectors are far, so this is a per-sector cadence (the plan's "far
  tail across frames").
- **Memory type.** BAR is the GPU-visible memory the sweep already writes; its limit is the
  write-combining path, and nothing writes across PCIe faster. What NoGraphicsAPI lacks is a
  host-cached, non-device-local upload type: the CPU would write at memory speed, threads scaling
  for real, and the copy engine would pull the data over PCIe on the GPU timeline. That moves the
  cost off the CPU rather than removing it, so it pays only once the CPU is the bottleneck and the
  copy is not; a change to the vendored fork.
- **Compression.** General-purpose compression does not fit: float positions are high entropy and
  a GPU decode costs more than the transfer. The only form that works is the delta, and its
  logical end is the second point: the GPU integrates the well-behaved rocks itself and receives
  only exceptions.

Order: the frame-loop overlap now, then L.3 and L.4 as planned, the rogue-list upload when
physics makes rocks diverge. The high tier doubles every number above (520k rocks, 8.3 MB) and
nothing else: only the active tier's prefix is written and copied.

## The tile normal: slopes in rg16 instead of a body-frame normal in rgba8

The near tier stores each tile's normal as a body-frame unit vector in `rgba8_unorm`, the height
repeated in alpha (`generate_colour_tiles`). Three things are wrong with that: eight bits per
component quantise the direction at about a third of a degree, which bands at grazing light; the
third component is redundant once the surface is a height field over a known sphere; and the alpha
is dead, since the patch path samples `.xyz` only and the crater-wall trace reads the equirect map.

Measured over the shipped seed, 32 random patches at each of levels 2, 4, 6, 8 and 10, every texel
of each 65x65 tile, the surface gradient in radii per radian (the tangent of the tilt off the
sphere) and the angular error each encoding costs:

| level | \|g\| p50 | \|g\| p99 | \|g\| max | tilt at max |
|---|---|---|---|---|
| 2 | 0.132 | 0.762 | 1.327 | 53.0 deg |
| 4 | 0.156 | 0.963 | 1.323 | 52.9 deg |
| 6 | 0.216 | 1.047 | 1.247 | 51.3 deg |
| 8 | 0.145 | 1.075 | 1.181 | 49.7 deg |
| 10 | 0.157 | 0.580 | 0.604 | 31.1 deg |

| encoding | bits/texel | error rms | error max | notes |
|---|---|---|---|---|
| rgba8 body frame (today) | 32 | 0.184 deg | 0.38 deg | a quarter of the bits unused |
| rgb10a2 body frame | 32 | 0.046 deg | 0.096 deg | two lines to change, nothing else bought |
| octahedral rg16 body frame | 32 | 0.001 deg | 0.004 deg | no frame needed, but folds, so it will not compress |
| **slope rg16, scale 2** | **32** | **0.001 deg** | **0.002 deg** | **nothing clipped at any level** |
| slope rg16, scale 1 | 32 | 0.13 deg | 6.4 deg | 0.06 to 1.5 percent of texels clip |
| slope rg8, scale 2 | 16 | 0.35 deg | 0.63 deg | worse than today |

Why the quantisation is visible at all: a diffuse term at incidence `theta` changes by `tan(theta)`
times the normal's error, so today's step of 0.005 to 0.008 rad is a 1 percent luminance step at 60
degrees and 3.7 percent at 80. One percent is about where a smooth ramp shows a contour, so the
banding is predicted everywhere within thirty degrees of the terminator, which is where these views
are judged. At rg16 the same step is 0.07 percent at 80 degrees.

The scale is a fixed constant, not per tile. The maximum gradient anywhere measured is 1.33, so 2
carries fifty percent of headroom and clips nothing; a per-tile scale would decode a shared edge
texel differently on each side of a tile border, which is the artefact class this tier has been
fighting all week. Encode `g / 2 * 0.5 + 0.5` per channel, decode `(c * 2 - 1) * 2`.

The frame the slope is against is the face's, orthonormalised, and both sides can build it from the
direction alone: `T = normalize(FACE_S[face] - d * dot(FACE_S[face], d))`, `B = cross(d, T)`, then
`N = normalize(d - T * gx - B * gy)`. Six ALU in the fragment shader, which already carries the
direction as `localNormal` and the face in `tileFace`. It is level-independent, so a parent tile's
slope at a point is in the same frame as its child's and the morph blend is a plain lerp of the two
gradients. It has no singularity inside a face, unlike the equirect path's east-south frame, which
degenerates at the poles the cube sphere covers.

This does reintroduce a tangent frame, which 05e9fc3 removed on purpose so that two faces could not
disagree at a cube edge. The disagreement it avoided was in the *derivative*, and the ring-texel
machinery that fixed it stays. A frame per face is safe because both faces reconstruct the same body
normal from their own frames: the stored bytes differ across the edge, the decoded normal does not.
`terrain_tests`'s cube-edge check already decodes body normals through a lambda that takes the face
and the texel and ignores them, so it is the guard for this, and its tolerance can drop from 0.02 to
about 1e-3.

Three honest notes against the case:

- **The linear-blending argument is almost worthless here.** Gradients average correctly where unit
  normals do not, so lerping slopes is the blend that matches the vertex path's lerp of heights. But
  measured at a morph of 0.5, normal-lerp and slope-lerp differ by 0.03 degrees at level 4 and less
  below it (0.28 degrees at level 2, 1.4 worst). It only starts to matter if detail octaves are
  added on top later, where gradients compose and normals need a blend hack.
- **Block compression is a memory win, not a precision one.** BC5 is 8-bit endpoints per 4x4 block,
  so a block holding a crater rim lands back near today's error; only smooth blocks beat it. The
  reason to pick a two-channel layout now is to keep that door open at a quarter of the bytes, not
  to expect it to look better.
- **The `1/(1+h)` factor costs as much as the quantisation being fixed.** The true normal of
  `(1+h)d` is `d - g/(1+h)`, and both bakes drop the divisor, for 0.10 to 0.13 degrees rms and up to
  0.54. It is smooth, so it makes no contours and nothing on screen reads as wrong, but if the
  normal is to be accurate to a fiftieth of a degree it is one token in each bake. Fix both bakes or
  neither: the far tier's equirect map and the near tier's tiles must agree at the tier switch.

The far tier already stores slopes: `bake_terrain_maps` writes `normalize(-east, -south, 1)`, the
same gradient, sine-compressed by the normalise and flattened into eight bits. So this moves the
near tier toward the far one rather than away.

Cost of the change: `rg16_unorm` is four bytes per texel like `rgba8_unorm`, so the arrays stay at
17 MiB for 1024 slots, the staging offsets stay four-byte aligned, and the copy path is untouched.
`VK_FORMAT_R16G16_UNORM` is already mapped in the fork and the height array proves 16-bit unorm
sampling on this hardware. The near shots move, so the three of them get re-accepted.

## Near-tier streaming: height first, and a quadtree chosen in one go

The near tier reaches a fine level by a pipeline: split, request the child tiles, wait for them to
arrive, then split again. `visit` descends into a child only when its tile is resident
(`terrain_tier.cpp`, the `in_range && resident` branch), so each level of depth costs a generation
round trip. Measured 2026-09-18, seed 1007, 6 workers on this machine:

- **Colour is 94 percent of a tile.** The height tile is 1.2 ms; albedo and slope together are 19 to
  21 ms, at every level (33 by 33 heights against 65 by 65 colour samples, each dearer).
- **Throughput, not the budget, is the limit.** Six workers at 21 ms is 4.7 tiles a frame at 60 Hz,
  against a `generate_per_frame` of 8.
- **A cold settle at bookmark 9 takes about 60 frames**: against the converged image, 0.16 percent of
  pixels are still moving at 30 frames and none at 60. Roughly 40 of those frames are colour
  throughput for the 200-tile settled set; the rest is the per-level round trip.

Two changes, independent, both worth having.

**Generate the height tile first and let the patch draw from its parent's colour.** The fragment
shader already samples the parent's albedo and slope at the right quadrant UV and blends them by the
morph (`surface_airless.slang`, the `parent < ORBITAL_TERRAIN_SLOTS` branch), so the sampling is
built. What is missing is a residency flag that separates height from colour, and a colour weight
independent of the geometric morph, so a height-only patch draws its own shape with its parent's
colour at weight 1. Better than the 17x per tile suggests: a patch that splits again before its
colour lands never pays the 20 ms at all, which is the common case while flying in.

**Choose the tree geometrically and let residency trail it.** The split test needs no tile content --
it is distance against `level_error` -- and since `render/terrain-cull-fixes` gave a node without a
tile its nearest resident ancestor's reach, the distance is sound for one never generated. So the tree can be
selected to its target depth in one pass, the whole set requested at once, and the deepest resident
prefix drawn, with a parent covering the quadrants of missing children as it already does. That is
CDLOD as it is usually built, and it turns N sequential round trips into one.

Watch three things when building it: `node_budget` (4096) becomes the live constraint during a dive,
since the tree expands fully instead of trailing the tiles -- the settled count at bookmark 9 is 250
to 350, so there is room, but a fast wide dive is the case to measure; the morph still needs the
parent tile, so the fill order must stay top-down, which the coarse-first request sort already gives;
and the inherited reach is wider than a measured tile, so selection can over-split by a level and
settle back, bounded by one level for the same reason D6 was in `terrain_culling.md`.

Together the transition costs about `tiles * 1.2 ms / workers` -- some 40 ms for the bookmark-9 set
against 700 ms of colour strung across nine levels -- with the colour filling in behind.

## Smoothing the morph ramp

The CDLOD morph weight is a linear ramp, `saturate((dist - 0.7 * range) / (0.3 * range))`, floored
by the arrival fade. The position it drives is continuous but its derivative is not: it jumps where
the ramp meets 0 and 1, so as the camera flies the vertices start and stop sliding abruptly and the
boundary between the two reads as a faint wave crossing the terrain. `smoothstep` over the same band
removes it for one instruction:

```hlsl
float m = max(smoothstep(patch.morph.x, patch.morph.y, dist), patch.morph.z);
```

Checked against `TerrainCDLODBabylonJs` (2026-09-19), which uses the same linear ramp: neither this
nor Strugar's original smooths it, so this is a refinement rather than a correction.

Two things to watch. The morph must still reach exactly 1 at the hand-over, which smoothstep does at
the band's end, so the pop-free argument survives -- a patch is fully at its parent's shape when the
parent takes over, and the selection's nearest-point distance keeps every vertex at or beyond the
band end by then. And `test_morph_bands` reports only 1.013x of headroom between the morph start and
the child's hand-over, so the band is thin already; smoothstep does not widen it, but anything that
moves the 0.7 has almost nothing to give.

It changes every frame the terrain is in, so the near shots want re-accepting and the change wants a
look at a slow fly-in, where the wave is what one is looking for.

## Split atomically, and stop covering a child that has no tile

`visit` sets a quadrant bit for a child for two reasons, and only one of them is CDLOD:

```cpp
if (!in_range) { collapse(child); out_of_range++; }     // Strugar's case
else { starved++; if (no slot) request(child.key, ...); } // ours
```

The first is canonical, down to the wording. `fstrugar/CDLOD`, `CDLODQuadTree.cpp`:

```cpp
// We don't want to select sub nodes that are invisible (out of frustum) or are selected;
// (we DO want to select if they are out of range, since we are not)
bool bRemoveSubTL = (SubTLSelRes == IT_OutOfFrustum) || (SubTLSelRes == IT_Selected);
```

with `SelectedNode { bool TL, TR, BL, BR; }` -- our quadrant mask, same three-way rule. His own
StreamingCDLOD keeps that mask for the range case and streams by a separate flag,
`IncludeAllNodesInRange`, which puts nodes in the selection buffer so the streamer learns what to
fetch. He never covers a quadrant because its data is late.

The second reason is ours, and it breaks the argument the morph rests on. A quadrant covered
because its child is **out of range** sits beside siblings whose distance is near the hand-over, so
they are nearly fully morphed and their shared edge already matches the parent's: that is the whole
of CDLOD's continuity claim. A quadrant covered because its tile is **late** sits beside siblings
anywhere in their range, morph included -- deep in it their morph is 0 and they carry their own
shape, while the covered quadrant carries the parent's. The gap is the level's full geometric error.
That is what `test_morph_streaming` counts: 0 boundary vertices break the morph settled, 330 during
streaming even with the arrival fade, 3030 without it. The skirts are hiding that.

So: do not descend at all unless every in-range child has its tile. The node draws whole until then.
Partial masks stay for the range case, as they should, and every seam is back inside the one level
the morph is defined over.

What it costs is refinement latency, and what it buys is that the failure is always "coarser for a
while" rather than "cracked". That distinction is the point, and it does not depend on how long a
tile takes: here they are generated on the CPU, 1.2 ms for heights and twenty times that with
colour, but a tile could as easily be a memory cache hit, a disk read or a network fetch, with a
tail no scheduler can predict. Nothing in the design should rest on the number -- only on the
degradation being graceful whatever it is.

Two consequences to build in rather than discover:

- **Serve siblings as a group.** With all-or-nothing splits, a budget spread evenly over many
  half-filled quads completes none of them; `choose_generation` sorts by level then screen size and
  would happily do exactly that. Requests want to carry their sibling set and be served together,
  and a node whose set is under way should outrank one that has not started.
- **The stuck-tile backstop matters more.** One tile that never arrives now holds a whole node at
  its coarse level instead of one quadrant. The reclaim sweep already covers it, and `Audit`'s
  pending age is what to watch: see the trace in renderer_terrain.cpp.

Skirts do not go away with this -- the arrival fade still puts a fresh tile at its parent's shape
for fifteen frames beside settled neighbours -- but they stop carrying the case they were never
sized for. Their drop is a flat 0.002 radii against a level-3 geometric error of 0.00226 and level
2's 0.0048, so at coarse levels it is shorter than the gap it hides and gets away with it only
because those patches are far enough for the crack to fall under a pixel.
