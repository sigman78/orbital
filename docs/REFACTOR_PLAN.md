# Scene, renderer and asset refactor plan

Prepared 2026-09-13 after the [code quality review](CODE_QUALITY_REVIEW.md). Renderer correctness, validation and compact culling fixes are committed through `d1e1e0b`. Stage 1 is committed as `f213b91`: explicit RGBA ownership, constexpr checked image views, PNG/font boundary migration, leaner headers and shared 16K image/texture limits. Stage 2 image resource ownership is committed as `2df853d`. Stage 3 app/render separation is committed as `b6b1c22`. Stage 4 showcase binding is committed as `bc93b94`. Stage 5 frame-calculation extraction is committed as `c9cba72`; app UI/CLI cleanup is committed as `073b2d2`. Stage 6 measurement is complete; the storage/evaluation rewrites are deferred based on the measurements.

## Immediate defects addressed

| Area | Change | Expected behavior |
| --- | --- | --- |
| App/frame boundary | Copy complete dust settings through a tested frame-input adapter | Density, brightness, far brightness, saturation and tint now reach rendering |
| Bookmarks | Share selection between CLI, keys and panel | Orbit follows the selected bookmark's body; invalid indices are rejected |
| Resource lifecycle | Destroy the final galaxy target; reconcile extent and galaxy divisor together | Final target is released; simultaneous size/divisor changes rebuild it once |
| Scene validity | Check finite rotation fields and belt thickness; reject negative rotation periods | Invalid scenes are rejected; period zero retains stationary rotation |
| Image/texture boundaries | Validate RGBA storage, mip extents and payloads before kernels/upload/cache writing; bound PNG encoder integer dimensions | Malformed public aggregates cannot silently reach these consumers; partial GPU mip chains remain supported |
| UI contract | Enforce documented one-time font upload and 16-bit ImGui indices | Unsupported use fails at its boundary |
| File output | Check directory creation and explicit file close | Buffered write completion errors propagate to callers |

These fixes enforce existing contracts. They do not yet replace mutable image aggregates, raw GPU ownership, the font pointer API or the showcase scene assumptions.

## Ordered stages

Each numbered stage is a review milestone. Keep commits scoped to one concern, with mechanical call-site migrations included where needed. Separate storage or behavior changes from interface migrations.

### 1. Make image interpretation explicit

Rename the material-processing owner to `Rgba8Image`. Introduce a checked, borrowed `ImageView` carrying extent, pixel layout, row stride and byte span. Layout determines channel count; do not store contradictory independent format/channel fields. Use `Gray8`, `GrayAlpha8`, `Rgb8` and `Rgba8` where supported by PNG output. Keep color encoding and alpha interpretation explicit where a consumer needs them.

Change `save_png` to consume the view, and migrate screenshots, font/HUD data and image owners through explicit adapters. Define whether trailing row padding is permitted and validate the last addressed byte with overflow-safe arithmetic. Keep existing RGBA kernels specialized and contiguous; validate at their boundary, not inside each pixel operation.

Acceptance: PNG round trips for supported layouts, padded rows and malformed spans; existing material/kernel tests and byte-identical generated assets. No mip allocation change in this stage.

Stage 1 additions: `ImageView` constructors, factory, row access and metadata accessors support constant evaluation; `Rgba8Image` accessors/validity do too. `core/extent.hpp` and basic `core/panic.hpp` separate dimension/invariant primitives from math and formatted `core/panic_if.hpp`. PNG declarations move to `image_io.hpp`; the public renderer only forward-declares the view. A shared 16,384-per-axis asset policy replaces unrelated dimension arithmetic in image decoding and texture validation. Stride/span and encoder integer checks remain. Tests include constexpr padded views, both size boundaries, and valid oversized PNG headers rejected before allocation.

### 2. Consolidate renderer resource ownership

Introduce small move-only owners for GPU images and cohesive target groups. Specify destruction order and the requirement that GPU work has completed before resources are released. Descriptor/attachment aliases remain visibly borrowed. Reconcile requested extent and quality settings in one place, with one wait and one rebuild per affected group.

Begin with frame targets, then migrate material/font ownership in a separate commit. Preserve the current bounded staging upload and explicit pass order. Replacing the raw resource inventory is the goal; a generic render graph is unnecessary for this fixed pipeline.

Acceptance: startup, resize, quality/divisor change, fullscreen, minimize/restore and shutdown checks; GPU validation with no lifetime errors; fixed-time capture comparisons. Measure resize stalls separately from steady-state frame cost.

### 3. Restore the app/render boundary

