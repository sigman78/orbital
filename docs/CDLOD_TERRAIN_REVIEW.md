# CDLOD cube-planet terrain review

Date: 2026-09-18

Scope: CPU quadtree selection, tile residency and asynchronous uploads, patch geometry, vertex morphing, and related tests. This records a read-only review; no implementation changes were made. The working tree was being edited concurrently, so references describe the code inspected during this review.

The strongest candidate for patches that stay coarse after a flyby is persistent starvation at the quadtree's node budget. Upload lifetime faults can independently leave a slot marked resident while its GPU contents are incorrect. The user's exact flight was not reproduced in the renderer.

## 1. P1: Existing splits can permanently block more important new splits

Source: [TerrainTier::visit](../src/render/terrain_tier.cpp), particularly the `wants` and `allowed` decisions around line 170.

Existing children bypass the node budget; new children require `nodes() < slot_count - 64`. Traversal follows fixed face and child order. There is no mechanism to reclaim lower-priority splits for a more demanding patch, and hysteresis helps existing splits retain their allocation after camera movement.

This can produce a stable but inadequate selection: a patch keeps requesting a split internally, the budget keeps refusing it, and no generation requests are issued for its missing descendants. An idle generation queue therefore does not prove that refinement has converged to the desired detail.

### Evidence

An independent, in-memory Python model of the selection rules treated every tile as immediately resident, removing streaming as a cause. It used the cube mapping, patch bounds, frustum and horizon tests, distance ranges, split hysteresis, child gates, and node budget. This was a model of the CPU rules, not execution of the production C++ selector or a GPU reproduction.

Parameters:

- View height: 900 pixels; vertical FOV: 60 degrees; aspect ratio: 16:9.
- Camera radial distance: 1.03 planet radii.
- Initial direction: normalized `(0.6, 0.3, 1)`.
- Final direction: normalized `(0.65, 0.3, 1)`.
- Grazing forward direction: normalized tangent minus `0.1` times radial direction.
- LOD bias: `+2`.
- Each view held for 120 selection updates.

| State | Blocked splits | Draws | Nodes |
| --- | ---: | ---: | ---: |
| Initial settled view | 12 | 596 | 962 |
| After moving and settling | 22 | 578 | 962 |
| Fresh selector at the final view | 15 | 579 | 962 |

The moved and fresh selections differed by 21 draw keys in their symmetric difference. Lower-bias cases tested did not hit the budget and converged to the same drawn keys after movement. The persistent failure is conditional on budget pressure.

Suggested direction: redistribute the selection budget according to current view importance, with a controlled merge policy. Increasing tile-generation throughput cannot resolve a selection that never admits the desired splits.

## 2. P1: An unfinished worker's staging buffer becomes reusable after 102 frames

Source: [prepare_terrain_tier](../src/render/renderer_terrain.cpp), ring availability and submission around lines 186–200.

A submitted job reserves its ring entry by setting `ring_used_frame[ring] = frame_index + 100`. Availability is tested using `frame_index >= ring_used_frame[ring] + 2`. A queued or running job that exceeds 102 frames therefore loses exclusive ownership of its staging buffer even though it has not completed.

Another worker can write the same memory, or a GPU upload can read data belonging to another tile. The destination slot may subsequently stay marked resident, preventing automatic regeneration of incorrect contents.

This is a source-confirmed lifetime defect with a conditional trigger; a slow-job GPU reproduction was not performed.

Suggested direction: represent worker ownership explicitly. After worker completion, track GPU use separately until the staging memory is safe to reuse. A future frame number is not a completion token.

## 3. P1: Rejected stale results are still uploaded

Sources: [prepare_terrain_tier](../src/render/renderer_terrain.cpp), result polling around line 84; [TerrainTier::mark_resident](../src/render/terrain_tier.cpp), around line 353.

The caller ignores the return value of `mark_resident()`. It queues all three texture copies even when the slot/key check rejects the result. After invalidation and slot reuse, an old result can overwrite the tile currently occupying that layer.

Slot and key are also insufficient to identify a generation. If the same patch receives the same slot after a detail-setting change, a result from the previous settings can pass the check. Completion order can then determine which generation remains on the GPU.

Suggested direction: attach a generation identifier or epoch to requests and results, validate it before accepting residency or recording copies, and discard rejected results while correctly releasing their staging ownership.

## 4. P1: Aborted frames can leave tiles resident without uploading them

