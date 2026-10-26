# Planet Forge: splitting the minor-planet terrain into libraries

Status: proposal for discussion, 2026-10-25; phases 0 to 3 are built on `evo/planet-forge`, see
"Phases" and the debt sections at the end.

## What exists and where it sits

| Piece | Today | Lines | Depends on |
| --- | --- | --- | --- |
| Crater model: `MinorPlanetTerrain` height, albedo, micro-crater detail, equirect bake | `src/scene/terrain.*` | 377 | `core/math`, `core/noise`, `core/parallel` |
| Cube sphere: Everitt warp, `PatchKey`, bounds, cell supports, tangent frame, grids | `src/scene/terrain_patch.*` | 528 | `core/math`, `core/panic` |
| Tile sampling: `generate_height_tile`, `generate_colour_tiles`, `patch_error` | same file as the cube sphere | | `MinorPlanetTerrain` |
| CDLOD runtime: `TerrainTier` selection, cache with stamps, stitching, diagnostics | `src/render/terrain_tier.*` (target `orbital_render_cpu`) | 1149 | `scene/geometry` for `Frustum`, `MinorPlanetTerrain::height_min/max` for the shell |
| GPU side: three texture arrays, 32 staging rings, worker pool calling the generator, `PatchInstance` records, two instanced draws | `src/render/renderer_terrain.cpp` | 415 | NoGraphicsAPI, `renderer_impl.hpp` |
| Shaders: `patch.slang` (vertex), `patchMaterial` in `planets/surface_airless.slang` | `shaders/` | | `scene_shared.h` constants |
| Tests | `tests/terrain_*.cpp` | 2591 | CPU only |
| Texture tools: `.otex` container, bc7enc and astcenc wrappers, Python driver | `tools/`, `src/assets/texture.*` | | not used by terrain |

Three facts shape the split:

- Everything except `renderer_terrain.cpp` and the shaders is already Vulkan-free and tested on the CPU.
  The split is mostly moving files and cutting four couplings, not a rewrite.
- The couplings to cut: the tier reads the shell range and horizon occluder from `MinorPlanetTerrain`
  constants; its `level_error` table is calibrated for seed 1007; `TierView` carries a
  `scene/geometry::Frustum`; the renderer's worker pool calls `generate_height_tile` directly, so the
  renderer knows the generator.
- Tiles are 33 (height, `r16_unorm`) and 65 (albedo `rgba8`, slopes `rg16`) texels. 65 is not a
  multiple of four, so block compression needs a tile-size change (DYNAMIC_TERRAIN.md decision 3).

## Kiln, the sibling asset library, as the reference

`D:\non-esp\libkiln` (v0.7) is the asset cook and load library. What matters for this split:

**Its rules on `std::`** (from `libkiln/CLAUDE.md`, enforced in review):

- C++23 baseline, no shims for older compilers.
- No `throw`/`try`/`catch`, no `dynamic_cast`/`typeid`. Third-party code that may throw is caught
  inside the one `.cpp` that calls it and converted to `Status`.
- No `<iostream>`, `<sstream>`, `<fstream>`, `<iomanip>`. Formatting is `kiln::format` (printf style).
- Public headers include only `<cstdint> <cstddef> <cstring> <new> <type_traits> <bit> <atomic>
  <utility> <concepts>`. Heavier standard headers live in `.cpp` files. No third-party header in `include/`.
- No `std::string`, `std::vector`, `std::unique_ptr`, `std::function`, `std::map`, `std::expected`
  anywhere. Kiln has its own `Vec<T>`, `HashMap`, `Span<T>`, `ByteSpan`, `StrView`, `Status`/`Result<T>`,
  and fixed `char[N]` buffers instead of an owning string.
- All allocation goes through an `Allocator` function-pointer struct with a `Tag`; no steady-state
  allocation in per-frame calls; arenas for per-cook temporaries.
- Plain descriptor structs with designated initializers, not builders. Function pointer plus `void* user`
  instead of owning callables or virtual interfaces. `noexcept` only on copy/move/swap and on
  consumer-facing members that cannot throw.