Replace the renderer's dependency on `app::Camera` with a render-facing camera snapshot containing only values rendering consumes. Keep navigation, bookmarks and orbit behavior in app. Have app construct and supply the HUD image, removing renderer includes of `app/camera.hpp` and `app/hud.hpp`.

The frame-input adapter and shared bookmark transition introduced by the defect fixes are the migration seam. Keep settings grouped by effect rather than introducing a catch-all settings object. Correct CMake ownership too: camera implementation should no longer be compiled into the scene library merely to satisfy tests.

Acceptance: no renderer-to-app include edges; standalone scene tests do not link app implementation; camera/frame mapping tests and unchanged fixed-time captures.

### 4. Express the showcase scene contract

Separate generic `SystemDescription` validity from the renderer's supported showcase configuration. Resolve required body roles and belt parent by ID once, validate those bindings, and use them consistently for state lookup and material assignment. Reject unsupported configurations explicitly rather than silently using the first belt or associating every belt with the gas giant.

Initial scope remains the current showcase and one belt. Supporting arbitrary systems or multiple belts is a separate feature. Audit description fields that rendering ignores (`scale_policy`, atmosphere scale and star properties): either connect their semantics deliberately or document/remove misleading promises in separate behavior changes. Preserve fields that already have consumers, such as material seeds.

Acceptance: missing/duplicate required roles, reordered state IDs, wrong belt parent and unsupported multiple belts have defined outcomes; default showcase pixels remain unchanged. Generic scene validation must not acquire showcase-only restrictions.

### 5. Separate frame calculations from GPU packing

Split `build_frame` into camera/history preparation, belt geometry/LOD, lighting and final shader ABI packing. Use small typed results with explicit inputs. Keep float4 component conventions inside the packing layer and retain layout assertions. Expose only the state each calculation needs instead of giving every helper unrestricted `Impl` access.

Extract UI sections by effect and share semantic actions with keyboard/startup handlers. Group repetitive CLI parsing where useful, preserving option-specific validation and errors. Keep `Renderer::draw` readable as the ordered pass sequence. Complexity counts identify mixed responsibilities; reducing guard counts or creating wrappers is not an acceptance goal.

Acceptance: targeted pure calculation tests, unchanged shader layouts, full rendering comparisons and per-pass performance checks. Do not combine floating-point algebra changes with this decomposition.

### 6. Evaluate storage and scene evaluation optimizations

After image views stabilize, measure allocations and peak memory during cache load/material preparation. If worthwhile, make texture storage one owning byte slab plus per-mip offset/extent/size records; derive spans on access so moves cannot leave internal views dangling. Keep compressed block layout distinct from pixel layout. `texture_from_images` already moves per-mip buffers; do not justify a rewrite by claiming it copies them.

Likewise, consider a validated immutable scene plus evaluation into reusable output if profiling warrants it. Preserve validation at mutation/load boundaries. The small bounded linear ID search does not need an ECS or hash-table infrastructure.

Acceptance: measured allocation/peak-memory improvement, move/lifetime and cache-corruption tests, unchanged cache compatibility and pixels. Defer this stage if measurements show little benefit.

## Validation and comparison policy

For each implementation stage: full release build, full CTest, GCC CPU portability check when public headers change, and fixed-time Earth/belt/tour comparisons against the preceding stage. Explicitly describe intentional pixel changes. Keep captures and arguments with the experiment; repeat the unchanged executable if GPU ordering introduces small differences.

For renderer changes, run the [performance suite](PERFORMANCE.md) in windowed and fullscreen modes with per-pass timings, comparing both the preceding stage and the retained baseline. Use compatible assets/settings/device and keep timing runs free of competing GPU workloads. Do not replace the historical baseline as stages accumulate.

The immediate fixes have no shader or per-pixel algorithm changes. Texture checks run during construction/upload; additional scene checks join existing per-frame validation. This predicts negligible steady-state cost, but is not a measured performance result.

## Verification of immediate fixes

- MSVC release build and all 10 CTest suites passed.
- GCC/MinGW CPU build and all 10 CTest suites passed.
- Window smoke passed: resize, minimize/restore, fullscreen and input transitions, capture and clean exit.
- Fixed-time 1600x900, 120-frame Earth/belt/tour captures completed with `--time 0 --vsync 0 --no-hud`. Earth and tour differed by at most one 8-bit channel code (143 and 125 pixels respectively). Belt differed in 1,408 pixels with mean absolute RGBA difference 0.000393 codes and maximum 14. An unchanged baseline repeat differed in 1,460 pixels, mean 0.000394 and maximum 5. This establishes tiny run-to-run variation of comparable mean magnitude, not bit identity or proof that every outlier is nondeterminism. The belt capture was visually inspected; no gross rendering regression was apparent. Local evidence is under `.scratch/quality-fixes/{before,after}`.
- GPU validation layers were unavailable locally; no validation-layer lifetime result is claimed. The Linux-only `/dev/full` close-error test was added but is not exercised by either Windows compiler run.

