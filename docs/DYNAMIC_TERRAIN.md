# Dynamic planet terrain: the minor planet's near tier as a CDLOD cube sphere

Design and task list for the next version of the minor planet's near tier. Written for handoff: a coding
agent should be able to implement it from this document plus the code it names. It replaces the vertex-pool
near tier merged in PR #59 and refined in PR #61; it keeps that tier's quadtree, culling, cache and switch.

Depends on: `docs/BACKEND_TEXTURE_ARRAYS.md` (texture array descriptors in the GPU layer), which must land first.

## Goal

Close in, the minor planet draws as a cube sphere whose shape and colour come from per-patch tiles of
textures, not from CPU-built vertices. One shared grid mesh draws every patch. Levels meet without cracks by
vertex morphing (CDLOD). Tiles are generated on the CPU on a worker pool, lazily, and appear as they are
ready; the same tile contract is later filled by a compute shader without changing the renderer. The design
must scale to a whole-planet tier later (the far tier as the same cube sphere at coarse levels).

## What exists today (read these first)

- `src/scene/terrain.hpp`: `MinorPlanetTerrain`, height and albedo as functions of a direction, crater
  regions. The one source of the terrain; every tile samples it.
- `src/scene/terrain_patch.hpp`: `PatchKey` (face, level, x, y; packed to 32 bits), `cube_direction` (the
  tangent warp), `patch_bounds`, `generate_patch` (vertices, to be removed), `patch_indices`.
- `src/render/terrain_tier.hpp`: `TerrainTier`, the CPU quadtree: `update(TierView, frame)` returns the
  patches to draw and the ones to generate; a 1024-slot cache keyed by patch with least-recently-used
  eviction, the six root faces pinned; a node draws itself until all four visible children are resident.
  Frustum and horizon culling per node. Tested alone in `tests/terrain_tier_tests.cpp`.
- `src/render/renderer_terrain.cpp`: generation before the previous frame's GPU wait into a staging slot
  chosen by frame parity, copies into a device pool at the start of the command buffer, one indexed
  multi-draw per pass through the surface vertex shader (`draw_body`). Patches draw in the depth pre-pass,
  the scene pass and the motion pass; the shadow map keeps the sphere.
- `shaders/surface/surface.slang`: the shared surface vertex shader (reads `root.vertices[index]`, places
  `centre + rotate(position) * radius`); `shaders/scene/motion.slang` has its own vertex shader that places
  the same vertex at the previous pose. `shaders/planets/surface_airless.slang` shades the body from the
  baked equirect maps by `input.localNormal` and draws the wireframe overlay from the face's cell
  coordinates when `root.flags & ORBITAL_ROOT_WIREFRAME`.
- `shaders/scene_shared.h`: `Root` (push constants, 40 bytes, asserted in `src/render/gpu_types.hpp`),
  `Instance`, `Vertex`, `DrawArgs`, `ORBITAL_ROOT_*` flags.
- Review aids: bookmark 10 (`--bookmark 9`) just above the surface, the `Near tier` and `Wireframe` switches
  under Quality (`--near-tier`, `--wireframe`), Z for slow travel. Check shots `minor-planet-close`,
  `minor-planet-sphere`, `minor-planet-wire` in the bodies group of `tools/check.py` (1600x900, 60 frames).
- Measured: a patch's geometric error by level (the terrain at quad centres against the corner mean, radii):
  0.017, 0.0063, 0.0019, 0.00086, 0.00033, 9.7e-5 for levels 1 to 6, about threefold a level. The terrain
  test prints it.

## Decisions (agreed, do not reopen without asking)