- What it does use from `std`: `memcpy`, `move`, `swap`, `<bit>`, `<atomic>`, `std::mutex`,
  `condition_variable`, `thread`, `chrono`, `sort`, `from_chars`, concepts and type traits.

The demo's own style (`docs/CODE_STYLE.md`) agrees on no exceptions, no streams, `std::format`-style
logging, designated initializers and panics for invariants, but it allows `std::vector`, `std::span`,
`std::string_view`, `std::optional` and `std::format` freely and is C++20.

**Its structure worth copying:** `kiln_core` (vocabulary) < `kiln_runtime` (readers, loader, thread
pool, adapter) < `kiln_cook` (importers, encoders, writers, store, CLI logic). The CLI executable is eight
lines calling `cook_cli_main` inside the library. The runtime never links the cook. The graphics
boundary is one POD `Adapter` struct of function pointers (`begin_upload` hands back mapped staging
memory and a token, `commit_upload`, `upload_status`, `destroy`, optional `bind` for bindless) and a
`Format` enum whose values equal `VkFormat` without including Vulkan.

**Its texture knowledge** (`bcn-spike`, `basis-spike`, `docs/design/bcn-encoding.md`):

- Encoders: `rgbcx` for BC1/3/4/5 (best quality at every speed, 120 KB compiled), `bc7enc` for BC7 High,
  Basis `bc7f` for BC7 Fast and Normal (24x faster, 2.4 dB loss on colour). ISPC encoders and
  Compressonator rejected (toolchain, CPU-dependent output). Everything compiled with
  `-ffp-contract=off` and no `-march` so cooked bytes are identical across compilers.
- Normals: BC5 beats BC7 (0.216 degrees mean, 1.95 p99, 173 MP/s). Heights stay `R16_UNORM`; BC4 is
  8 bit.
- Container: own KTX2 writer and zero-copy reader, Zstd per level (one frame per mip), deterministic
  output, key/value entries carry source and cook hashes. BasisLZ, UASTC and XUBC7 rejected.
- Mips: 2x2 box, sRGB averaged in linear through exact integer tables, renormalize for normals,
  alpha-coverage preservation.
- Incremental cook: every artifact is named by an XXH3-128 build key over cooker version, settings hash
  and input content hashes; a flat store with a manifest, one writer lock, `--watch`, `--gc`,
  `--explain`, `--trace` (Chrome trace), `--threads`.
- Not there: ASTC (planned v0.9), anything tile- or quadtree-aware, large rasters, pyramids, millions of
  small files (its store is flat and per-asset).

This repo's own texture tooling already vendors `bc7enc` and `astcenc` and has a hash-keyed `.otex`
cache, but TEXTURE_COMPRESSION.md and ASSETS.md disagree on whether the runtime verifies the hashes
(the code does not). Kiln is the more complete reference.

## Proposed structure

A new top-level `planet/` tree beside `src/`, built as libraries with the same CMake and layering
discipline, consumed by the demo exactly as `orbital_scene` is today.

```
planet/common    vocabulary shared by generator, runtime and renderer     depends on core only
planet/clod      CDLOD selection, tile cache, streamer, diagnostics       common
planet/gen       procedural model, tile sampling, baked and mixed providers, tile store reader   common
planet/cook      tile store writer, block encoders, cook keys, CLI main   gen, third_party encoders
tools/planet-forge.cpp   the CLI: a few lines calling planet::cook_main
src/render/planet_*.cpp  the Vulkan binding: arrays, rings, records, draws   clod, NoGraphicsAPI
shaders/planet/          patch.slang, the surface material functions
```

### planet/common

What a tile *is*, independent of who makes it and who draws it.

- `PatchKey`, `patch_neighbour`, Everitt cube mapping, `PatchBounds`, supports, tangent frame, grids.
  Moved from `terrain_patch.hpp` unchanged.
- `BodyShape { double radius; Range<float> heights; }`. The shell range stops being a
  `MinorPlanetTerrain` constant; `horizon_occluder` derives from it.