## Proposed commit boundaries for the current work

1. Preserve app settings and centralize bookmark selection, including app regression tests.
2. Repair renderer cleanup/resize and enforce existing UI upload/index contracts.
3. Validate image/texture and scene inputs, with focused negative tests (separate assets and scene commits).
4. Report buffered file completion failures, with file tests.
5. Record review findings, completed-fix status and this staged plan.

No commit or push is part of the current request.


## Stage 1 resumed verification

- Release build and all 11 CTest suites pass, including Vulkan core/synchronization and window lifecycle checks.
- GCC/MinGW build and all 10 CPU suites pass. Both compilers evaluate the padded-image constructor, accessors and rejecting factory at compile time.
- The PNG tests reject valid-header images of 16,385x5 and 7x16,385 before pixel allocation. Thin views accept 16,384 pixels on either axis and reject 16,385; malformed layouts/spans and supported-layout round trips remain covered.
- Paired 1600x900, 120-frame captures against committed `d1e1e0b` show mean absolute RGBA differences of 0.000011 (Earth), 0.000188 (belt) and 0.000013 (tour); maxima 1, 16 and 1. This is small run-to-run variation, not a bit-identical result. Commands/logs/report: `.scratch/refactor-resume/captures/`.
- Include, constexpr and domain-limit conventions are recorded in `CODE_STYLE.md`. No frame resource ownership changes are included in this milestone.
- Three-run belt GPU medians: window 5.426 to 5.443 ms; fullscreen 14.591 to 14.510 ms. No material total-GPU regression in these measurements. Recorded reports: [before](performance/image-refactor-before.md), [after](performance/image-refactor-after.md), with adjacent JSON metadata.


## Stage 2 implementation

- `GpuImage` is move-only and privately owns the view, texture and heap; lightweight constructors/handle accessors are constexpr. Formatting and allocation logic stay in its implementation file.
- Frame-sized targets and fixed-size maps/meters have separate owning groups. Resize and shutdown reset groups after explicit GPU waits, replacing independently maintained destruction inventories.
- Material/font upload now puts each allocation directly into its single owning vector. Upload loops index that vector instead of copying owning-looking handle triples into a second vector. Staging size and upload order are unchanged.
- The validation probe covers move construction, replacement of a live image after GPU completion, self-move, repeated reset, and non-copyability. The full release CTest suite passes, including synchronization validation and window lifecycle changes.
- The existing extent/divisor reconciliation remains in place: one full rebuild when extent changes, and only galaxy replacement when its divisor changes alone. No pass order, shader, target dimensions, or image format changes are included.

- Separate GPU-assisted validation passes, including owner-move/reset probes and fullscreen rendering. Six paired captures cover Earth, belt TAA on/off, high quality, sun-through-belt and late simulation time. Mean absolute RGBA differences range from 0.000014 to 0.000379 display codes (maximum 17); these are not bit-identical images. All 33 runtime shader binaries match the previous milestone exactly. Evidence: `.scratch/resource-ownership/{validation-gpu,captures}`.

- Sequential belt benchmarks measured 4.689 to 5.291 ms windowed (flagged noisy GPU increase) and 13.051 to 13.115 ms fullscreen. Alternating before/after builds then measured 5.210 versus 5.388 ms windowed (medians of three run medians); the baseline also slowed relative to its first measurement. This did not reproduce a regression exceeding both 5% and 0.2 ms, but is not a zero-cost proof. Reports: [before](performance/resource-ownership-before.md), [after](performance/resource-ownership-after.md), [alternating medians](performance/resource-ownership-alternating.json).
- Fullscreen-transition CPU frame peaks, measured over frames 28 through 35 of the same runs, were 96–138 ms before (median 117) and 66–82 ms after (median 73). These include target recreation, GPU wait and presentation rather than isolating allocation cost. Evidence: `.scratch/resource-ownership/resize-timings.json`.


## Stage 3 implementation

