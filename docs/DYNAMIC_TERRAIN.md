# Dynamic planet terrain: the minor planet's near tier

The minor planet draws close in as a CDLOD cube sphere whose shape and colour come from per-patch
tiles of textures rather than CPU-built vertices. One shared grid mesh draws every patch; levels meet
without cracks by vertex morphing. Tiles are generated on a CPU worker pool and appear as they are
ready. The design is meant to scale to a whole-planet tier later, and to a compute generator that
fills the same tile contract without a renderer change.

Historical record of how this got here is in `docs/DECISIONS.md`. This document is what the code does
now and what is still wrong with it.

## Terms

Five things are counted separately here and are easy to confuse, because a patch usually has one of
each and the limits on them are unrelated.

| Term | What it is | How many |
|---|---|---|
| **Patch** | A cell of a face's quadtree, named by a `PatchKey` (face, level, x, y). The unit of drawing. | one draw instance each |
| **Node** | The quadtree bookkeeping for a patch the selector is tracking: its key, bounds and child link. 64 bytes, CPU only. | `node_budget`, 4096 |
| **Tile** | A patch's three textures: heights, albedo, slope. What a worker generates and the GPU samples. | one per resident patch |
| **Slot** | One layer index shared by the three texture arrays, holding one tile. The cache entry, LRU-evicted. | `slot_count`, 1024 |
| **Ring** | One entry of the staging ring buffer: a CPU-writable, GPU-readable scratch block sized for one tile's three planes. | `tile_ring_count`, 32 |

A **ring** is the hand-off lane between a worker thread and the GPU, and the name is literal: entries
are taken in order by `ring_head`, which wraps. A worker generates a tile *into* a ring entry; later,
inside the command buffer, a copy moves those bytes from the ring into the tile's array layer. Each
entry cycles `free -> worker -> upload -> free`, and ownership is explicit rather than by frame number
because a job can outlive any frame count. Rings are what cap generation: the tier is handed the free
ring count as its per-frame budget, so it never asks for more tiles than there are lanes to carry them.

Not to be confused: a **slot** is where a tile *lives* (GPU, for as long as it is cached); a **ring** is
how it *travels* (CPU, for a few frames). Running out of slots evicts something; running out of rings
only delays a request to the next frame.

## Decisions that stand

1. Tiles live in texture arrays, one layer per slot: heights `r16_unorm`, albedo `rgba8_unorm` in
   linear light, slope `rg16_unorm`. Not atlases.
2. Selection is by distance ranges per level, derived from that level's worst error, so a patch's
   morph state at an edge is a function of distance alone. Per-tile error is not used for selection.
3. The colour planes are finer than the height plane by `tile_colour_ratio` (2): 32 quads of geometry,
   65 colour texels a side. Both sides are named constants; block compression later wants a multiple
   of four.
4. The far tier stays the sphere levels with the equirect maps, switching above a projected radius.
5. The face warp is Everitt's, `w(s) = tan(0.8687 s) / tan(0.8687)`. Texel areas across a face stay
   within about 11 percent, against 43 for a plain tangent warp.
6. Generation stays on the CPU on a persistent worker pool.
7. Texture compression of tiles is out of scope. Heights stay 16-bit raw in any case.
8. The tree size (`node_budget`) is not the cache size. A node is 64 bytes of CPU bookkeeping and
   only drawn patches need slots, so the two have no reason to share a number. They did once, and
   capping the tree at `slot_count - 64` starved splits long before memory justified it.
9. `patch_level_max` is 11: cells of 90 degrees over 2048, quads of 24 urad, about 10 m on this body.
   Every field carrying a cell index is guarded by a `static_assert` in `terrain_patch.hpp`, because
   overflowing one truncates silently. 12 is the ceiling `PatchKey::packed`'s 12-bit x and y allow.

## Design

### Patches, tiles, slots

A patch is a cell of a face's quadtree (`PatchKey`: face, level, x, y, packed to 32 bits).
`patch_bounds` gives its cap centre, angular radius and angular size, with that radius' cosine and
sine for `patch_support`, which both culling tests are built on.

A tile is that patch's textures: `tile_side` (33) squared heights and `tile_colour_side` (65) squared
albedo and slope texels. Texel (i, j) is the terrain sampled at cell coordinates `s0 + i * size / 32`
through the warp, so a child's even texels are its parent's texels at the same directions and a fully
morphed child edge lands on the parent's edge.