- `TileLayout`: the planes a level carries, each with a `PlaneId` (height, albedo, slope, weights,
  ...), a `Format` (enum valued like `VkFormat`, as Kiln does), a side in texels and a byte size.
  Baked coarse levels may carry albedo while fine levels carry splat weights, so the layout is per
  level range, declared by the provider, read by the renderer to create its arrays.
- `TileView` / `TileBytes`: views over one tile's planes plus its measured height range.
- `LevelTable`: per-level geometric error and the deepest level. Today's `level_error[]` and
  `descendant_relief[]` become data the provider supplies: a baked store measures them exactly
  while cooking, a procedural provider ships a calibrated table.
- Shared constants that the shaders also need live in one header generated or checked against
  `scene_shared.h` (the duplication is a listed open problem).

### planet/clod

`TerrainTier` renamed and parametrized. Behaviour unchanged through the move; the tier tests prove
it.

- Inputs: `BodyShape`, `LevelTable`, a `ClodView` with its own five planes (no `scene/geometry`).
- Outputs as now: draws with quadrant masks, edge flags and morph; requests with stamps; audit,
  pressure and health structs.
- New: `TileStreamer`. Owns the worker pool, calls the provider on workers, owns completion with
  stamp checks, and hands finished tiles to a sink. The renderer stops knowing the generator. The
  sink is the one GPU-facing seam, shaped like Kiln's adapter: `acquire(bytes, staging)` hands out
  four-byte aligned bytes and a token, `complete(request, staging, heights, ms)` returns a produced
  tile for upload, `abandon(staging)` returns the bytes of a failed one; the renderer's staging rings
  sit behind it. Today's budget rule (generate only as many tiles as there are free rings) is the
  sink saying no, which releases the slot.
- Instance-based from the start: one `Clod` per body. The showcase hard-wires one minor planet
  through `Frame`, `resource_slots.h` and `Showcase::resolve` (PLANS-2026-09-15 thread D); the
  library must not inherit that.

### planet/gen

- `TileProvider`: the pluggable source. Shape is the first decision below (virtual interface or
  Kiln-style function table). Contract: `levels()` and `layout(level)` are constant; `produce(request,
  output)` runs on a worker, is reentrant, writes every plane of the layout and the measured height
  range into the output, and returns false on failure so the slot is released.
- `ProceduralProvider`: today's `MinorPlanetTerrain` plus `generate_height_tile` and
  `generate_colour_tiles`. First implementation, and the regression anchor.
- `BakedProvider`: reads a tile store (below). I/O on the worker under a per-shard lock, no decode
  when planes are stored in GPU block formats. A level the store lacks fails `produce`.
- `MixedProvider`: one source for levels 0..B, another beyond; built as the plain level split,
  shape and level table taken from the fine source. The renderer uses it as baked up to the store's
  last level, procedural beyond. Seeding the procedural fine levels from the baked parent is not
  built: with the generator as the only baked source the parent carries nothing the function does
  not already know, so it waits for a source that differs (edited heights, phase 5 materials). When
  it comes, `produce` receives the parent tile and the CLOD requests parents first, which also
  serves IDEAS.md's "height first" entry.
- The equirect far-tier bake moves here too; a far tier later becomes levels 0..2 of the same cube
  sphere (DYNAMIC_TERRAIN "Later").

### Tile store (planet/gen reads, planet/cook writes)

Own format, not KTX2 per tile and not one file per tile (built in phase 3; the structs are
`planet/common/include/planet/store_format.hpp`, little-endian, every byte written). Per body
directory:

- `store.planet`: a fixed 616-byte header: format version, cooker version, the terrain's seed, the
  level range and face mask present, `BodyShape`, `LevelTable` and a `TileLayout` per level.
- One shard file per face and level (`f3_l07.tiles`): a 56-byte header, the tiles' bytes back to
  back in ascending packed-key order, then the index of 24-byte entries (key, bytes, offset, the
  tile's height range for the CLOD's bounds). Planes stored exactly as the GPU reads them; raw
  today, BC5 and BC7 blocks in phase 4. Zstd per tile is an option to measure later, as Kiln only
  keeps it when it saves enough.
