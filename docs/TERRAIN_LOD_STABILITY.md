# Terrain LOD stability under cache pressure

2026-09-19. Follow-up to the CDLOD continuity fixes on
`fix/terrain-cdlod-continuity`, through commit `1db6502`.

## Conclusion

Cache competition can cause repeated tile generation and visible local detail changes during
movement. Positive LOD bias increases the requested detail footprint without increasing cache,
tree, generation, or upload capacity, so it can amplify this effect. However, the user's remaining
visible glitches have not yet been correlated with eviction events. Cache pressure is a plausible
cause, not a confirmed diagnosis of that observation. Occupancy alone does not establish thrashing.

There is also a separate selection stability issue: tree retention has hysteresis, but actual child
selection still compares against a single distance threshold. A cached tile can switch in and out
without regeneration. Local neighbour balancing and edge constraints can then change the geometry
of nearby patches that already have their tiles.

The preferred direction is to preserve established visible coverage and admit new refinement only
when its dependencies and budget permit it. Under sustained pressure, accept locally coarser stable
coverage rather than continually replacing equally useful detail. Do not restore the previous
body-wide range scaling policy, which caused widespread LOD movement.

## What the code established on 2026-09-19

- The cache has 1024 slots, the tree has a 4096-node ceiling, generation admits at most 8 tiles per
  update, and the renderer has 32 staging rings. Actual generation admission is also capped by free
  rings. These are distinct constraints, not a single triangle budget.
- LOD bias multiplies distance ranges by `2^bias`: +1 doubles the distance at which a given split
  is requested. The exact increase in visible tiles depends on terrain and viewpoint.
- `visit()` touches selected tiles and their ancestors; all six roots are also touched. Eviction
  selects only resident slots last used before the current frame. Current traversal dependencies
  and pending generation are protected. Cache saturation therefore does not directly evict the
  tiles that traversal is currently using; requests can instead remain unserved.
- A tile that leaves traversal is eligible for eviction on the following frame. There is no warm
  retention interval enforced by the allocator. The existing `evicted_recent` counter merely counts
  eviction of tiles used within the previous 60 frames; it does not prevent eviction.
- `prepare()` retains existing child blocks out to `range / 0.8`, but recursive child preparation
  and `visit()` both use the strict `distance < range` test. This preserves some bookkeeping rather
  than providing separate promotion/demotion thresholds for actual drawn coverage.
- `stitch()` can coarsen resident fine subtrees to maintain a one-level neighbour difference.
  Changes in neighbour availability can therefore change a resident area's coverage and edge
  morph constraints without any tile being regenerated.
- In the renderer, full cache invalidation is triggered by a change to terrain micro-relief
  `detail`. Camera movement and LOD bias do not directly invalidate the cache. Disabling the near
  tier retains cached tiles. Pending jobs can be reclaimed after a 240-frame timeout plus sweep
  latency; this should be diagnosed separately from ordinary resident eviction.
- Commit `1db6502` fixed a proven visibility-related morph replay: looking away for one update and
  returning restarted the arrival morph on 56/56 unchanged cached patches. The regression now
  reports 0/56. That fix does not prevent actual eviction/reassignment or selection changes.

Relevant implementation: `src/render/terrain_tier.cpp` (`prepare`, `visit`, `stitch`,
`choose_generation`, `update`) and `src/render/renderer_terrain.cpp` (`prepare_terrain_tier`).

## Detect the cause before changing policy

The existing terrain panel already reports occupancy, pending jobs and their oldest age, free
rings, requested/served tiles, evictions/warm evictions, blocked splits, and local balancing.
These counters provide clues but cannot attribute an individual visible jump.

Add a bounded event history keyed by patch key and assignment stamp, with frame and reason:
generation requested/admitted/completed, resident eviction, timeout, explicit invalidation,
promotion/demotion, and balancing replacement. Track regeneration of recently evicted keys,
drawn coverage changes, and edge-mask changes on otherwise unchanged draws. Distinguish slot
exhaustion from the per-update generation limit, staging-ring exhaustion, and the node limit.
Keep short rolling totals so one-frame events can be inspected after a visible jump.

Replay a fixed camera and repeatable small pan/look-away path at biases 0, +1, +2, and +3, with
delayed/out-of-order uploads and an artificially restricted usable cache in tests. Record:

- Whether a fixed view settles with no further generation or coverage changes.
- Whether the same keys are repeatedly evicted and regenerated during small movements.
- Whether jumps occur with zero generation and eviction, indicating selection/balancing instead.
- The spatial extent of coverage changes, not just how many draw records changed.
- Whether seams remain continuous throughout transitions and all resource limits hold.

A full cache with no warm evictions or repeated generation may simply be retaining useful history.
Pending jobs with free cache slots point toward throughput pressure rather than cache capacity.

## Proposed changes, in order

1. Add the event attribution and deterministic pressure regressions above. Preserve the existing
   default distance/error settings while measuring.
2. Give recently selected tiles a bounded retention preference, including a small visibility margin
   where useful. Prefer cold evictions; if all candidates are protected, defer optional refinement.
   Retention must remain bounded and allow meaningful camera movement to replace old coverage.
3. Make refinement admission account for required children, fallback/material ancestors, neighbour
   compatibility, and transition overlap before committing a coverage change. Retain the parent
   until the required local replacement is ready. Prioritize expected screen-error reduction per
   additional resource cost, with preference for established coverage and deterministic tie breaks.
4. Introduce persistent promotion/demotion state with separate thresholds and a short dwell period
   where needed. Integrate this with geomorph endpoints and neighbour constraints; changing only
   the distance comparison can retain children beyond their valid morph band or introduce seams.
5. Under sustained overload, defer low-benefit splits or coarsen selected low-benefit regions with
   controlled transitions. Recover refinement gradually after headroom returns. Avoid a per-frame
   global quality multiplier; a larger cache alone would only move the pressure threshold.

These were proposed follow-up changes, not implemented by the original note. The code inspection establishes
the mechanisms above; a runtime trace is still needed to identify which dominates the reported
remaining glitch.

## Verification and changes, 2026-09-20

Reviewed `fix/terrain-cdlod-continuity` through `9f8f8f6`. The arrival-morph fix remains covered by its
regression. The earlier horizon guard and frozen-camera drift fixes are already present; the older
`terrain_culling.md` report is not a list of outstanding bugs against this branch.

### Deterministic cache sweep

Run `build/release/terrain_tier_tests.exe --cache-sweep` (or the corresponding Linux executable).
[Recorded CSV](data/terrain-cache-sweep-2026-09-20.csv): 24 cases, 755 updates each, seed 1007,
1024/2048/4096 slots, biases 0 through +3, nadir and grazing orientations. Each case settles for
150 updates, repeats a small pan / 35-frame look-away / return three times, then holds the view for
80 updates. The cycle restarts the pan at its initial position. Uploads arrive after deterministic,
out-of-order delays; one request is held longer. Admission is fixed at six tiles/update and the node
budget stays 4096, so cache capacity is the only resource limit varied.

At bias +3:

| View | Slots | Generated | Evicted (warm) | Re-generated keys | Coverage changes | Final 80: generation / changes |
|---|---:|---:|---:|---:|---:|---:|
| Nadir | 1024 | 2143 | 1119 (705) | 847 | 17261 | 0 / 0 |
| Nadir | 2048 | 1417 | 0 (0) | 0 | 14273 | 0 / 0 |
| Nadir | 4096 | 1417 | 0 (0) | 0 | 14273 | 0 / 0 |
| Grazing | 1024 | 2251 | 1227 (1223) | 1035 | 16355 | 95 / 469 |
| Grazing | 2048 | 1759 | 0 (0) | 0 | 13501 | 0 / 0 |
| Grazing | 4096 | 1759 | 0 (0) | 0 | 13501 | 0 / 0 |

All bias 0/+1/+2 cases produced identical results across capacities, with no eviction or regeneration
and quiet final phases. All cases passed cache accounting, ownership, node-budget and sampled boundary
checks: 154232 checked boundary vertices, zero violations. The six-slot unit regression separately
forces slot exhaustion and distinguishes it from a zero generation budget; it covers empty, pending,
resident, disabled, invalidated and age-bin boundary states.

Coverage changes count added/removed `(patch key, quadrant mask)` records, not pixels or a count of
visual glitches. Edge-mask changes on unchanged records are counted separately. The
`changes_without_generation` column means no new requests were admitted on that frame; previously
pending uploads can still complete, so it does not isolate pure selection changes. Likewise,
`regenerated_within_60_frames` measures time between generation requests, not time since eviction.
The tests sample real tile heights and enforce edge morph constraints, but do not establish temporal
image quality or attribute a particular user-observed horizon patch.