The slope texel holds the terrain's gradient over `[-tile_slope_scale, tile_slope_scale]` (3 radii per
radian; the terrain's steepest is 1.33), from central differences with a one-texel ring around the
tile. Past a cube edge the ring takes the *neighbouring face's* own texel one step inside its edge, not
this face's warp extrapolated, since the two faces' lines of constant t differ by up to six tenths of a
texel along the edge.

The tangent frame is built from the sphere's own direction and the face's s axis, never the face's
axis: `T = normalize(FACE_S[f] - d * dot(FACE_S[f], d))`, `B = cross(d, T)`, decoding as
`normalize(d - T * gx - B * gy)`. At a cube edge the two faces hold the same gradient in their own
frames and read the same body-frame normal out. The frame does not depend on level either, so a parent
tile's gradient is in the same frame as its child's and the morph zone lerps the two gradients.

A slot is one layer index shared by the three arrays. `TerrainTier` owns the slot cache; the renderer
owns the arrays.

### The shared grid

One static mesh, `patch_grid_mesh()`: 33 by 33 vertices holding `(x, y, skirt)` plus a ring of
4 × 33 skirt vertices the shader drops by `patch_skirt_drop` (0.002 radii) along `-d`. Quads are
triangulated on a uniform `(x,y)–(x+1,y+1)` diagonal — **this is load-bearing**, see the open defects.

The skirt is a fallback only: with correct morphing no crack exists between neighbours one level apart.
It hides the transient where a neighbour is two levels coarser while children load.

### Per-patch records

`PatchInstance` in `scene_shared.h`, 48 bytes: `cell` (s0, t0, size, level), `morph` (start and end
distance in radii, the arrival fade floor under them, and an unused fourth), `tile` (face, slot, the
parent's slot or the slot count for none, then the child's quadrant, which sides lie on a cube edge,
and which grid quadrants to draw). One record per drawn patch, written each frame into a host-visible
slot chosen by frame parity and copied beside the tile uploads. `ORBITAL_ROOT_PATCHES` puts the vertex
shaders on the patch path.

### The vertex path

`shaders/surface/patch.slang`, shared by `surface.slang` and `motion.slang`:

1. `g = position.xy` (0..32), `skirt = position.z`.
2. `m = max(saturate((dist - morph.x) / (morph.y - morph.x)), morph.z)`, the second term the arrival
   fade floor.
3. `g' = g - frac(g * 0.5) * 2 * m` — odd vertices slide to the even neighbour along each axis.
4. Height is a bilinear read of the height array at layer `slot`, coordinates `g'`.
5. A quadrant the record does not list is dropped.

The motion pass places the previous position through the same morph and skirt drop, or morphing patches
carry false motion vectors into TAA.

### Selection

`level_error[level]` is the worst measured geometric error per level in radii, from
`terrain_tests --calibrate`: a uniform sample per level (64 patches, 1024 below level 9), then a hill
climb around the worst one found, because relief clusters and a uniform sample of a deep level misses
it entirely. Levels 9..12 used to be extrapolated at 2.3x a level and ran about **half** the true
error, which understated their ranges and held those levels back until the camera was far too close.

The whole table must come from one method. `test_morph_bands` requires every adjacent ratio to exceed
`1 / 0.7`, so a level measured more thoroughly than its neighbour shrinks the step between them and
breaks the band it hands over in — measuring only 9..12 fails at the 8->9 step. Re-derive all of it
or none of it.

Each frame the tier derives
`range[level] = level_error[level] * height_pixels / (tan_y * error_pixels)` with `error_pixels` 3 in
`cull_bodies`' units, and `morph.start = 0.7 * range[level]`.

Visiting from the six roots: a node out of the frustum or past the horizon collapses. A node whose
nearest distance is under `range[level + 1]` wants children, with 0.8 hysteresis on the way back.
A child is drawn only within its own range and once resident; the node draws every other visible
quadrant itself, so nothing is drawn beyond its level's range and neighbours at one level morph alike.
Residency is therefore per quadrant: a patch refines as each child's tile arrives.

Distances come from `nearest_distance`, the closest the camera can be to any point of the patch's cap
over its height shell. The shader tests each vertex, which is never nearer, so a child drawn within its
range is fully morphed wherever an unsplit neighbour meets it.

**A resident tile supplies its own height range** rather than the global `[-0.05, +0.05]` shell,
measured from the tile it uploaded and padded by one `r16_unorm` quantum plus float decode roundoff.
The asymmetry here is deliberate and easy to get backwards: **the split test keeps the global shell**,
because a tile bounds its own bilinear surface and says nothing about relief its children will reveal.
Drawing, gating and culling use the tight range.