1. Tiles live in texture arrays, one layer per tile slot: heights (`r16_unorm`), albedo (`rgba8_unorm`,
   linear light), normal+height (`rgba8_unorm`, the airless shader's layout). Not atlases.
2. Selection is by distance ranges per level, one range per level derived from that level's worst error, so
   a patch's morph state at an edge is a function of distance alone and levels meet without cracks. Per-tile
   error is not used for selection.
3. Tiles are 1:1: a 64-quad grid, 65 height texels per side, and 65 colour texels per side, all at the same
   resolution. The colour-to-height ratio is one named constant (`tile_colour_ratio = 1`), documented as the
   knob for 2 later. Layer dimensions are one named constant too (65 today); block compression later needs
   a multiple of four (68, the tile in the corner), a change to that constant and nothing else.
4. The far tier stays the sphere levels with the equirect maps, switching to the cube sphere above a
   projected radius as today. A cube-mapped far tier sharing this projection is a later step.
5. The face warp is Everitt's: `w(s) = tan(0.8687 s) / tan(0.8687)`, inverse `atan(x tan(0.8687)) / 0.8687`.
   Texel areas across a face stay within about 11 percent (the tangent warp today is 43 percent).
6. Generation stays on the CPU on a persistent worker pool. A compute generator is a later step and must not
   need a renderer change beyond where the tile bytes come from.
7. Texture compression of tiles is not in scope. Heights stay 16-bit raw in any case.

## Design

### Patches, tiles, slots

- A patch is a cell of a face's quadtree, `PatchKey`. Its bounds (`patch_bounds`) give the cap and the cell's
  angular size.
- A tile is the patch's textures: `tile_side` (65) squared heights, and the same count of albedo and
  normal+height texels. Texel (i, j) of a tile is the terrain sampled at the cell coordinates
  `s0 + i * size / 64`, `t0 + j * size / 64`, through the warp. So a child's even texels are its parent's
  texels at the same directions, and a fully morphed child edge lands on the parent's edge.
- The height texel is the height over `[height_min, height_max]` in 16 bits. The normal texel is the tangent
  normal (x east, y south in the face's s and t) with the height in alpha, from finite differences of the
  tile's own heights (no extra terrain samples). The albedo texel is `MinorPlanetTerrain::albedo` at the
  texel's direction, height and slope.
- A slot is one layer index shared by the three arrays. `TerrainTier` owns the slot cache; the renderer owns
  the arrays. Slot count 1024 (see the memory table).

### The shared grid

- One static mesh, `patch_grid_mesh()`: 65 by 65 vertices whose position holds `(x, y, skirt)`, x and y in
  0..64, skirt 0 on the grid and 1 on a ring of 4 * 65 vertices the shader drops below the surface. Indices
  cover the grid quads then the skirt quads, wound outward. Uploaded once to the static heap.
- The skirt is a fallback only: with correct morphing no crack exists between neighbours one level apart. It
  hides the transient case where a neighbour is two levels coarser while children are still loading. A
  named constant sets its drop (a fraction of the cell's angular size, as today).

### Per-patch instance records

- `PatchInstance` in `scene_shared.h`, 48 bytes: `cell` (s0, t0, size, level), `morph` (start, end in
  radii; unused; unused), `tile` (face, slot, flags, unused). One record per drawn patch, written by the CPU
  each frame into a host-visible staging slot chosen by frame parity and copied into a device buffer beside
  the tile uploads.
- `Root` gains one field, `patches` (address of this frame's records); `sizeof(Root)` becomes 48 and the
  assert in `gpu_types.hpp` follows. `ORBITAL_ROOT_PATCHES` (4) tells the vertex shaders to take the patch
  path. `ORBITAL_ROOT_WIREFRAME` stays; the level for the overlay comes from the record, not from the flags.

### The vertex path (CDLOD)

In `shaders/surface/patch.slang`, one function used by both `surface.slang` and `motion.slang`:

1. Read the grid vertex: `g = position.xy` (0..64), `skirt = position.z`.
2. Unmorphed cell coordinates `st = cell.xy + g / 64 * cell.z`, direction `d = cube_direction(face, st)`
   with the Everitt warp, local position `p = d * (1 + h(g))`, world position through the body's rotation
   and centre as today. The distance `dist = length(world) / radius` (positions are camera-relative).
3. Morph factor `m = saturate((dist - morph.start) / (morph.end - morph.start))`. Morphed grid coordinate
   `g' = g - frac(g * 0.5) * 2 * m` (odd vertices slide to the even neighbour along each axis). Recompute
   `st`, `d`, and `h` at `g'`.
4. `h(g)` is a bilinear read of the height array at layer `slot`, texel coordinates `g'` (`Load` of the four
   neighbours, or `SampleLevel` with a clamping sampler since each layer is its own texture). The value maps
   through `height_min + v * (height_max - height_min)`.
5. Skirt vertices drop by the skirt constant along `-d`.
6. Outputs as today: `worldPosition`, `worldNormal = rotate(d)`, `localNormal = d`, and a new
   `patchLevel` varying for the wireframe overlay.

Level 0 patches never morph (`morph.end` infinite). A patch drawn beyond its level's range end is fully
morphed and therefore identical to its parent's shape: the tier may draw a child beyond its range without a
crack (see selection).

### Selection by ranges

- `TerrainTier` holds `level_error[level]`, the worst measured geometric error per level in radii (task 2
  calibrates it; extrapolate deeper levels by the measured ratio). Each frame it derives
  `range[level] = level_error[level] * height_pixels / (tan_y * error_pixels)` in radii, the distance at
  which that level's error projects to the tolerance (`error_pixels`, 3 in cull_bodies' units = 1.5 px).
- Visit from the six roots: a node out of the frustum or past the horizon collapses and is skipped. A node
  whose nearest distance (camera in the body's local frame, radii, to the node's cap) is under
  `range[level + 1]` wants children; hysteresis 0.8 on the way back. Children are descended into only when
  all visible ones are resident; otherwise the node draws itself and requests them, coarse levels first, up
  to `generate_per_frame`. All four children of a split node are drawn (a child beyond its range is fully
  morphed, so no crack).
- `morph.start = 0.7 * range[level]`, `morph.end = range[level]` for the drawn patch's level.
- Roots are requested and pinned as today; the tier is active only when all six are resident.

### Generation on a worker pool

- `core/parallel.hpp` gains a persistent `WorkerPool` (N threads, N = cores minus two, at least one): a job
  queue with a mutex and condition variable, `submit(job)`, and `poll()` returning finished jobs on the
  caller's thread. Jobs are whole tiles: heights, normal, albedo for one `PatchKey`, written into a job's
  own buffer from a ring.
- The job ring: R job buffers (R = 32), each holding one tile's three planes. A buffer is free once the frame
  that copied it has completed; with one frame in flight that is the frame after the copy was recorded.
- Per frame the renderer: submits the tier's requests (a request marks the slot "pending"), polls finished
  jobs, records their copies (`copy_memory_to_texture` per plane into the slot's layer), marks the slots
  resident in the tier, and writes this frame's `PatchInstance` records. The tier's residency rule then
  gives lazy detail for free: a patch draws at its parent's level until its own tile arrives.
- Priority: the tier's request order (coarse first, then largest on screen). A request for a patch that
  collapses before its job finishes is not cancelled; the job completes into its slot and the cache keeps it.

### Fragment shading

- The airless shader keeps the equirect path for the sphere. In the patch path (`ORBITAL_ROOT_PATCHES`) it
  samples the albedo and normal arrays at the patch's slot by the cell's uv (from `cube_coordinates` of the
  local normal, as the wireframe does today, or from a varying), builds the tangent frame from the face's s
  and t directions, and runs the same lighting. The height trace for crater-wall shadows reads the tile's
  height plane in the same layout as the equirect trace; if that proves fiddly, the trace can be skipped in
  the patch path for the first cut and noted.
- The wireframe overlay reads the level from the `patchLevel` varying.

### Memory and budgets

| Item | Size |
|---|---|
| Height array, 1024 layers of 65x65 r16 | 8.6 MiB |
| Albedo and normal arrays, 1024 layers of 65x65 rgba8 each | 17 MiB each |
| Patch records, 1024 x 48 B, two staging slots plus device | 150 KiB |
| Job ring, 32 x (65x65 x (2 + 4 + 4) B) | 1.4 MiB host-visible |
| Shared grid | 4485 vertices, 26112 indices, static |

The host-visible heaps count against the BAR aperture (`tools/check-bar1.py`); all of the above is small.

## Acceptance criteria

1. From bookmark 10 (`--bookmark 9`) with the tier on, the terrain shows no cracks or holes at any level
   boundary with the wireframe overlay on, still or while flying with Z slow travel, including across cube
   face boundaries. Skirts may show only during a load transient.
2. Turning the near tier off and on at the same view shows no visible change in the shading of the surface
   (the tiles sample the same function the equirect maps were baked from). `tools/check.py --group bodies`:
   the `minor-planet` (far) shot unchanged; the three near shots re-accepted with the new geometry.
3. The scene pass draws all patches of a body with one draw call per pass (`draw_calls` in the panel).
4. `terrain_tier_tests`: with a synthetic per-level error table, the tree converges, every drawn patch's
   distance is under its level's range times the hysteresis, no drawn patch has a drawn ancestor, and a
   requested slot is never drawn the same frame.
5. `terrain_tests`: for a parent and each of its four children, the child's even texels equal the parent's
   texels in the same directions within one 16-bit code; neighbours at the same level share their edge
   exactly; `cube_coordinates(cube_direction(f, s, t))` round-trips within 1e-9; the height tile of a patch
   generated twice is identical.
6. Generation runs on the worker pool: the main thread's time in `prepare_terrain_tier` is under 0.5 ms
   with 32 jobs in flight; tiles appear progressively (the panel's "patches drawn, cached" line rises over
   frames after a cut).
7. The near tier off (`--near-tier 0`) and every other body are unchanged: `tools/check.py --all` within
   tolerance except the three near shots.
8. Docs: `docs/ARCHITECTURE.md`'s minor planet section rewritten for this design; a `docs/DECISIONS.md` entry
   with the measurements (per-level errors, patch counts, generation times, draw calls, memory).

## Status after the first implementation (PR #64, 2026-09-18)

Tasks 1 to 8 are implemented; the near tier draws from tile arrays with the worker pool. A review of that
implementation found the following. The first group was fixed before the merge; the rest is open and is
what acceptance 1, 2 and 4 to 6 still need.

Fixed in the PR after review:

- The patch draw set `instanceIndex` to `base + patchId`, so every patch but the first shaded with another
  body's instance (its rotation, kind and self-shadow test).
- Tile copies started at offsets that were not multiples of the texel size (an entry was 42,250 bytes):
  `VUID-vkCmdCopyBufferToImage-dstImage-07975` on every upload. Planes and entries are now 4-byte aligned.
- The tier went active when the roots had a slot, before their tiles arrived ("Near tier on at frame 24,
  0 patches resident"): the sphere stopped and nothing drew. Activation now needs resident roots.
- Requests the renderer could not submit (no free ring entry) kept their pending slot forever and their
  subtree never refined. `update` takes a budget, the free ring count.
- Eviction could take a pending slot; the old job then marked the new key resident with the old tile.
  Pending slots are not evicted, and `mark_resident(slot, key)` checks the key.
- The motion pass placed the previous position with the unmorphed height and no skirt drop, so morphing
  patches carried false motion vectors into TAA.
- The wireframe was an analytic grid in the face's cell coordinates, not the mesh. It now draws the
  triangles from a barycentric each corner carries (an unindexed copy of the grid, drawn only with the
  overlay on), with the patch borders from the tile coordinate; the sphere path has no overlay.
- A split node drew all four children, the far ones fully morphed to its shape, as designed. That is
  not crack-free: the far children stand at this node's level unmorphed, while an unsplit neighbour at
  the same level, farther than 0.7 of its range there, is already morphing toward the level above, up to a
  full level apart in geometry and shading along their shared border. On a face edge (a border at every
  level) with the camera over the other face it read as one whole face shaded flatter. Now, as in the
  original CDLOD, a child is drawn only within its own range and when resident; the node draws every other
  visible quadrant itself (`Draw::quadrants`, the vertex shader dropping the rest of its grid), so nothing
  is drawn beyond its level's range and neighbours at one level morph alike. This also makes residency
  per quadrant: a patch refines as each child's tile arrives. The tier test checks that a drawn ancestor
  covers no drawn descendant's quadrant.
- `init` bound the 1x1 placeholder to every array slot after `create_terrain_tier` had bound the tile
  arrays, so the shaders sampled the placeholder: every height read as `height_min` (a smooth sphere at
  0.95 radii) and the albedo as black. The placeholder now goes in first. Found because the near tier
  only activates above 960x540 at bookmark 10, so every low-resolution capture had shown the sphere.

Open, in the order they matter (the numbering is kept from the review):

1. Fixed after review: selection by the patch centre. `TerrainTier::nearest_distance` is the closest the
   camera can be to any point of the patch's cap over the shell between the terrain's lowest and highest
   heights, and the tier splits on it; the vertex's own distance is never less, so an unsplit neighbour's
   edge lies beyond the child's range and the child is fully morphed there. The tier test checks the
   drawn children against it.
2. Fixed with 1: the shell bound covers the displaced vertex.
3. **The tile tangent frame is only nearly orthonormal.** The slopes are per unit of arc (the chord between
   the two neighbours) and the shader projects the face's S and T axes onto the sphere's tangent plane, but
   off the face centre those projections are not orthogonal to each other, so the shading normal tilts
   slightly. Acceptance 2 compares the tile path with the equirect path: mean 18.2 against 19.2 and
   contrast 17.7 against 19.2 (standard deviation of the frame) at bookmark 10, 1600x900, the tiles
   resolving finer relief than the 2048-texel map.
4. Fixed after review: shading seams. Same-level seams came from one-sided edge differences
   (`generate_colour_tiles` now samples a one-texel ring; `terrain_tests` checks neighbours' edge texels
   match byte for byte). Level-boundary seams came from the coarser tile's coarser-scale normal:
   `patchMaterial` now blends the albedo and normal toward the parent tile's through the morph zone
   (the parent's slot and the child's quadrant ride in `PatchInstance.tile`), so a fully morphed child
   shades as its parent, which its unsplit neighbour matches at the shared edge.
5. **`SKIRT_DROP` is a fixed 0.002 radii**, not a fraction of the cell size. With 1 fixed it only covers
   the load transient; at levels 0 and 1 that transient's gap can exceed it.
6. Mostly fixed: the patch path traces the crater-wall shadow through the tile's own heights
   (`tileShadow`, clamped at the tile's edge, so a wall within six texels of an edge loses part of its
   shadow), fades the tilt and the trace by `root.detail`, and passes the albedo's slope as `1 - n.z` as
   the bake does (the raw gradient had brightened every slope). The regolith grain is still missing.
7. **Acceptance 6 is not measured** (main-thread time under 0.5 ms with 32 jobs in flight); the panel now
   shows the queue, the ring and the last tile's generation time, so it can be. Acceptance 3 (one draw a
   pass) was not read off the panel.
8. **Dead code from tasks 3 and 6**: `generate_patch`, `patch_indices`, `patch_vertex_count`, `patch_quads`
   remain in `terrain_patch.hpp` for the old tests only. Stale comments: `slot_count`'s "11 MiB of
   vertices", "staging-to-pool copies" in `renderer_impl.hpp`.
9. **The calibration runs inside `terrain_tests`** (13.9 s; ctest went from 10 s to 25 s). Put it behind a
   flag. `patch_error` samples each corner once per quad and its comment claims the sphere's curvature is
   included; it compares radii only.
10. **Ring bookkeeping**: `ring_used_frame = frame + 100` marks an entry in flight and 0 marks it free.
    With the near tier off the poll is skipped, results wait unpolled while their markers expire, and the
    entries can be reused under them. An explicit state per entry is safer. `WorkerPool`'s destructor runs
    every queued job before joining (up to 32 tiles at shutdown).
11. **`draw_body` leaves `ORBITAL_ROOT_PATCHES` and `root.patches` on the caller's `Root`**; the depth
    pre-pass and motion pass do not reset flags between bodies. Harmless only because the minor planet is
    the last body.
12. **Constants duplicated between C++ and the shaders**: the Everitt constant, the face tables (in
    `patch.slang` and `terrain_patch.cpp`), `tile_side`. The slot count and the height range are now
    asserted equal in `renderer_impl.hpp`.
13. **The effective tolerance is one level looser than `error_pixels` reads**: a node splits at
    `range[level + 1]`, as designed, so a level draws until its error is about 2.5 times the tolerance
    (about 3.75 px, not 1.5). Keep in mind when judging quality.

## Tasks

1. **Backend texture arrays.** Per `docs/BACKEND_TEXTURE_ARRAYS.md`. Blocks everything below.
2. **Calibrate level errors.** A small tool or test mode that samples 64 random patches per level 0..8 and
   prints the maximum `patch_error` per level; put the table in `terrain_tier.hpp` with the seed and date
   in a comment, extrapolating levels 9..12 by the measured ratio. Keep `patch_error` in the scene layer.
3. **Scene layer.** Replace the tangent warp by Everitt's in `cube_direction`, add `cube_coordinates`.
   Replace `generate_patch` by `generate_height_tile` and `generate_colour_tiles` (the latter deriving the
   normal from the height plane and evaluating albedo per texel), both writing into caller-provided spans.
   Add `patch_grid_mesh`. Remove `patch_indices`, `patch_vertex_count`. Update `tests/terrain_tests.cpp` to
   the criteria in acceptance 5. Note `tile_colour_ratio`.
4. **Shared ABI.** `PatchInstance`, `Root.patches`, `ORBITAL_ROOT_PATCHES`, three `TEX_TERRAIN_*` array
   slots in `resource_slots.h` (the array table, not the 2D one), asserts in `gpu_types.hpp`.
5. **Tier.** Replace the error test in `TerrainTier::visit` by the range test; `TierView` gains what the
   ranges need (height pixels and tan already there). `Draw` carries key and slot; the renderer derives the
   record. Drop `Generation.error`. Update `tests/terrain_tier_tests.cpp` to acceptance 4.
6. **Renderer, synchronous first.** Create the three arrays and bind them; upload the grid; write patch
   records per frame; the vertex path in `patch.slang` used by `surface.slang` and `motion.slang`; one
   instanced draw per pass (`draw_indexed` with the patch count as instances; indirect later). Generate at
   most two tiles a frame on the main thread for this step. Remove the vertex pool, the multi-draw args and
   their heaps. Verify with the wireframe at bookmark 10.
7. **Fragment path.** Tile sampling in `surface_airless.slang`, the tangent frame from the face, the
   wireframe level from the varying. Verify acceptance 2.
8. **Worker pool and job ring.** `WorkerPool` in `core/parallel.hpp` with a test in `tests/core_tests.cpp`;
   the job ring in the renderer; lazy residency. Verify acceptance 6.
9. **Checks and docs.** Re-accept the near shots on main after the merge; acceptance 7 and 8.

Order matters only as written: 1 before all, 2 and 3 in parallel, 4 before 5 and 6, 6 before 7 and 8.

## Later (not in scope, keep the door open)

- Compute tile generation writing layers directly (port `core/noise` and the crater loop to Slang; the CPU
  keeps the tree). Then GPU culling with an indirect count.
- Detail octaves and small craters rising with the level, past the baked maps' resolution.
- The far tier as this cube sphere at levels 0..2, retiring the equirect maps for this body.
- Block compression of colour tiles in the compute path; mips per layer.
- Aerial perspective in the patch path if the haze is ever flown through.