- Deterministic bytes independent of the thread count. Re-cooks skip at shard granularity: a shard
  whose header already matches (version, cooker, seed, face, level, tile count and bytes) is left
  alone unless forced, since every tile of a shard shares the same inputs. `cook_version` is
  bumped by hand when the same seed starts producing different bytes.

Kiln's KTX2 writer stays the reference for the material library textures (the splat detail sets),
which are ordinary 2D assets and could even be cooked by Kiln itself.

### planet/cook and the planet-forge CLI

Structured as Kiln's: all logic in the library, the executable calls `planet::cook_main(argc,
argv, host)` (`tools/planet-forge.cpp`, which links `orbital_scene` only to map a showcase seed to
the minor planet's terrain seed through `CookHost`). `cook()` itself is generic over a
`TileProvider`. Options: `--out`, `--seed` or `--terrain-seed`, `--levels a..b`, `--faces`,
`--threads`, `--force`, `--check` (every tile read back and compared with the provider),
`--explain f,l,x,y`, `--quiet`; progress by level; exit codes 0 ok, 1 usage, 2 I/O or options the
library rejects, 3 mismatch or conflict. Encoders come in phase 4, vendored from the bcn-spike winners: `rgbcx` for BC4/BC5
(not yet in `third_party/`; `bc7enc_rdo` ships it next to the `bc7enc` we have), `bc7enc` High for
albedo, `bc7f` later if cook time matters. Contraction off is still to do, so a bake is
reproducible across the LAN machine and this one.

Built behind `ORBITAL_BUILD_PLANET_TOOLS` (default ON; it is CPU only and belongs in CI), unlike
today's `EXCLUDE_FROM_ALL` tools.

### Block compression of tiles

Heights stay `R16_UNORM` at 33: 6 percent of tile memory and BC4 is 8 bit. Colour and slope tiles
change side to a multiple of four with a border, 64 interior texels plus a border (68 is the first
4-aligned size with a two-texel border; the border width is a measurement item for bilinear plus
one mip level).

| Plane | Today, 65 texels | Proposed | Bytes per tile |
| --- | --- | --- | --- |
| albedo | rgba8, 16,900 | BC7 68x68 (or BC1 at half) | 4,624 (2,312) |
| slope | rg16, 16,900 | BC5 68x68 | 4,624 |
| height | r16 33x33, 2,178 | unchanged | 2,178 |
| total | 35,980 | | 11,426 |

At 2048 slots the arrays drop from 70 MiB to about 22 MiB, which is what pays for the material
refinement TERRAIN_DETAIL_BUDGET.md wants (ratio 4 at the same memory as today). IDEAS.md records
the precision risk: BC5's 8-bit endpoints bring a crater-rim block back near the old rgba8 normal
error (0.18 degrees rms). Measure with the existing tile-normal comparison before committing;
the fallback is BC5 for coarse levels and raw rg16 for the last two.

Procedural tiles generated at runtime stay raw at first (CPU encoding at 14 to 20 ms per material
pair already dominates generation). Compressing them on a compute pass is the listed "Later" item
and is unchanged by this split.

### The close-range surface: splat atlas

Not part of the extraction, but the layouts above are chosen for it:

- A material library: N detail sets (albedo, normal+height, roughness) as texture arrays, cooked
  like the belt rock sets, which already blend two sets triplanar with parallax in
  `surface_rock.slang`.
- Fine-level tiles carry a weights plane (four weights in BC7 or `rgba8`, or two indices plus two
  weights) instead of albedo; coarse levels keep baked albedo so the far look is unchanged and the
  blend fades in across a level band.
- UVs come from the face `s,t`: the Everitt warp keeps texel area within 11 percent across a face,
  so no triplanar is needed for the ground itself. Cube edges need the neighbour-face frame the
  slopes already handle.
- The forge computes weights from slope, altitude and crater class (fresh floor, ejecta, worn ground),
  which `MinorPlanetTerrain::albedo` already classifies; the procedural provider computes the same
  function per tile.

## Decisions taken (2026-10-25)

1. **`std::` regime: Kiln's public-header rules.** Public headers under `planet/*/include` carry no
   standard containers, strings or heavy headers (allowed: `<cstdint> <cstddef> <cstring> <new>
   <type_traits> <bit> <atomic> <utility> <concepts>`, plus `<span>` and `<string_view>` as the
   demo's view types until a `planet` view type replaces them). Errors are status returns, never
   exceptions. Standard containers stay inside `.cpp` files. C++20 as the demo; the rules are what
   make the libraries droppable into another host, not the language level.
2. **Provider and sink shape: virtual interfaces to start.** `TileProvider` and `TileSink` are
   abstract classes with a handful of per-tile calls. A later move to Kiln's function table is a
   mechanical change behind the same call sites, so it is not decided now.
3. **Foundation: `orbital_core`.** The libraries include `core/math`, `core/panic`, `core/log` and
   `core/parallel`. Utilities the libraries need and the demo does not grow in `planet/common`;
   an extraction later copies `core` along.
4. **Branch point: merge first.** PR #70 took the first five commits of `fix/terrain-cdlod-continuity`;
   PR #71 takes the remaining five. `evo/planet-forge` branches from `main` after #71 merges.
5. **Compressed tile granularity and the odd tile sizes.** Two separate facts:
   - Block formats encode 4x4 texel blocks, so a compressed plane's side must be a multiple of
     four. Today's colour planes are 65 (32 quads at ratio 2, plus the shared edge texel), so they
     become 64 interior texels plus a border, 68 being the first 4-aligned size with a two-texel
     border. The height plane stays 33 and raw.
   - A texture array has one format for all its layers. If the mixed provider delivers baked
     coarse tiles as BC5/BC7 and procedural fine tiles as raw `rg16`/`rgba8`, they cannot share
     one array. So the provider declares the plane formats per level range, the renderer creates
     one array set per distinct layout (coarse compressed, fine raw), and the shader picks the set
     by level. Compressing procedural tiles at runtime is not planned; raw is the default for them.

### Allocation accountability

The libraries follow Kiln's rule: every allocation the library makes goes through an allocator the
host supplies, with a tag naming the purpose (nodes, slots, tile staging, store index, cook
temporaries), and no steady-state allocation in per-frame calls.

- `planet/common` defines `Allocator { alloc, free, void* user }` plus `Tag`, Kiln's shape, with a
  default that forwards to `malloc`. Every library object takes an `Allocator*` at construction
  (null means the default). This is plumbed in phase 1 so no later signature changes.
- Internals convert over time. `std::pmr::vector` with a `memory_resource` adapter over the
  supplied allocator is the bridge: it keeps the container type out of the signature and routes the
  bytes through the host, and it is allowed because standard containers stay inside `.cpp` files.
  A `planet::Vec<T>` replaces it only if the pmr indirection shows up in a profile.
- The per-frame rule is already mostly true: the tier clears and reuses its draw, request and
  generation vectors. The remaining per-frame allocations (worker job captures, `unordered_map`
  rehash on growth) are listed and removed in phase 2.

### Regression policy

Pixel exactness against today's demo is not a goal: the demo is self-contained and the library
will render other bodies with other morphology. The refactor is gated by the CTest selection below,
plus one cheap determinism check through phases 1 to 3: the bytes of a procedural tile for a fixed
key and seed must not change while the procedural provider is only being moved. Captures stay a
review aid for the surface work in phases 4 and 5, not a gate.

## Test selection

The three terrain suites hold 2591 lines and about 35 named tests, several of which bundle checks.
The library keeps about 50 tests, split by target; demo-specific constants become inputs.

**`planet/common` (12).** From `terrain_tests.cpp`: `patch_neighbours`, `patches`,
`cube_coordinates`, `grid_mesh`, `quadrant_mask`, `height_tile_range`, and the geometric halves of
`test_tiles` split into their own tests: cell support holds every shell point without slack (versus
the sphere), descendant reach measurement, corner derivative-stencil regression. From
`terrain_tier_tests.cpp`: `nearest_cell_distance`, and the frustum and horizon plane tests once the
view type is the library's own.

**`planet/gen`, procedural provider (9).** `noise`, `detail_continuity`, `detail_regions`,
`terrain`, `regional_albedo`, `bake` (the equirect far tier), and the tile halves of `test_tiles`:
child texels coincide with parent texels, adjacent tiles share edge texels, cube-edge slopes decode
to the same normal across the face frame.

**`planet/clod` (22).** The whole default run of `terrain_tier_tests.cpp`: `motion_stability`,
`cached_reentry`, `invalidate_before_update`, `morph_streaming`, `resident_height_selection`,
`height_range_lifetime`, `resident_terrain_morph`, `morph_bands`, `mesh_spacing_bands`,
`morph_continuity`, `cache_stats_accounting`, `cull_waste`, `draw_quadrant_culling`,
`leaf_quadrant_outside_frustum`, `provenance_and_audit`, plus the inline checks at the end of its
`main` promoted to named tests: inactive far away, six roots requested first, sphere held until the
roots are resident, every drawn slot drawn once and never regenerated in the same frame, disjoint
ancestor and descendant coverage with valid split ranges. From `terrain_cache_tests.cpp`:
`invalidation_rejects_late_bounds` and `stationary_constrained_cache` at two capacities. All of
these take a synthetic or procedural provider through the `TileProvider` interface, so they also
test the seam.

**Slow suites, kept behind flags as today (5).** `--truth`: `occluder_floor`, `cull_soundness`,
`horizon_heuristic` ray-march the real surface and are the only checks that can fail the horizon
heuristic; they run against whichever provider is under test. `--sweep`: `flight_sweep`.
`--cache-sweep` and `--cruise` report rather than assert and move to tools.

**Becomes data or a cook step, not a test.** `terrain_tests --calibrate` (the `level_error` and
`descendant_relief` tables) becomes a `planet/cook` function that measures a store's tables while
baking, and a procedural provider's table is checked by a test that recomputes a few levels. The
seed-1007 floor depth asserted in `occluder_floor` becomes the provider's shell range, and the test
checks the range against the surface instead of a constant.

**Dropped.** Assertions on renderer constants (`slot_count` 1024 as the CPU baseline,
`generate_per_frame`, the panel's age-bucket layout) go with the renderer; the library tests set
their own capacities.

## Phases

Each phase ends with a full build, the CTest selection above, the tile determinism check, and a
commit.

0. Branch, this document, CMake skeleton for the four library targets, `Allocator` and `Tag` in
   `planet/common`, and the test moves.
1. **Mechanical extraction.** Move `terrain_patch` to `planet/common`, `terrain_tier` to `planet/clod`,
   `terrain` and tile sampling to `planet/gen`. Cut the four couplings: `BodyShape`, `LevelTable`,
   own planes, generator out of the renderer. Every library object takes its `Allocator*`. Tests
   move with their code and are split as listed.
2. **Provider and streamer.** Done. `TileProvider`, `TileStreamer`, the sink; `ProceduralProvider` is
   the only implementation; `renderer_terrain.cpp` shrinks to arrays, rings, records and draws. The
   per-frame allocations left are listed under debt. `prepare_terrain_tier` main-thread time is now
   a stat (panel Queue row, benchmark column `cpu_terrain_prepare_ms`): 0.95 ms median converged at
   bookmark 9, 1600x900 (134 draws, 337 resident), 0.26 ms while loading. DYNAMIC_TERRAIN's 0.5 ms
   target is missed before any of this split; where the time goes is a profiling item.
3. **Store, forge, baked and mixed providers.** Done. Tile store format, `planet-forge`,
   `BakedProvider`, `MixedProvider`, `planet_store_tests` (cook, read back, equality with the
   procedural tiles, skip, conflict, corrupt and incomplete stores). The demo stays procedural;
   `--planet-store dir` opts a run into a store when its seed, shell, level table and layouts are the
   terrain's and it holds every face from level 0. The LAN machine was unreachable, so the showcase body
   was baked here (8 cores): levels 0..5, 8,190 tiles, 281 MiB in 42 s plus 45 s for `--check`,
   0 mismatched; level 5 alone is 6,144 tiles in 33 s, about 43 ms of CPU per tile. With the store the
   close view (bookmark 9, 1600x900) has no pending tile after 46 frames instead of 250.
4. **Block compression.** 4-aligned colour tiles, BC5 slopes, BC7 albedo in the store and arrays,
   normal-error measurement against IDEAS.md's table, memory and generation numbers in
   TERRAIN_DETAIL_BUDGET.md.
5. **Splat surface.** Material library arrays, weights plane, the per-level material function in the
   shaders, forge-side classification. Reviewed at the glancing, limb and night-side views.

## Debt after phase 1 (the mechanical move)

Recorded by the step's audit:

- Public headers still carry standard containers: `PatchGrid` (two vectors), `MinorPlanetTerrain`
  (`Region`, `DetailRegion`, `TerrainMaps` vectors and arrays), `TerrainTier` (private vectors and an
  `unordered_map`, `std::vector<Step>&` in `explain`, `std::array` in the stats structs) and
  `types.hpp` pulls in all of `core/math.hpp`. The provider and sink redesign in phase 2 is where
  these turn into views over allocator-owned storage.
- The CLOD test suites borrow `view_frustum` from `orbital_render_cpu`; a plane builder in
  `planet/clod` or the tests removes the renderer from the library tests.
- `clod_tests` converges the close view three times through `converge_close`, and its 58-draw
  bound is the demo's workload rather than an input.
- `Clod` takes `BodyShape`, `LevelTable` and an `Allocator*` through `ClodConfig` (the second
  step), but its containers do not use the allocator yet, and `shell_height_min/max` remain as
  the default `BodyShape` and the bake's alpha range.

## Debt after phase 2 (provider and streamer)

- `TileStreamer` runs on the host's `WorkerPool`: `submit` wraps each job in a `std::function`
  (the closure is at the small-buffer limit, so it may allocate per tile) and `poll` hands back a
  fresh vector each frame, both through global new. A fixed job ring in `planet/clod` on the
  `Allocator` replaces it. The renderer's `tile_uploads` and `patch_copies` grow to a high-water
  mark and then stop allocating.
- The sink lives in the renderer as `Renderer::Impl::TileRings` over the 32 staging rings; by
  design, but it is the only place that knows ring reuse must wait two frames after a copy.
- The renderer sizes its arrays and staging from `ProceduralProvider::tile_layout` at compile time
  and asserts the 35,980-byte total. A provider whose layout differs per level (decision 5) needs the
  arrays built from `provider.layout(level)` at `create_terrain_tier`.
- `streamer_tests` links only the planet libraries; the two CLOD suites still borrow `view_frustum`.
- The public headers still carry the phase 1 containers; `tile.hpp` and `streamer.hpp` add only
  `<cstddef>`, `<cstdint>` and `<span>`, but `streamer.hpp` includes `clod.hpp` with its
  `unordered_map` and vectors.

## Debt after phase 3 (store and forge)

- Parent seeding and "request the parent first" are not built (see planet/gen above).
- The store reads with C stdio on narrow paths (ASCII asset names) under a per-shard mutex; a
  positional-read backend like Kiln's `IoBackend` would drop the lock, and Unicode paths need the
  platform layer.
- The cook parallelizes with the host's `parallel_for`, capped at 16 workers; its scratch goes
  through the `Allocator` but the pool does not.
- The renderer accepts a store only when every level's layout equals the procedural one and the
  range starts at 0; arrays built from `provider.layout(level)` lift both.
- Contraction is not pinned, so a bake from another compiler may differ from this machine's;
  `--check` is the test when the LAN machine bakes.
- The reader seeks with `std::fseek`, a `long`: shards past 2 GiB (level 8 raw is 1.6 GiB per face)
  fail on Windows, and the cook refuses to write them; a 64-bit positional read goes with the
  backend above.
- Baked tiles carry micro-relief at detail 1; the panel's detail setting only reaches generated
  levels while a store is in use.
- The skip rule trusts a shard's header and size, not its bytes; `--check` is what proves them.

## Open questions carried over, unchanged by the split

The 256-slot overload, the heuristic plane horizon test, independent material refinement, per-tile
mips and anisotropy, the morph smoothstep, and compute-side tile generation. The split should make
each of them a change in one library, not a reason to delay it.