### Culling

Both tests take the resident tile's range, padded by `descendant_relief[level]` — how far a child's
heights are measured to reach outside its parent's, twice the worst of 120 to 400 patches a level,
0.0037 radii at level 1 and nothing by level 9. A patch with no tile keeps the global shell, so the
bound stays sound while the relief is unknown.

The horizon test allows only what stands above the reference surface, per patch, rather than a
constant 17.75 degrees taken from the shell's `height_max`. The terrain's true relief is -0.029 to
+0.023 radii, so the constant let a patch 130 km past the limb survive.

Those two were one defect seen twice: culling a patch by a shell it does not occupy. Together they
cut the drawn set by a fifth to a half, and the share of it provably off screen from two thirds to a
third. `test_cull_waste` holds the line by re-deriving visibility from the patch's own samples.

**Both tests are `patch_support`**, the largest `dot(normal, point)` over the shell — every direction
of the cap at every radius of the interval. A patch is outside a plane when its support falls below
that plane's offset, and hidden past the horizon when its support along the eye falls short of the
occluder's radius squared, since a point is on the near side of a sphere of radius R seen from C when
`dot(point, C) >= R * R`.

The maximum is closed-form, which is why this is cheaper than the sphere it replaced rather than
dearer. Over a cap of radius θ about `centre`, the direction closest to a normal is the normal itself
where it points inside the cap and the rim otherwise, so the largest `dot(normal, direction)` is
`cos(max(0, φ - θ))` for `φ` the angle to the centre — expanded as `cos φ cos θ + sin φ sin θ`, one
dot and a square root, no inverse trig at all. The radius that maximises it is the far end of the
interval when that reach is positive and the near end when it is not. `patch_bounds` carries `cos θ`
and `sin θ` so a node never recomputes them, and `update` carries the frustum into the body's frame
once, so a plane test is a dot and a compare.

A cap is not a ball, and the difference is the whole point. A sphere drawn around a cap also contains
the space behind it, which is most of its volume once the cap is wide; close to the ground that space
alone reaches every frustum plane, since they all pass through the camera. Measured over four views,
the support test draws 34 and 34 patches where the sphere drew 44 and 47, and takes 0.047 and 0.035 ms
against 0.070 and 0.056.

The occluder for the horizon is the shell's floor, never the reference surface: ground below that
surface sets the horizon further out, so assuming the surface itself would hide terrain that can in
fact be seen over it. That costs about one patch a view and is the sounder of the two.

### Generation and upload

`WorkerPool` (cores minus two threads) takes whole tiles. A staging ring of 32 entries each holds one
tile's three planes. Ownership is explicit — `free`, `worker`, `upload` — because a frame number cannot
say "still working": a job outliving any frame count would otherwise hand its buffer to the next worker
mid-write. Frame numbers only guard reuse after a copy has been recorded.

Each slot assignment issues a **generation stamp**, once, never reissued. Slot and key cannot identify
a generation: evict a patch and ask for it again, or change the detail setting and regenerate, and the
same patch is handed back the same slot, so an older result would pass a check made of both. Stamps
start at 1 and `invalidate()` clears slots to 0, so nothing in flight across an invalidation is
accepted.

**A slot handed out must come back.** `choose_generation` commits the slot — key in `slots_by_key_`,
stamp issued, `resident` false — before the renderer has found a ring for it. A slot left in that
state is unreachable: `visit` re-requests only keys with *no* slot, eviction passes over anything not
resident, and `free_slots_` refills only on `invalidate()`. So the patch never refines again and the
slot is gone from the cache for the session. Two things close it:

- `release(slot, key, stamp)` hands a slot back, stamp-guarded exactly as `mark_resident` is. The
  renderer calls it if no ring was free, instead of dropping the generation on the floor.
- `choose_generation` sweeps for slots still pending past `pending_timeout` (240 frames) and reclaims
  them, whatever lost them. The sweep is a cursor over `sweep_window` (64) slots a frame rather than
  the whole array, so there is no queue to overrun and a slot waits at most 16 extra frames. `used`
  cannot drive this: a visible pending patch is touched every frame, so `assigned_frame` decides.

The renderer's path is unreachable today, and `ORBITAL_ASSERT` after `update()` says so: the tier's
budget *is* the free ring count, so every generation finds a ring. Nothing else enforces that coupling,
and it spans two files.

