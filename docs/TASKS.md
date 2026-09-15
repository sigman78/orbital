# Task list

Preliminary, gathered from the September 2026 sessions after the lens, HDR,
exposure, timing and memory work merged. Order within a group is the suggested
order; effort is a rough single-person estimate. Decisions behind the items are
in DECISIONS.md, measurements in PERFORMANCE.md and docs/performance/.

## 1. Panel and instrumentation

| # | Task | Notes | Effort |
|---|------|-------|--------|
| 1.1 | Exponential moving average for every panel reading | Replace the 0.5 s deque window in `timing_average.hpp` with an EMA that covers the group timings, their children, the meter, cull counts and memory. Time constant about 0.5 s, reset on camera cut and bookmark. Readings are hard to read while they jump. | half a day |
| 1.2 | Confirming run for the fullscreen belt surface and atmosphere increase | A +0.45 ms delta was flagged once; the child timings now say which pass moved. Run the benchmark suite twice at 1920x1080 and record. | hour |
| 1.3 | Frozen frustum outline | Optional: draw the frozen cull camera's frustum edges while Freeze culling is on, so the cut is visible from outside. | half a day |

## 2. Bodies: culling, order and cost

The planets cost as much as the whole belt at distance. Nothing about a body is
culled today and every body runs its full shader at every size.

| # | Task | Notes | Effort |
|---|------|-------|--------|
| 2.1 | CPU frustum culling of bodies | Sphere against the padded frustum, same parameters the belt cull takes; uses the frozen cull camera when the freeze holds. Mandatory before anything else here. | half a day |
| 2.2 | Body depth pre-pass | Order independent, so a giant in front of its belt or another body costs nothing to shade twice. Measure with the Bodies and Rocks children before and after; draw-order tricks were rejected because the occluder set changes with the view. | one day |
| 2.3 | Fast-path body shader tiers | Chosen by projected size. No-pop rule: lighting and tone terms stay identical across tiers, only detail terms (normal maps, clouds, night lights, ocean specular, band detail) fade with the texel-to-pixel ratio, so the switch is invisible. Candidates in order: Earth, gas giant, airless bodies. | two days |
| 2.4 | Far-tier body impostors | Below a few pixels a body is a disc of its average colour with the phase shading, drawn as a billboard; no surface pass at all. Ties into 2.3's smallest tier. | one day |
| 2.5 | Async cull one frame ahead | Double-buffered cull scratch, frustum padded for one frame of motion, camera cuts fall back to a same-frame cull. Removes the cull from the frame's critical path. | one day |

## 3. Belt motion on the CPU

Rocks are placed and tumbled in closed form on the GPU today (cull, light-map
splat and disc bake all recompute it) and TAA sees no rock motion. Physics on
the CPU needs the positions there, and the same data gives TAA proper motion.

| # | Task | Notes | Effort |
|---|------|-------|--------|
| 3.1 | Per-rock state buffer written by the CPU | SoA position and previous position; the static record keeps radius, seeds and class. The three consumers read state instead of recomputing; staging copy into device memory per frame, not host-visible reads across the bus (BAR heap is at its edge). First cut writes the same closed-form motion, so the image is unchanged and the upload cost is measured honestly (12.5 MB per frame at 520k rocks). | one day |
| 3.2 | Motion vectors for rocks and bodies | Previous position through the instance record to a velocity target, per-pixel reprojection in TAA. Replaces the moving-clip workaround for rock trails; bodies get rotation motion for free. | one to two days |
| 3.3 | Physics integration pass | One sequential streaming pass per frame that integrates, writes the state and sets sector flags; SIMD lanes and a job split across cores, far tail optionally across frames. Deterministic fixed step so captures and benchmarks stay reproducible. | two days |
| 3.4 | Sector partition and coarse CPU cull | Belt sorted by band and angular sector with slack for spawns; the CPU rejects sectors in bulk, the GPU refines. Per-rock CPU culling stays out: the GPU pass is 0.19 ms and the splat lighting needs textures. | one day |
| 3.5 | Unified state model for bodies and rocks | Same SoA layout and integrator interface; bodies are a few entries with mass and their own draw path. Spawn and destroy as appends and holes. | with 3.3 |

## 4. Image quality investigations

| # | Task | Notes | Effort |
|---|------|-------|--------|
| 4.1 | TAA contrast loss | TAA-only light was halved in the belt flight captures; check the history weight and clip against the Sep 11 belt and Dawn medians. | half a day |
| 4.2 | Mars at +2.4 stops under the histogram meter | Confirm whether the highlight band or the key is responsible; may want a per-body sanity capture in the suite. | hours |
| 4.3 | HDR output on the display | Verify the metadata path and paper-white tone on the HDR desktop once more after the exposure changes; record the observed peak. | hours |

## 5. Suggested order

1. 1.1 and 2.1 (small, unblock reading the rest).
2. 2.2 with the child timings, then 1.2 as the confirming run.
3. 3.1 then 3.2, since motion vectors also settle 4.1.
4. 2.3 and 2.4 together, 2.5 after the body work so its padding covers both.
5. 3.3 to 3.5 when the physics model is ready.
6. 4.2 and 4.3 whenever a capture session is open.