Sources: [Renderer::draw](../src/render/renderer_frame.cpp), preparation and early returns around lines 39–51; [prepare_terrain_tier](../src/render/renderer_terrain.cpp), copy-list clearing and result polling.

Terrain preparation polls results and marks slots resident before drawable and swapchain-acquisition checks. If drawing returns early after preparation, the queued copies are never submitted. The next preparation clears those copy lists, but CPU residency remains true.

The slot can then display old or uninitialized GPU contents without requesting another tile. This finding is conditional on an aborted frame, such as failed swapchain acquisition. Ordinary minimization is already guarded in the application loop, so minimization alone was not established as a reproduction.

Suggested direction: retain pending uploads across aborted frames, or commit residency with the successful submission that makes the tile usable.

## 5. P2: Quadrant removal leaves parent triangles underneath children

Source: [evaluatePatch](../shaders/surface/patch.slang), quadrant masking around lines 65–81.

The vertex test always retains vertices on either central grid line. Some triangles inside an excluded quadrant have all three vertices on those lines, so none of their vertices is rejected.

An in-memory enumeration of the actual 32-by-32 grid triangulation confirmed:

| Excluded quadrant | Surviving parent triangles |
| --- | --- |
| 0 | None |
| 1 | `(16,15)–(17,16)–(16,16)` |
| 2 | `(15,16)–(16,16)–(16,17)` |
| 3 | None |

These triangles overlap the child replacing that quadrant and can cause local depth fighting or incorrect coverage. This check establishes the masking defect; it did not capture the resulting GPU artifact.

Suggested direction: exclude complete primitives, for example through quadrant index ranges or an equivalent primitive-level mechanism.

## 6. P2: The fade does not enforce continuity at partial parent/child boundaries

Sources: [TerrainTier::visit](../src/render/terrain_tier.cpp), sibling fade and partial coverage around lines 190–234; [evaluatePatch](../shaders/surface/patch.slang), morph calculation around line 98; [patch_grid_mesh](../src/scene/terrain_patch.cpp).

A resident child can finish fading while a neighboring sibling is still missing. The parent continues covering that sibling, leaving an unmatched boundary. The fade is based on the resident children that will be visited, rather than holding a boundary constraint until its neighbor is ready.

Furthermore, assigning `fade = 0` to the parent's remaining quadrants does not hold their geometry at the parent's own shape. The shader still computes `max(distance_morph, fade)`, so distance morphing continues.

The shared mesh has skirts only around the full patch perimeter. It has no skirts along internal quadrant boundaries, so those skirts do not cover every parent/child fallback seam.

### Existing test evidence

The available terrain-tier test binary reported:

- Without fade: 2,624 boundary-vertex violations across 14 frames; worst frame 425.
- With fade: 72 violations across two frames; worst frame 68.

The test passes because [test_morph_streaming](../tests/terrain_tier_tests.cpp) asserts only that fading produces no more violations than the unfaded case. It does not require continuity.

Suggested direction: enforce compatible geometry at shared boundaries during streaming and budget fallback. A fixed-duration fade can smooth arrival, but does not establish that compatibility.

## 7. P2: The clamp-mode ancestor search stops too early, and the test repeats the defect

Sources: [TerrainTier::mark_finer_sides](../src/render/terrain_tier.cpp), `covering` around line 254; [MorphCheck::violations](../tests/terrain_tier_tests.cpp), `coverer` around line 135.

Both searches increment `up` while decrementing `cell.level`, using the condition `up <= int(cell.level)`. Starting from level 6 visits only levels 6, 5, 4, and 3. It never checks levels 2, 1, or 0.

The production search can miss a coarser covering ancestor and classify the neighbor as finer, incorrectly clamping the morph. The test's search can miss the same ancestor and silently skip checking that boundary.

Suggested direction: walk until the root has been checked, with a stopping condition that does not shrink alongside the traversal counter. Validate the test helper independently, including multi-level gaps.

## 8. P2: Selection uses the child's error threshold to decide whether the parent is adequate

Sources: [TerrainTier::visit](../src/render/terrain_tier.cpp), split and child-range gates; [TerrainTier::level_error](../src/render/terrain_tier.hpp), error-table definition.

The table describes each level's own approximation error, but a level `L` node splits against `range[L+1]`. It can therefore remain selected after its own table-predicted error exceeds the documented tolerance.