The copy and the residency are one decision, taken together in `record_terrain_uploads` inside the
command buffer. A tile counts as resident exactly when the upload that makes it so has been recorded;
uploads no frame recorded are retained, not dropped. A frame abandoned after preparation therefore
cannot leave a slot vouching for contents it never uploaded.

### Fragment path

The airless shader keeps the equirect path for the sphere. On the patch path it samples albedo and
slope at the patch's slot, rebuilds the frame with `patchTangentFrame`, and blends albedo and normal
toward the parent tile's through the morph zone, so a fully morphed child shades as its parent. The
crater-wall shadow traces the baked equirect map, as the sphere path does, so the field is continuous
across tile borders. `TerrainSettings::debug` (`--terrain-debug`, the panel's Patch view) draws the
path one term at a time so a seam can be attributed.

### Memory

| Item | Size |
|---|---|
| Height array, 1024 layers of 33×33 r16 | 2.1 MiB |
| Albedo and slope arrays, 1024 layers of 65×65 at 4 B | 16.5 MiB each |
| Patch records, 1024 × 48 B, two staging slots plus device | 150 KiB |
| Staging ring, 32 tiles | 1.1 MiB host-visible |
| Quadtree nodes, 4096 × 64 B worst case | 256 KiB |
| Shared grid | 1221 vertices, 6912 indices, static |

Host-visible heaps count against the BAR aperture (`tools/check-bar1.py`).

## Open defects, in the order they matter

### 1. The range ratios are not a geometric series

CDLOD wants each level's range to be half the one above, because the morph band is defined as a
fraction of a range whose neighbour is assumed to be half of it. With the re-derived table the ratios
between adjacent ranges run:

| levels | 0→1 | 1→2 | 2→3 | 3→4 | 4→5 | 5→6 | 6→7 | 7→8 | 8→9 | 9→10 | 10→11 | 11→12 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| now | 1.45 | 2.65 | 2.12 | 3.23 | 2.45 | 1.86 | 2.03 | 2.01 | 1.75 | 2.35 | 2.27 | 2.01 |
| before | 1.45 | 2.65 | 3.46 | 2.30 | 2.11 | 2.38 | 1.59 | 2.76 | 2.31 | 2.31 | 2.32 | 2.29 |

One consistent measurement narrowed the spread from 1.45–3.46 to 1.45–3.23 and put most steps near 2,
but no morph band spans 1.45 and 3.23 alike. `test_morph_bands` reports 1.013x headroom at the 0→1
step — it passes, with almost nothing to spare, and any future re-measure that lowers level 0 or
raises level 1 breaks it.

The fix is still a design decision, not a bug fix: prescribe the x2 series and use the measured errors
to *place* it rather than to define each step.

Related: `patch_error` compares scalar radii using `length(d)` on a normalized direction, so sphere
curvature is excluded despite the comment. Correcting that does not fix the ratios — it reshuffles
them. The curvature term is 7 percent of the error at level 0 and under 0.1 percent from level 4 down,
so it is not worth a change on its own. The table is also sampled, not a conservative bound.

### 2. Quadrant removal leaves two parent triangles

Triangles whose three vertices all lie on the centre cross are never dropped: quadrant 1 leaves
`(16,15)–(17,16)–(16,16)` and quadrant 2 leaves `(15,16)–(16,16)–(16,17)`, 16 across all 16 masks out
of 2048 per patch. They overlap the child replacing that quadrant and can fight it in depth.

**Do not fix this with a checkerboard diagonal.** It removes all 16 and breaks the collapse invariant:
enumerated, a child at `m = 1` no longer has the parent's triangulation, so it would crack every level
boundary to remove a sliver at one patch centre.

The vertices survive because they are *shared* with the neighbouring quadrant, so no per-vertex rule can
reach them. The cheapest correct fix keeping one draw per patch: duplicate the 65 centre-cross vertices
once per adjacent quadrant (64 at two quadrants, the centre at four, so +67 of 1089, about 6 percent)
and carry an explicit quadrant index per vertex. Duplicates hold identical positions, so the morph and
the collapse invariant are untouched.

### 3. The tile cache is too small above the default bias

Children already allocated bypass the node budget, traversal is in fixed face and child order, and
there is no way to reclaim a low-priority split for a more demanding patch. So whichever patches split
first keep their splits, and a patch refused at the ceiling is refused *every* frame — the selection
settles stable and inadequate. That is what a lone coarse patch among refined neighbours looks like,
and it does not heal, which is how it differs from streaming lag.

Raising `node_budget` off `slot_count` removed it at the measured demand: `splits_blocked` went from
93 to 0, the tree settled at its true 1398 nodes (2422 at level 11), and the worst run of a patch more
than one level behind fell from 101 frames to 66. **The grandfathering and the fixed order are still
there** — they simply have headroom now, and would bite again at a ceiling.

What is left at `--lod-bias 2` is capacity, not selection: 4198 of 8515 evictions are of tiles used
within the last 60 frames, so the working set genuinely exceeds 1024 slots and tiles are thrown out
before they are done with. At bias 0 nothing warm is evicted in any flight measured — stationary,
rotating, orbiting fast or slow. Wants slot count scaled with bias, or eviction ordered by view
importance instead of plain LRU. More generation throughput does not fix a cache that is too small.

### 4. The fade does not enforce continuity at partial boundaries

A resident child can finish fading while a sibling is still missing, leaving the parent covering an
unmatched boundary. `fade = 0` on the parent's remaining quadrants removes its inherited arrival fade
but does **not** hold them still — the shader morphs by `max(distance, fade)`. Held still they would
crack against the outer neighbours instead, which is CDLOD's granularity limit: one node carries one
level of transition, not two.

The real remedy is the streaming policy, never splitting until all four children are resident, which
was measured and rejected once for its transient cost. The internal quadrant boundaries also have no
skirts, so skirts do not cover every parent/child fallback seam.

### 5. Selection uses the child's threshold to judge the parent

A level `L` node splits against `range[L + 1]`, so it stays selected past its own tolerance — about
5.2 px of table-predicted level-2 error against the stated 1.5 px near the child threshold. Thresholds
and morph bands want deriving together from the error of the geometry actually drawn; changing one
comparison alone invalidates the seam relationships.

### 6. The cap is not the cell

A sixth of the drawn set is still provably off screen, and the shape of the bound is still why,
though one step in rather than two. `patch_support` is exact for the **cap**, and the cap is not the
**cell**: a cap reaches its angular radius in every direction, while the cell only reaches it at four
corners. A whole face is 90 degrees across and its cap 54.7 in every direction, so the support gives
away 0.19 radii at level 0 and 0.07 from level 3 down, measured in `test_tiles`.

Closing it wants a bound that knows the cell's shape — four corner planes, or the support of the
cell's own convex hull rather than its cap. Neither is expensive; both want care at a cube edge, where
the cell is not convex in any single face's coordinates. Worth doing only if the sixth turns out to
cost anything, and nothing on the panel says whether it does.

### 7. Detail still stops at the depth cap, further in

`patch_level_max` bounds how fine the geometry goes, and the error metric keeps asking past it. At 11
the smallest quad is 24 urad, about 10 m on this body, and the metric wants finer from roughly 650 m
up. Below that the near patches sit at the cap while their farther neighbours are served correctly —
correct saturation, but it reads as a stall, and it is worth ruling out before chasing one.

Level 12 is the last the packing allows and needs no code change beyond the constant, but each level
multiplies the near working set against a cache already short at bias 2. The cheaper answer is the
detail octaves below tile resolution listed under *Later*: what is missing up close is small relief,
not accurate large shapes, and shader detail costs no tiles, no slots and no CPU generation.

### 8. Leftovers

- No counter says how much of the drawn set is wasted. `test_cull_waste` measures it offline by
  re-deriving visibility from each patch's samples, which is far too slow for a frame, but a cheap
  proxy -- patches whose support clears a plane by less than their own reach -- would put a number
  beside the frozen-cull view, which otherwise shows culled quadrants and kept islands alike.
- Constants duplicated between C++ and the shaders: the Everitt constant, the face tables, `tile_side`.
- `draw_body` leaves `ORBITAL_ROOT_PATCHES` and `root.patches` on the caller's `Root`; harmless only
  because the minor planet is drawn last.
- Acceptance never read off the panel: main-thread time in `prepare_terrain_tier` under 0.5 ms with 32
  jobs in flight, and one draw call per pass. Selection now walks a larger tree for a deeper cap, so
  this is the number to watch: measured in isolation it roughly doubled at bias 2.

## Later, out of scope

- Compute tile generation writing layers directly, then GPU culling with an indirect count.
- Detail octaves and small craters rising with the level, past the baked maps' resolution.
- The far tier as this cube sphere at levels 0..2, retiring the equirect maps for this body.
- Block compression of colour tiles in the compute path; mips per layer.
- Aerial perspective in the patch path if the haze is ever flown through.