- `CameraView` is a render-facing value with position, unchanged basis vectors, vertical FOV and cut serial. The app adapter copies it per frame; the renderer no longer sees controller/navigation state. Camera accessors and the snapshot adapter are constexpr where possible.
- Tests cover non-default pose/FOV, complete basis mapping, cut propagation, and isolation of an already-created frame from subsequent camera mutations.
- App startup creates the HUD, supplies a checked image view, and releases its CPU pixels after renderer construction. Renderer HUD/font uploads share the RGBA upload adapter. HUD layout lives in the app namespace.
- No `src/render` file includes `app/`. Public borrowed scene types are forward-declared; their full definitions stay in implementation headers. Camera implementation moves from `orbital_scene` into `orbital_camera`, linked only by the executable and camera/app tests. The generated scene-test link rule contains no navigation library.
- Release build and all 11 CTest suites pass, including Vulkan core/synchronization validation and window lifecycle checks. GCC/MinGW build and all 10 CPU suites pass. The public renderer/camera headers also compile without a preceding scene definition.
- Five paired 1600x900, 120-frame captures cover Earth, belt, tour, visible HUD, and stopping camera motion at the belt. Mean absolute RGBA differences range from 0.000012 to 0.000206 display codes; maxima are 1, 14, 1, 1 and 5 respectively. These are near-identical, not bit-identical results. The HUD capture was visually inspected. All 33 runtime shader binaries match the committed stage 2 baseline. Commands, logs and pixel metrics are under `.scratch/app-render-boundary/captures/`.
- Three-run belt GPU medians are 5.366 to 5.247 ms windowed and 14.374 to 14.479 ms fullscreen. No tracked metric exceeds the suite's combined 5% / 0.2 ms regression threshold. Reports: [before](performance/app-render-before.md), [after](performance/app-render-after.md), with adjacent JSON metadata. The before executable was saved from committed `2df853d`; both reports were collected while the stage 3 working tree was dirty. HUD startup now has a transient CPU copy, released after construction; this benchmark measures steady rendering, not startup cost.


## Stage 4 implementation

- `render::Showcase` resolves and validates the renderer's supported scene before GPU creation: exactly one terrestrial, gas giant, desert and rocky moon, optional moonlets within the existing eight-body limit, and exactly one positive-thickness belt parented by gas giant ID. Generic validation remains unchanged.
- Frame states may arrive in any order. A bounded stack copy matches IDs to description slots before materials, mesh selection, body packing and atmosphere/belt passes consume them. Missing, duplicate, unknown or non-finite states fail at draw entry. This uses at most eight-by-eight ID comparisons, with no frame allocation.
- Renderer anchors use the resolved binding. The sun-through-belt reproduction camera now finds its parent by ID instead of assuming body slot 1.
- Documented ignored metadata (`scale_policy`, atmosphere scale, star radius/temperature/intensity) without changing their visual semantics. Star position and moonlet material seeds retain their consumers. General app bookmarks still assume the generated showcase order; arbitrary-system navigation is outside this stage.
- Release build and all 12 CTest suites pass, including Vulkan core/synchronization validation and window lifecycle checks. GCC/MinGW build and all 11 CPU suites pass. CPU contract tests cover independently reordered descriptions/states, invalid state identities/values/counts, missing/duplicate roles, absent/extra belts, wrong parent, zero thickness and generic invalid input.
- Five paired 1600x900, 120-frame captures cover Earth, belt, tour, sun-through-belt and high quality. Mean absolute RGBA differences range from 0.000014 to 0.000394 display codes, with maxima 2, 8, 1, 5 and 9 respectively. These are near-identical, not bit-identical results; the sun-through-belt image was visually inspected. All 33 shader binaries are unchanged. Local evidence: `.scratch/scene-contract/captures/`.
- Three-run belt GPU medians are 5.360 to 5.257 ms windowed and 14.537 to 14.368 ms fullscreen. No tracked metric crosses the combined 5% / 0.2 ms threshold against either the preceding executable or the retained packed-culling baseline; this is not proof of zero CPU cost. Fullscreen CPU preparation is 0.086 ms above the historical baseline (0.019 ms above the preceding executable). Reports: [before](performance/scene-contract-before.md), [after](performance/scene-contract-after.md), with adjacent JSON metadata and raw evidence under `.scratch/scene-contract/perf-*`. The before executable was saved from committed `b6b1c22`; both reports were collected with the stage 4 working tree dirty.


## Stage 5: frame-calculation extraction