At zero bias, the level-2 to level-3 error ratio is approximately 3.46. Near the child threshold, this permits approximately 5.2 pixels of table-predicted level-2 error against the stated 1.5-pixel target, before accounting for morphing.

Suggested direction: derive selection thresholds and morph bands together from the error of the geometry actually drawn. Changing only one comparison can invalidate the intended seam relationships.

## 9. P2: The calibrated error metric omits geometric error

Source: [patch_error](../src/scene/terrain_patch.cpp), around lines 195–210.

The function compares scalar radii using `length(d)` where `d` is a normalized cube direction. Despite the comment, this removes sphere curvature from the measurement. A perfectly spherical constant-height patch would report zero approximation error even though its rendered triangles form a faceted surface.

The comparison also uses the mean of all four corner heights, a bilinear-center estimate. The rendered mesh uses triangles, whose surface at the quad center depends on the diagonal endpoints instead.

The level table is additionally based on random patch samples and extrapolation of finer levels. It is an estimate, not a conservative error bound.

Suggested direction: measure deviations from the actual rendered triangle surface in body-space positions, including curvature. Treat sampled calibration separately from a guaranteed bound.

## Additional contributor: Global height bounds inflate selection demand

Source: [TerrainTier::nearest_distance](../src/render/terrain_tier.cpp), around lines 86–94.

Every patch uses the full terrain height shell `[-0.05, +0.05]`. If the camera lies inside that shell and its direction lies inside a patch's angular cap, the calculated nearest distance can be zero even when the actual ground is appreciably farther away.

This is conservative bounding, but it can drive unnecessarily deep selection and worsen node-budget contention. Tighter per-node height bounds would make demand better reflect actual terrain. Any replacement must remain conservative for the geometry and morphs it bounds.

## Validation and limits

- Ran the available `terrain_tests.exe` and `terrain_tier_tests.exe`; both exited successfully.
- The terrain tests reported matching parent/child even height texels, matching decoded normals at tested cube edges and corners, and passing sampled patch-bound checks.
- Used file-free Python checks for the selection-budget model, quadrant triangle enumeration, and ancestor-loop traversal.
- Did not rebuild the project, add tests, modify implementation files, or reproduce the user's exact flight on the GPU.
- Existing test binaries and a concurrently edited working tree do not constitute a fresh-build validation of every source revision.

For the reported stuck-patch symptom, investigate node-budget starvation first. Distinguish it from incorrect GPU tile contents despite CPU residency. The existing `splits_blocked`, node count, requested/served, pending, and residency counters can help separate these cases; an empty generation queue alone is insufficient.

## Follow-up: Finest-level 2x border evaluation fix

Implemented on 2026-09-18 after the user reported a persistent border between the finest visible level and its surroundings, even after streaming finished.

Each generated height tile now supplies its own minimum and maximum, padded for r16_unorm encoding and float decode. The generation stamp protects these bounds together with the tile contents. Resident child selection, range-span checks, and morph diagnostics use this interval. Unknown tiles, descendant discovery, and visibility culling retain the full conservative terrain bounds. These extrema bound the bilinear surface actually rendered; they are not claimed to bound unsampled finer terrain.

This addresses the case where the global +0.05 ceiling selects a fine patch even though every actual vertex is beyond that patch's morph end. Previously, the fine patch and its coarser neighbor could both be fully collapsed, preserving a 2x spacing discontinuity. The shader's morph formula and distance bands are unchanged.

Validation:

- Release renderer build succeeded.
- Both terrain test suites passed.
- A C++ regression reproduces the old all-collapsed selection on a flat sphere at radius 1.08. Supplying the resident height range changes the finest selected level from 7 to 5, with unmorphed vertices near the camera, for both overhead and grazing views.
- Generated terrain at two close viewpoints, including a cube-edge view: 1,156 boundary vertices checked, zero morph violations.
- A 900-frame GPU capture settled at 51 draws, 149 resident tiles, zero pending tiles, zero blocked splits, and zero arrival fade. Capture: `captures/cdlod-height-bounds-wire.png`.

From the repository root, launch the visual evaluation build with:

```powershell
.\build\release\orbital.exe --bookmark 9 --back -0.05 --time 437.5 --width 1600 --height 900 --near-tier 1 --wireframe 1 --lod-bias 0 --lod-seam 0 --ui
```

Use the existing `Patch view` control's `Morph and level` view to inspect the shader's actual morph. This is a targeted fix for the saturated-selection mechanism, not a claim that all streaming or grazing-view seam cases from the review are resolved.
