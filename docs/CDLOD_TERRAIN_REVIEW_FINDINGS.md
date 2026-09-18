# CDLOD terrain review: verification and outcome

Date: 2026-09-18

Companion to [CDLOD_TERRAIN_REVIEW.md](CDLOD_TERRAIN_REVIEW.md). Every item in that
review was checked against the code and, where the claim was about behaviour, against
measurement. All nine plus the additional contributor are confirmed as descriptions of
the source. Two of them do not have the consequences the review attributes to them, and
one of the review's own suggested directions is wrong; both are recorded below with what
was measured.

Fixed in `caf7f6e` (items 2, 3, 4, 7 and the comment half of 6) and `eb006e8` (the
additional contributor). Items 1, 5, 8 and the behavioural half of 6 are open.

## Summary

| Item | Claim about the source | Consequence as described | State |
| --- | --- | --- | --- |
| 1 | confirmed | confirmed, at bias 2 only | open |
| 2 | confirmed | confirmed | fixed |
| 3 | confirmed | confirmed, and understated | fixed |
| 4 | confirmed | confirmed | fixed |
| 5 | confirmed | confirmed | open; review's suggested direction is sound, its obvious alternative is not |
| 6 | confirmed | confirmed | comment fixed, behaviour deferred |
| 7 | confirmed | **not reproducible** | fixed anyway |
| 8 | confirmed | confirmed | open |
| 9 | confirmed | **immaterial as stated**; the real defect is adjacent | open |
| height bounds | confirmed | confirmed | fixed |

## Items 2, 3 and 4: one defect with three faces

All three were the same mistake — a frame number standing in for a fact nobody tracked —
and took one fix.

A staging entry now carries explicit state (`free`, `worker`, `upload`) instead of
`ring_used_frame[ring] = frame_index + 100`, so a job that outlives any frame count keeps
its buffer. Availability still waits two frames after a *recorded copy*, which is the only
thing a frame number can legitimately guard.

Item 3 understates the problem. Slot and key cannot identify a generation at all: evict a
patch and ask for it again, or change the detail setting and regenerate everything, and
the same patch is handed back the same slot, so an older result passes a check made of
both. Each slot assignment now issues a **stamp**, once, never reissued; a result carries
the stamp of the assignment that asked for it. `invalidate()` clears slots to stamp 0 and
stamps start at 1, so nothing in flight across an invalidation can be accepted.

Item 4 is fixed by making the copy and the residency a single decision, taken together in
`record_terrain_uploads` inside the command buffer. A tile counts as resident exactly when
the upload that makes it so has been recorded. Uploads that no frame recorded are retained,
not dropped.

Test: same slot, same key, different stamp across an `invalidate()` — the in-flight result
is refused, the current one accepted.

## Item 7: real in the source, not reachable in practice

The loop provably stops after `ceil(L/2)` steps. Both walks were then run side by side over
a flying, streaming selection — three altitudes, three LOD biases, 600 frames each, the
halted walk against a walk to the root:

**8.3 million neighbour queries, zero differing verdicts.**

A CDLOD quadtree stays balanced enough across a shared edge that the covering ancestor is
always inside the halved budget. Multi-level gaps of the kind the bug needs do not arise
in a selection driven by a distance metric. Fixed in both `mark_finer_sides` and the test
helper as a landmine, but it was never a symptom of anything.

## Item 9: the curvature claim is true and does not matter

`length(d)` on a normalized direction does remove curvature, as described. What that costs
was measured by comparing the drawn triangle surface against the true surface in body-space
positions, on real terrain:

| level | table | scalar (today) | geometric (true) | curvature alone | geo/table |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 1.834e-02 | 1.672e-02 | 1.832e-02 | 1.245e-03 | 1.00 |
| 1 | 1.268e-02 | 1.075e-02 | 1.131e-02 | 3.191e-04 | 0.89 |
| 2 | 4.783e-03 | 4.340e-03 | 4.691e-03 | 8.080e-05 | 0.98 |
| 3 | 1.381e-03 | 2.061e-03 | 2.595e-03 | 1.675e-05 | 1.88 |
| 4 | 6.016e-04 | 6.461e-04 | 7.362e-04 | 4.614e-06 | 1.22 |
| 5 | 2.848e-04 | 2.842e-04 | 3.499e-04 | 1.057e-06 | 1.23 |
| 6 | 1.197e-04 | 1.133e-04 | 1.150e-04 | 3.037e-07 | 0.96 |
| 7 | 7.520e-05 | 5.566e-05 | 9.292e-05 | 6.913e-08 | 1.24 |
| 8 | 2.720e-05 | 1.244e-05 | 2.173e-05 | 1.623e-08 | 0.80 |