- Camera/history preparation, motion lattice coordinates, body light selection, light projection, belt light/disc geometry and LOD, and screen-space sun visibility now have typed inputs/results in `render/frame_calculations`. They cannot access renderer `Impl`, GPU handles or shader structs.
- `renderer_frame_data.cpp` packs those results into the existing shader ABI; resource availability and effect settings remain in the packing layer. Pass order, projection convention, temporal thresholds, jitter sequence and float conversion points are preserved.
- The CPU target is renamed `orbital_render_cpu` and includes both the showcase contract and frame calculations. Tests exercise history rejection, pixel jitter/cycle, negative-coordinate motion cells, sun occlusion, orthographic projection, shadow selection and belt LOD endpoints/disable behavior.
- UI sections, shared semantic actions and CLI parsing remain a separate stage 5 chunk. Culling-specific band transforms remain local to culling packing for now.
- Release build and all 13 CTest suites pass, including Vulkan core/synchronization and window lifecycle checks. GCC/MinGW build and all 12 CPU suites pass.
- Six paired 1600x900, 120-frame captures cover Earth, belt, tour, sun-through-belt, high quality and stopping camera motion at frame 60. Mean absolute RGBA differences range from 0.000010 to 0.000365 display codes; maxima are 2, 16, 1, 4, 10 and 10 respectively. These are near-identical, not bit-identical results. All 33 runtime shader binaries match the committed scene-contract baseline. Evidence: `.scratch/frame-calculations/captures/`.
- Three-run belt GPU medians are 5.314 to 5.253 ms windowed and approximately 14.41 to 14.57 ms fullscreen. No tracked metric exceeds the combined 5% / 0.2 ms threshold against either the preceding executable or packed-culling baseline. CPU preparation varies by tens of microseconds; no zero-cost claim is made. Reports: [before](performance/frame-calculations-before.md), [after](performance/frame-calculations-after.md), with adjacent JSON metadata. The before executable was saved from committed `bc93b94`; both reports were collected with this working tree dirty.


## Stage 5: app controls and CLI

- Split ImGui lifecycle from panel assembly; each effect section accepts its settings rather than the entire app state. Preserve section order, labels, ranges, reset behavior and ID scopes. A mechanical comparison confirms every literal ImGui widget call/label is retained.
- Share orbit/free/tour/capture actions between panel, keyboard and applicable startup paths. Share constexpr enum cycling; reject orbit requests with missing/stale selected bodies instead of indexing an empty span.
- Move Options, usage and parsing out of the frame loop into the CPU-tested app target (`orbital_app_cpu`, replacing the camera-only target name). Share bounded-choice parsing while retaining option-specific domains and diagnostics. Reject non-finite floating-point values at parsing; the previous parser admitted non-finite pan and positive-infinite LOD scale.
- Release build and all 14 CTest suites pass, including Vulkan core/synchronization validation, the visible-panel render case and window lifecycle checks. GCC/MinGW build and all 13 CPU suites pass. New tests cover CLI defaults, bounds, missing/unknown/malformed/overflow values, paths, choices, non-finite input and repeated-option precedence, plus shared actions.
- Four paired 1600x900 captures (Earth, belt, tour and Earth with UI enabled) have mean absolute RGBA differences from 0.000011 to 0.000176 display codes; maxima 1, 5, 1 and 1. The app capture excludes the ImGui overlay. Separate desktop captures exercised the panel at the top and scrolled to the bottom; the lower panel was visually inspected and differs in 141 pixels, mean 0.000157 RGB codes (maximum 3), primarily reflecting its translucent background. Evidence: `.scratch/app-cleanup/{captures,panel}`.
- Three-run belt GPU medians are 5.237 ms windowed and 14.512 ms fullscreen. No tracked metric exceeds the combined 5% / 0.2 ms threshold against the recorded preceding milestone or packed-culling baseline. This standard suite runs with the panel hidden; it does not isolate panel CPU cost. [Report](performance/app-cleanup.md), adjacent JSON and raw evidence under `.scratch/app-cleanup/performance/`.


## Stage 6 measurement decision

- Added an opt-in `storage_profile` executable with isolated C++ allocation instrumentation, aligned allocation control and live-allocation checks. MSVC and GCC optimized builds both run successfully; raw CSVs and cache/input identity metadata are retained.
- Six-body evaluation costs about 0.456 microseconds with one 288-byte allocation per call. Generic validation allocates nothing on the valid path. A new immutable scene/reusable-output API is not warranted for this workload.
- The installed catalog contains 27 textures, 320 mips and 197.79 MiB of payload. Current parsing takes 227.01 ms serially; checksum-only work takes 180.98 ms. An optimistic single-copy-per-texture baseline takes 222.38 ms, reducing 508 allocations to 28 but only about 16 KiB of peak requested memory. It omits metadata/header validation and runs in fixed order; this is not a production speedup claim.
- Defer newly allocated slab storage. Ownership transfer of the file buffer could avoid payload copies, but needs a separate production-equivalent prototype and concurrent startup/staging measurements before changing the interface. Runtime code/cache format remain unchanged. Details and reproduction: [STORAGE_PROFILE.md](STORAGE_PROFILE.md).
