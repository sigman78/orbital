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

## Design

### Patches, tiles, slots

A patch is a cell of a face's quadtree (`PatchKey`: face, level, x, y, packed to 32 bits).
`patch_bounds` gives its cap, angular size and bounding radius.

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

`level_error[level]` is the worst measured geometric error per level in radii, calibrated over 64
random patches a level. Each frame the tier derives
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
Only drawing and gating use the tight range, and frustum culling keeps `bound_radius` off the global
shell and the skirt drop.

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
fraction of a range whose neighbour is assumed to be half of it. Measured against the drawn triangles,
the ratios between adjacent ranges run:

| levels | 0→1 | 1→2 | 2→3 | 3→4 | 4→5 | 5→6 | 6→7 | 7→8 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| from the table | 1.45 | 2.65 | 3.46 | 2.30 | 2.11 | 2.38 | 1.59 | 2.76 |
| from the true geometric error | 1.62 | 2.41 | 1.81 | 3.52 | 2.10 | 3.04 | 1.24 | 4.28 |

No morph band spans steps between 1.24× and 4.28× consistently. This is the strongest candidate for the
hard level edge seen from the ground, and it is the one open item that matches the reported symptom.

The fix is a design decision, not a bug fix: prescribe the ×2 series and use the measured errors to
*place* it rather than to define each step.

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
93 to 0, the tree settled at its true 1398 nodes, and the worst run of a patch more than one level
behind fell from 101 frames to 66. **The grandfathering and the fixed order are still
there** — they simply have headroom now, and would bite again at a ceiling.

What is left at `--lod-bias 2` is capacity, not selection: 3396 of 8515 evictions are of tiles used
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

### 6. Detail stops at the depth cap

`patch_level_max` bounds how fine the geometry goes, and the error metric keeps asking past it. At 10
the smallest quad is 48 urad, about 20 m on this body, and the metric wants finer from roughly 1.3 km
up. Below that the near patches sit at the cap while their farther neighbours are served correctly —
correct saturation, but it reads as a stall, and it is worth ruling out before chasing one.

### 7. Leftovers

- `calibrate_level_errors` runs inside `terrain_tests` (13.9 s of a 25 s ctest); it wants a flag.
- Constants duplicated between C++ and the shaders: the Everitt constant, the face tables, `tile_side`.
- `draw_body` leaves `ORBITAL_ROOT_PATCHES` and `root.patches` on the caller's `Root`; harmless only
  because the minor planet is drawn last.
- Acceptance never read off the panel: main-thread time in `prepare_terrain_tier` under 0.5 ms with 32
  jobs in flight, and one draw call per pass.

## Later, out of scope

- Compute tile generation writing layers directly, then GPU culling with an indirect count.
- Detail octaves and small craters rising with the level, past the baked maps' resolution.
- The far tier as this cube sphere at levels 0..2, retiring the equirect maps for this body.
- Block compression of colour tiles in the compute path; mips per layer.
- Aerial perspective in the patch path if the haze is ever flown through.
