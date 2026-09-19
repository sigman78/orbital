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

## What the current code establishes

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

These are proposed follow-up changes, not implemented by this note. The code inspection establishes
the mechanisms above; a runtime trace is still needed to identify which dominates the reported
remaining glitch.