This reproduces a real cache-capacity failure: the stationary +3 grazing tail continues replacing
detail at 1024 slots and settles at 2048. It also shows why cache growth is incomplete: movement and
look-away/return still change coverage and edges with zero evictions at larger capacities.

### Implemented response

- Renderer capacity is now 2048 layers in all three tile arrays, approximately 70.3 MiB total versus
  35.1 MiB. Patch-record storage and the shader's missing-parent layer sentinel move with it.
  Generation limits, staging rings, tree budget, error settings and LRU retention policy are unchanged.
  This is a measured mitigation for the replay, not a general admission-policy solution.
- CPU `TerrainTier(capacity)` keeps 1024 as the regression baseline and supports the 4096-slot
  experiment independently. CPU absence is `no_slot`, separate from any valid slot. The renderer
  explicitly converts it to its shader sentinel. A 4096-layer renderer would need a parent-sentinel
  packing review (the current field is 12 bits), as well as device-limit validation.
- Cache snapshots expose free/resident/pending slots, oldest last-use age, pending age and eight
  30-frame age bands (the last is 210+). These are render-frame ages, not wall-clock seconds or tile
  creation ages. Counts remain truthful while the near tier is disabled; its stale patch map clears.
- Requests are deduplicated for diagnostics, priority ties resolve by patch key, and admission reports
  slot blockage separately from throughput deferral. Once no slot is evictable, the remaining requests
  are counted without rescanning the whole cache for each one. Lifetime eviction totals survive
  invalidation and disabling. No bounded per-patch event history is implemented yet.
- Terrain controls and six grouped readings (Cache/Flow, Queue/Evict, Tree/LOD) are together in two columns.
  Readings use x/y/z notation with hover explanations and amber `!` problem indicators. Terrain settings
  and the patch map stay unfolded while the Terrain section is open. The previous two
  expandable counter lists have been removed; secondary readings live in the relevant hint. Redundant
  cull timing was removed from Frame; normal range coverage and morph endpoint counts no longer show
  as alarms. The frame panel also now copies the body count correctly instead of displaying zero.
  G toggles wireframe, N near terrain, V cycles debug views, C freezes terrain and belt culling;
  F remains free camera and E vertical movement. `--help`, HUD and README describe the shortcuts.
- Benchmark CSVs append named `terrain_*` capacity, residency, age, admission, eviction, ring and
  balancing columns, so a transient event can be inspected after the run.

### Further UI/UX proposals

1. Select a patch from the map or viewport, then show its key, slot/stamp and the existing `explain()`
   chain. Pair it with a bounded recent-event list to identify an individual coarse horizon patch.
2. Add a separate "Hold diagnostics" control that freezes readings and the event list without
   freezing selection. C currently freezes the cull camera; it is not a diagnostic-history pause.
3. Add a repeatable camera-path preset and a compact comparison view for two recorded benchmark CSVs.
   Keep the detailed event/cull data there, while leaving the live panel focused on current pressure.

### Build and runtime verification

Release C++ and shader builds succeeded. All 17 CPU CTest cases passed, and renderer validation
passed on a serial rerun. An earlier concurrent renderer-validation launch exited during asset
loading without a validation message; its cause was not established. The separate `--truth` terrain
run found zero uncovered visible probes in all eight tested horizon/lowland/summit views.

The compact panel was inspected at 1280×900 and 960×720, then its expanded diagnostics at 1280×900.
A temporary offscreen UI capture hook was removed after inspection (normal captures omit ImGui).
The pre-capacity-change +3 snapshot showed 505 free slots, 31 pending tiles and two free upload rings:
an example of throughput pressure despite substantial cache headroom.

The final 2048-slot GPU run used a GTX 1080 Ti with core/synchronization validation enabled:

```powershell
build/release/orbital.exe --headless --ui --bookmark 9 --time 0 --tier-activate 100 `
  --lod-bias 3 --wireframe 1 --frames 2400 --width 1920 --height 1440 `
  --benchmark captures/terrain-cache-2048-validation.csv `
  --capture captures/terrain-cache-2048-validation.png
```

The OS constrained the actual client height to 1421. It reached 1418 resident tiles, exercising
layers beyond the former 1024 limit, and ended with 1005 drawn patches, 630 free slots and 1666 nodes.
The final 100 frames had zero generation admissions, evictions and pending tiles. No core or
synchronization validation findings were logged. A separate 1280×900 fixed run settled at 637
resident tiles. These runs verify the larger allocation/streaming path; the deterministic CPU sweep
provides the controlled capacity comparison. Neither is a reproduction of the exact reported patch.
