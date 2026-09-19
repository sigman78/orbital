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
2. Selection is by distance ranges per level, derived from the parent level's sampled triangle error, so a patch's
   morph state at an edge is a function of distance alone. Per-tile error is not used for selection.
3. The colour planes are finer than the height plane by `tile_colour_ratio` (2): 32 quads of geometry,
   65 colour texels a side. Both sides are named constants; block compression later wants a multiple
   of four.
4. The far tier stays the sphere levels with the equirect maps, switching above a projected radius.
5. The face warp is Everitt's, `w(s) = tan(0.8687 s) / tan(0.8687)`. Texel areas across a face stay
   within about 11 percent, against 43 for a plain tangent warp.
6. Generation stays on the CPU on a persistent worker pool.
7. Texture compression of tiles is out of scope. Heights stay 16-bit raw in any case.
8. The tree size (`node_budget`) is not the cache size. Nodes are CPU bookkeeping; drawn patches and
   their material ancestors need slots, so the two have no reason to share a number. They did once, and
   capping the tree at `slot_count - 64` starved splits long before memory justified it.
9. `patch_level_max` is 11: cells of 90 degrees over 2048, quads of 24 urad, about 10 m on this body.
   Every field carrying a cell index is guarded by a `static_assert` in `terrain_patch.hpp`, because
   overflowing one truncates silently. 12 is the ceiling `PatchKey::packed`'s 12-bit x and y allow.

## Design

### Patches, tiles, slots

A patch is a cell of a face's quadtree (`PatchKey`: face, level, x, y, packed to 32 bits).
`patch_bounds` gives its cap centre, angular radius and angular size, that radius' cosine and sine for
`patch_support`, and the cell itself -- four corner directions and the four plane normals bounding it
-- for `patch_cell_support`. Both culling tests are built on those two.

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

One static mesh, `patch_grid()`: four quadrant grids of 17 by 17 vertices, each owning its
centre-cross boundary vertices, plus eight half-edge skirt strips of 17 vertices. That is 1292
vertices and 6912 indices. Vertices carry `(x, y, skirt, quadrant)`, so removing a quadrant removes
all its triangles. This fix is already present; the former shared-vertex mask left two triangles.
The uniform `(x,y)–(x+1,y+1)` diagonal preserves the child's collapse into the parent triangulation.

Skirts remain an outer-edge fallback. Streaming continuity is enforced by selection and common
morph bands; skirts do not cover the internal boundaries of a partially drawn parent.

### Per-patch records

`PatchInstance` in `scene_shared.h`, 48 bytes: `cell` (s0, t0, size, level), `morph` (start and end
distance in radii, the local interior fade and the coarse-edge mask), `tile` (face, slot, the
parent's slot or the slot count for none, then the child's quadrant, which sides lie on a cube edge,
which grid quadrants to draw, and a 16-bit mask of edges to keep unmorphed). One record per drawn patch, written each frame into a host-visible
slot chosen by frame parity and copied beside the tile uploads. `ORBITAL_ROOT_PATCHES` puts the vertex
shaders on the patch path.

### The vertex path

`shaders/surface/patch.slang`, shared by `surface.slang` and `motion.slang`:

1. `g = position.xy` (0..32), `skirt = position.z`.
2. Interior `m = max(saturate((dist - morph.x) / (morph.y - morph.x)), morph.z)`; roots use zero.
   Quadrant edges ignore the arrival fade. At a 2:1 boundary the fine edge uses one and the coarse
   edge zero; same-face peers use the common distance morph, and cube-face peers pin their edge.
   The CPU supplies four side bits per quadrant for each kind of constraint.
3. `g' = g - frac(g * 0.5) * 2 * m` — odd vertices slide to the even neighbour along each axis.
4. Height is a bilinear read of the height array at layer `slot`, coordinates `g'`.
5. A quadrant the record does not list is dropped.

The motion pass places the previous position through the same morph and skirt drop, or morphing patches
carry false motion vectors into TAA.

### Selection

`level_error[level]` estimates error in radii against the actual 3D triangles. `patch_error` samples
the diagonal midpoint and both triangle centroids, including curvature. `terrain_tests --calibrate`
samples 64 patches per level (1024 at levels 9–12), then hill-climbs around the worst patch. This is
not a certified bound over all terrain, seeds or intermediate morphs.

With `projection = height_pixels / (tan_y * error_pixels)`, the requested handover distance is
`range[L + 1] = level_error[L] * projection * exp2(lod_bias)`. A parent therefore asks for children
when its own estimated error reaches the configured budget. The default is now 5 screen pixels,
chosen to retain the original workload: the convergence view draws 56 patches, versus 52 before
these fixes and 308 with the initial 1.5-pixel correction. The triangle calibration and parent-error
model remain; `--lod-bias 1.737` requests approximately the stricter 1.5-pixel budget explicitly.
Cache capacity and the depth cap can still prevent the requested accuracy.

The morph band for level L ends at `range[L]` and starts at
`range[L + 1] + 0.4 * (range[L] - range[L + 1])`. Thus a parent's morph starts beyond its child
handover, without assuming an exact 2:1 ratio. The current calibration has at least 1.232x headroom
through the supported levels. Geometry and material use the same band.

Selection uses two walks: discover desired tiles, then select resident tiles with local parent
fallback. Missing tiles never change another patch's distance ranges. All roots must be resident
before the terrain tier takes over.