The omitted curvature term is 7% of the total at level 0 and under 0.1% from level 4 down.
It does not distort the table, and a constant-height patch reporting zero error is a
theoretical embarrassment rather than a practical one.

The defect that does matter is the one the review mentions in passing: the scalar
mean-of-four-corners measure misses the real triangle deviation by up to a factor of two,
**inconsistently by level**. Since `range_L` is proportional to `error_L`, that lands
directly on the ratios between adjacent ranges:

| levels | from today's table | from the geometric measure | CDLOD asks for |
| --- | ---: | ---: | ---: |
| 0 → 1 | 1.45 | 1.62 | 2.00 |
| 1 → 2 | 2.65 | 2.41 | 2.00 |
| 2 → 3 | 3.46 | 1.81 | 2.00 |
| 3 → 4 | 2.30 | 3.52 | 2.00 |
| 4 → 5 | 2.11 | 2.10 | 2.00 |
| 5 → 6 | 2.38 | 3.04 | 2.00 |
| 6 → 7 | 1.59 | 1.24 | 2.00 |
| 7 → 8 | 2.76 | 4.28 | 2.00 |

Correcting `patch_error` does not fix this — it reshuffles it. Steps between 1.24× and
4.28× cannot give a consistent morph band, because CDLOD's morph band is defined as a
fraction of a range whose neighbour is assumed to be half of it. This is the strongest
remaining candidate for the hard level edge seen from the ground, and it is a design
decision (a prescribed ×2 series, with the error table used to place the series rather
than to define each step) rather than a bug fix.

## Item 5: confirmed, and the obvious fix is wrong

The surviving triangles reproduce exactly as tabled: quadrant 1 leaves
`(16,15)–(17,16)–(16,16)`, quadrant 2 leaves `(15,16)–(16,16)–(16,17)`, 16 across all 16
masks, out of 2048 per patch.

The tempting fix — flipping the quad diagonal on a checkerboard so no triangle has all
three vertices on the centre cross — removes all 16 and **must be rejected**. Enumerated:

```
uniform       child-at-m=1 triangles 512, parent 512, identical: True
checkerboard  child-at-m=1 triangles 512, parent 512, identical: False
```

At `m = 1` the child's morphed mesh would no longer equal the parent's triangulation, which
is the invariant the whole morph rests on. It would crack every level boundary to remove a
sliver at one patch centre.

The triangles survive because their vertices lie on the centre cross and are *shared* with
the neighbouring quadrant, so no per-vertex rule can reach them. The review's own suggested
direction is the sound one. The cheapest form that keeps a single draw per patch: duplicate
the 65 centre-cross vertices once per adjacent quadrant (64 vertices at 2 quadrants, the
centre at 4, so +67 of 1089, about 6%) and carry an explicit quadrant index per vertex.
Duplicates hold identical positions, so the morph and the collapse invariant are untouched.

## Item 6: the comment was the lie

The review is right that `fade = 0` does not hold the parent's quadrants still — the shader
morphs by `max(distance, fade)`, so dropping the floor leaves the distance term running.
The comment claiming otherwise was wrong and has been corrected.

The behaviour is deliberate and stays. Held still, the parent would crack against its outer
neighbours instead, which is CDLOD's granularity limitation: one node carries one level of
transition, not two. The real remedy is the streaming policy — never split until all four
children are resident — which is deferred.

## Item 1: real, but not what was being flown

Reproduced at `--lod-bias 2` from the ground:
`nodes 962/960, blocked 52, req 2/405, evict 0, starved 433, behind 1.35 (176 over a level)`.

At bias 0 the flight log that prompted the investigation reads `blocked 0, nodes 370/960`,
so budget starvation is not the cause of that particular symptom. Open.

## Additional contributor: global height bounds — fixed