The draw list is balanced locally to at most one level of difference across an edge. Where a late
tile would create a larger gap, only the adjacent fine subtree is drawn from a resident ancestor.
The final coverage map supplies per-quadrant edge constraints, including internal parent-mask
boundaries and cube edges. Same-face peers retain the common distance morph; independent arrival
weights affect patch interiors only. New draws relax their interior fade over 15 updates. Boundary
vertices can still change when local topology changes; this is a local transition, not a shared
quality scale affecting the entire screen.

The first implementation reduced every range when any needed tile was missing. It passed spatial
seam tests but made small view changes promote/demote the whole visible planet. That policy was
removed after interactive feedback. The pressure panel now reports local balancing work. A delayed
upload is covered locally, and the existing timeout reclaims permanently missing jobs.

Invalidation initializes the cache before resetting it, so non-default startup detail cannot insert
the free-slot list twice.

Distances and culling use the tile height interval padded for the entire descendant subtree, or the
nearest resident ancestor's interval until the tile arrives. The sampled interval includes height
quantization padding. This prevents a loose global shell from selecting fully collapsed extra levels.

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

A cap is not a ball, and a cell is not its cap. A sphere drawn around a cap also contains the space
behind it, which is most of its volume once the cap is wide; close to the ground that space alone
reaches every frustum plane, since they all pass through the camera. And a cap reaches its angular
radius in every direction where the cell is a factor of root two closer along its edges, which at
level 2 is 0.085 radii of slack, 36 km on this body -- enough to keep a patch that far outside the
frustum, and what detached islands in a frozen-cull view were made of.

So `patch_cell_support` bounds the cell itself, which is exact: the cell is a convex cone of four
planes through the origin, since its `s = s0` boundary lies in the plane of the face axis offset by
the warp and the face's t axis. The maximum over a convex cone is the normal itself where it points
inside, and otherwise on the boundary, at a corner or on one edge arc -- four corner dots and four
arc projections, against the cap's one dot. The cap still runs first, since it is conservative and
settles any plane it already rejects.

| bound | drawn, looking down at 0.05 and 0.15 radii | of those provably off screen | selection |
|---|---|---|---|
| sphere | 44, 47 | 16, 18 | 0.070, 0.056 ms |
| cap | 34, 34 | 6, 5 | 0.047, 0.035 ms |
| cell | 28, 29 | **0, 0** | 0.098, 0.080 ms |

The cell bound costs about half again what the sphere did and draws a third fewer patches for it.

The occluder for the horizon is the shell's floor, never the reference surface: ground below that
surface sets the horizon further out, so assuming the surface itself would hide terrain that can in
fact be seen over it. The horizon takes the cell support too, which at a steep pitch is worth four
patches of thirty-four.

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
| Shared grid | 1292 vertices, 6912 indices, static |

Host-visible heaps count against the BAR aperture (`tools/check-bar1.py`).

## Fixes verified on 2026-09-19

Startup invalidation survived 5595 slot assignments without duplicate live ownership. Delayed and
out-of-order streaming checked 15736 moving boundary vertices with zero violations; settled checks
covered another 24544. Cube-neighbour mapping is reciprocal and has matching physical edge midpoints.
A small-step flight with delayed uploads replaced at most 7.1% of drawn patches in a frame, with
40 frames awaiting tiles. The default convergence workload is bounded by a regression test.
The quadrant-ownership mesh fix predates this work.

## Remaining limits

### 1. The tile cache is too small above the default bias

The fixed 1024-slot cache can be smaller than the requested working set, particularly at positive
LOD bias. Unavailable detail is covered by local resident ancestors, and neighbouring subtrees may
be coarsened to preserve 2:1 edge stitching. More generation throughput cannot supply an oversized
resident set.

The tree still has a separate 4096-node budget and fixed traversal order; existing splits retain
priority. Reclaiming low-priority splits or scaling cache capacity remains future work.

### 2. What the cull still keeps, and nothing says whether it costs

The five default test views currently draw 13–43 patches with none provably off screen. What is left is the tail of the same thing: a plane rejects on the
shell's extremes, and a patch can have a corner inside the frustum while almost all of it is outside.
Closing that wants per-quadrant culling, which is a change to what a draw is, not to the bound.

There is also no counter for it. `test_cull_waste` measures waste offline by re-deriving visibility
from each patch's samples, far too slow for a frame, and the frozen-cull view shows culled quadrants
and kept islands alike with no number beside them. A cheap proxy on the panel would say whether the
remaining tail is worth anything at all.

### 3. Detail still stops at the depth cap, further in

`patch_level_max` bounds how fine the geometry goes, and the error metric keeps asking past it. At 11
the smallest quad is 24 urad, about 10 m on this body, and the calibrated metric continues asking for finer detail close to the ground. Below that the near patches sit at the cap while their farther neighbours are served correctly —
correct saturation, but it reads as a stall, and it is worth ruling out before chasing one.

Level 12 is the last the packing allows and needs no code change beyond the constant, but each level
multiplies the near working set against a cache already short at bias 2. The cheaper answer is the
detail octaves below tile resolution listed under *Later*: what is missing up close is small relief,
not accurate large shapes, and shader detail costs no tiles, no slots and no CPU generation.

### 4. Leftovers

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