The largest practical win of the set. Every patch used the full `[-0.05, +0.05]` shell, so a
camera inside that shell over a patch's angular cap computed a nearest distance near zero and
selected maximum depth for terrain that was not there. `test_resident_height_selection`
records the mechanism: on a flat sphere the selection reached level 7 although every vertex
of levels 6 and 7 was past its morph end, so both sides collapsed and the transition became
a hard 2:1 border.

A resident tile now supplies its own height range, measured from the tile it uploaded and
padded by one `r16_unorm` quantum plus float decode roundoff.

The decisive design point is an asymmetry that would be easy to get backwards: **the split
test keeps the global shell**, because a resident tile bounds its own bilinear surface and
says nothing about relief its children will reveal. Only drawing and gating use the tight
range. Frustum culling still uses `bound_radius` off the global shell and the skirt drop.

Measured on real terrain, flying 700 frames, the same cached tiles in both runs:

```
camera    altitude  bounds    graded%  at own%  pinned%  deepest    drawn    tiles   behind
overhead  0.004     global        92%       1%     7.4%       10      139      999     0.86
overhead  0.004     resident      89%       5%     6.5%       10      131     1009     0.70
overhead  0.010     global        92%       1%     7.3%       10      142      998     0.86
overhead  0.010     resident      88%       4%     7.9%       10      142     1010     0.76
overhead  0.040     global        93%       0%     6.3%       10      153     1001     0.85
overhead  0.040     resident      96%       1%     2.3%        7       91      974     0.57
grazing   0.004     global        92%       1%     7.2%       10      163     1001     0.83
grazing   0.004     resident      89%       4%     6.6%       10      154     1012     0.69
grazing   0.010     global        92%       1%     7.1%       10      162     1001     0.83
grazing   0.010     resident      88%       4%     7.9%       10      160     1011     0.74
grazing   0.040     global        92%       1%     7.0%       10      155      999     0.85
grazing   0.040     resident      95%       2%     3.6%        7       96      916     0.57
```

`pinned%` is the discrete-LOD symptom: a patch every vertex of which sits at its parent's
shape. `at own%` is the harmless converse.

At 0.04 radii the effect is decisive — level 10 to 7, 153 patches drawn to 91, pinned 6.3%
to 2.3%. The symptom halves and the frame gets 40% cheaper. Near the ground it is a wash on
the symptom (7.4 to 6.5, but 7.3 to 7.9 at 0.01), so **this does not close the ground-level
seam**, which is consistent with the range ratios above being what drives that.

`behind` improves everywhere, 0.86 to 0.70 and 0.85 to 0.57: the selection is simply more
accurate about where the ground is.

One cost: near the ground it holds about ten more tiles (1009–1012 against 998–1001 of 1024).
Children are discovered on the global shell and given tiles, then some are judged out of
range on their own bounds and covered by the parent, keeping a touched tile they never draw.
At 0.04 the balance runs the other way.

### The risk that had to be checked

Tighter bounds are less conservative and could in principle part a boundary. The morph needs
`nearest_distance` to remain a lower bound on every vertex; vertices, bilinear samples and
morphed positions all stay inside the tile's texel extrema, which `test_height_tile_range`
asserts through the encode/decode round trip.

`test_resident_terrain_morph` originally looked straight down at one altitude, which culls
exactly the grazing boundaries where a level meets the next — the ones a tighter bound could
part. Widened to three altitudes across overhead and grazing views: **23,120 boundary
vertices checked, 0 morph violations**, against 1,156 before.

## What to do next, in order

1. **The range ratios.** Replace the data-driven table's implicit step with CDLOD's ×2
   geometric series, using the measured errors to place the series rather than to define
   each step. This is the one that matches the reported symptom.
2. **Item 5**, by duplicating the centre-cross vertices as described.
3. **Item 1**, budget redistribution by view importance, which only bites above bias 0.

## How the measurements were made

- Selection, morph and error harnesses built against the production `TerrainTier`,
  `patch_bounds` and `MinorPlanetTerrain` — not re-implementations of the rules.
- The ancestor comparison ran both walks over the same live cover map each frame.
- The height-bounds comparison generated each tile once and cached it, so both runs saw an
  identical surface, and discarded the first 250 frames of each flight as streaming warm-up.
- Full rebuild and 18/18 ctest, GPU validation included, at each commit.
