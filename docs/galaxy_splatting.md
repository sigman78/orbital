# The Milky Way as Gaussian splats

Working notes on representing the galactic band without a bitmap: what the
renderer does, how the fit works, what was tried, what the measurements say,
and where the promising direction is. Dates are September 2026; the tool is
`tools/bake-splats.py`, the pass is `shaders/sky/galaxy.slang`, the experiments
live in `.scratch/milky-way/` (ignored by git). The decisions log holds the
short version of each step; this file holds the reasoning.

## Why splats

Every map-based route was rejected on review. ESO's panorama at 1024 and
2048 wide, even as cubic B-spline coefficients with the stars removed, read
as blotches: a map that small holds the band's large shapes but none of its
character, and every treatment of it was a treatment of blur. An analytic
galaxy model (discs, bar, arms, clumped dust, marched from the Sun) rendered
as a grey wash and was a fiction anyway. The reviewer's proposal was to take
a star-free reference and compress it into a formula or a cloud of splats.

A cloud of Gaussian splats on the sphere is the formula: a few thousand
parameters, evaluated per pixel, so there is no texel lattice at any zoom,
no star residue, and the file is a couple of hundred kilobytes. Detail comes
from more or narrower splats, not from resolution.

## The representation

Each splat is an anisotropic Gaussian on the unit sphere:

- a centre direction `c` (galactic xyz, x toward the centre, z the north pole),
- two widths `sigma1, sigma2` in radians along a tangent frame `axis1`,
  `axis2 = cross(c, axis1)`, rotated by an angle,
- a signed linear RGB amplitude. Negative splats carve the dust lanes out of
  positive ones; about a third to a half end up negative.

The shape uses azimuthal equidistant coordinates: the angle to the centre
`alpha = atan2(sin, cos)` scales the tangent-plane direction, `u = dot(g,
axis1) * alpha / sin(alpha) / sigma1`, `v` likewise, value `exp(-(u^2 +
v^2) / 2)`. It is right anywhere on the sphere and has no antipode ghost,
which the plain chord projection had. The radiance is the plain sum (no
sorting, no alpha, the accumulated-summation model of GaussianImage). Each
splat carries a reach, `cos(3.5 * max(sigma))`, beyond which the pass skips it.

Record file (`assets/materials/milky_way_splats.bin`): 16-byte header
(`SPLT`, count, record size 64, cell-table length), then per splat four
float4 (`centre.xyz, sigma1 | axis1.xyz, sigma2 | rgb, cutoff | unused`),
then the cell table. The records are ordered by energy (amplitude times
area) so the renderer can draw the first n.

## The render

`shaders/sky/galaxy.slang` sums the splats per pixel at a fraction of the frame
(a quarter by default, half or full from the Sky panel) into an rgba16
target; the background samples it bilinearly. A 0.6-degree splat is about 11
pixels at 1080p, which the quarter target resolves; the shipped 0.3-degree fit
is about 5.5 pixels, 1.4 quarter-resolution samples per sigma, so its finest
splats are softened at the default and the half setting shows them.
A band contrast exponent (Sky panel, default 1) is applied to the summed
radiance about the fit's unit before the brightness, so the halo can stay
faint while the core comes up. Alpha carries a dust depth, how much the dark splats take from the bright,
which the background redistributes into filaments by a three-octave gradient
noise fBm (amplitude, feature size, lacunarity and gain in the Sky panel; defaults 2, 1.4 degrees, 3 and 0.7, set on review).

Cost followed the splat count linearly (1.9 microseconds a splat at 1080p in
a sky-filling view), so the bake buckets the splats by the cells of a 7.5
degree longitude/latitude grid their reach cones meet, a table after the
records, and the pass sums only its pixel's cell. Measured in the
sky-filling view at 1080p on a 2.16 ms frame:

| Fit | Full-res pass | Quarter-res pass | Quarter + cell table |
|---|---|---|---|
| 256 at 1.2 deg | +3.7 ms | +0.39 ms | |
| 512 at 1.2 deg | | +0.98 ms | +0.47 ms |
| 1024 at 1.2 deg | | | +0.72 ms |
| 1024 at 0.6 deg | | | +1.00 ms |
| 2048 at 0.6 deg | | | +1.54 ms |

The cost is set by the cell lists' length, not the count: the progressive
fits below hold half the list entries of the plain ones at the same count.

Two render-side lessons from playtesting: a star-cloud grain effect gated
on screen derivatives switched on toward the frame's corners and everywhere
at higher resolutions (speckle), and since the camera has no field-of-view
zoom it never did what it was for; it was removed. The dust noise's first
wiring converted degrees as if the lattice took cycles, ran at three pixels
a cell and hid two of its three octaves; value noise also drew its cube
grid, hence gradient noise.

## The fit

`tools/bake-splats.py`, PyTorch, CUDA when present. A GTX 1080 Ti fits the
2048-splat ladder in about twenty minutes; the peer machine's RTX 4080 fitted
the whole 4096 lineage in minutes. The LAN box's 20 cores take an hour for a
1024 fit.

Target preparation:

- `--kind map` (the default; ESA's Gaia DR2 sky in colour, CC BY-SA 3.0 IGO,
  credit ESA/Gaia/DPAC): the integrated starlight itself, no point stars, no
  patches. The shipped asset is fitted to the 1800x900 cartesian JPEG (SHA-256
  `58536692...6224`), upsampled to the 2048-wide target, decoded as gamma 2.2
  with a half-percentile floor and normalised so the 99.5th percentile is 1.
  Its outermost columns and rows are a lighter border (mean 35 against 23
  beside them, 17 against 4 at the top and bottom); fitted, that became a
  line of narrow splats at the 180-degree seam (138 within 3 degrees of it
  where a uniform share is 68), visible in the demo, so the preparation
  replaces the border with its neighbours.
  ESA's larger files (up to 8000x4000) are in the Hammer projection only; the
  bake reprojects those by the projection's forward formula. The map is a
  logarithmic stretch of flux, so `--decades D` can undo it (`flux =
  10^(D(v-1))`): at 0 the sky keeps a bright star-density halo; at 3.5 the pole
  falls to a fraction of a percent of the bulge. `--blur` low-passes the map
  before fitting so a fit at a fine floor chases the mean, not the stars; use
  about 1.5 times the floor. The shipped asset uses neither: its halo is the map's.
- `--kind photo` (ESO's panorama eso0932a, CC BY 4.0): median filters at two
  scales remove the point stars, the blacked-out patches (the photographer's
  horizon) are masked out of the loss and of the floor, the floor is the third
  percentile of valid sky.

Loss: the area-weighted squared log ratio over a floor of a hundredth of the
bulge. A square-root loss, even with the dark sky weighted fivefold, kept
forgiving a haze of a hundredth over the whole sky (the tails of the broad
halo splats); the log ratio prices that the same as a wrong bulge. Loss values
compare only between fits of the same source, width and preparation.
`--core-weight` adds a square-root brightness weight to hold the compact
bright core, which the area weight alone lets the fit halve; it is untested
with the staged recipe and off by default.

Parameters: widths as logs clamped between `--sigma-min` (0.3 degrees) and 45
degrees, the aspect clamped to `--aspect-max` (2.5); Adam with a cosine
schedule per stage; after every step the latent widths are projected back to
their clamped values so a splat sitting on a clamp can leave it.

### Plain fit (`--no-progressive`)

Centres drawn from the target's brightness, all splats from the start, random
widths 2 to 12 degrees. It works, but capacity never moves: the lanes, where
the error is, get no splats; 2048 splats looked like 1024, and 4096 was worse
than 2048 (loss spikes, colour artifacts at the bulge). Only 0.2 percent of
its splats ever reached the width floor: a small floor is permission, not an
instruction.

### First allocation attempt (retired)

Half the splats at the start placed by the image gradient, the rest allocated
every 500 steps at directions sampled by the per-pixel squared log error,
seeded with the residual's colour, plus a falling width floor. It cut the
error 14 percent on a 512-splat smoke test, but on the Gaia map the largest
per-pixel errors are individual bright stars and speckle, so the new splats
fit stars as dots and lanes as hairs, and even with a low-pass, an aspect cap,
brightness-weighted sampling and broader seeds the halo stayed covered in
small confetti splats. Sampling the error per pixel is the flaw: coherent
structure has to be told apart from noise before the allocation, not after.

### Staged adaptive fit (current default; the peer machine's recipe)

Developed on the RTX 4080 machine (its reports are in
`.scratch/milky-way/peer/`, starting with `CANDIDATES.md`) and ported into
`bake-splats.py` as the `--progressive` path:

- A coarse cloud of 256 splats (centres by brightness, widths 2 to 12
  degrees) is fitted first with fast rates (centres 3e-3, widths and rotation
  1e-2, colour 1e-2), 600 steps.
- The cloud then grows in stages, 512, 1024, 1536, 2048, 3072, 4096 (400,
  600, 800, 1000, 1200 and finally `--steps` updates). At each growth the
  signed log residual of the current fit is averaged over 4x4 blocks of the
  target before squaring, which suppresses isolated pixels and speckle and
  favours coherent lanes; new centres are sampled from that score (area
  weighted, clipped at its 99.7th percentile, without replacement).
- New splats start narrow (1.25 to 2.75 floors on the minor axis, 1.1 to 1.8
  times that on the major, random rotation) with zero amplitude, so growing
  preserves the image exactly and the optimiser learns each newcomer's sign.
- Every later stage refines all splats together at a fifth of the coarse
  rates (5e-4, 3e-3, 1e-3), cosine to 5e-5, with a fresh optimiser.
- A `.pt` checkpoint is written beside every export and `--resume` continues
  from one, so a lineage can be extended stage by stage; `--compact` also
  writes the same records with a 32x16 table.

Measured by the peer machine on the same prepared target from the exported
records with their reach cutoffs (display RGB RMSE on a 0 to 255 scale, area
weighted; band = latitude within 15 degrees, halo = 15 to 60, polar = above
60; "candidates" = mean cell-list length per pixel, a work proxy, not a frame
time):

| Fit | Bytes | Loss | Band RMSE | Halo RMSE | Candidates/pixel |
|---|---:|---:|---:|---:|---:|
| Plain 2048, 0.4 deg | 483,340 | 0.019172 | 12.01 | 4.00 | 105.5 |
| Plain 4096, 0.4 deg | 886,832 | 0.020866 | 13.32 | 3.98 | 194.1 |
| Adaptive 1024 | 205,296 | 0.018875 | 12.37 | | 35.9 |
| Adaptive 1536 | 256,368 | 0.017502 | 11.40 | | 40.7 |
| Adaptive 2048 | 305,072 | 0.016663 | 10.83 | 3.71 | 45.0 |
| Adaptive 3072 | 394,276 | 0.016122 | 10.37 | | 51.3 |
| Adaptive 4096, 0.4 deg | 482,040 | 0.015700 | 9.96 | | 57.1 |
| Adaptive 4096, 0.3 deg ("v4") | 479,616 | 0.015318 | 9.72 | 3.58 | 56.8 |
| v4 plus 1,500 refinement updates ("v5 control"; shipped with a 32x16 table, 391,248 bytes) | 479,508 | 0.015227 | 9.62 | 3.57 | 56.9 (48x24) |

The adaptive 2048 beats the plain 2048 by 9.9 percent in band error at 37
percent fewer bytes and 57 percent fewer candidates per pixel; the 4096 at a
0.3 degree floor is 19 percent better than the plain 2048 in band error at
the same size, its median narrow width 0.50 degrees against 2.7. At 4096, 24
percent of splats sat on the 0.4 floor, so the floor had become a real
constraint and 0.3 bought another 2.4 percent (with extra steps, so not a
clean attribution). Fine source texture is still smoothed and individual
ellipses remain visible at close zoom: this is a better approximation, not a
reproduction.

## What was tried, in order

| Fit | Verdict |
|---|---|
| ESO, 256 and 512, 1.2 deg, sqrt loss | First Milky-Way-like result; sky lifted by halo tails |
| ESO, 1024 and 2048, 1.2 deg, log loss | Haze gone; 2048 no better than 1024, the floor limits |
| ESO, 1024 and 2048, 0.6 deg | Lanes the 1.2 fit blurred, same cost; was the default |
| ESO, 1024 and 2048, 0.25 deg | 1024 never used it (narrowest 0.38); 2048 a little more, streaky |
| Gaia 2k cartesian, 1024, 0.6 deg | Better target, halo captured, lanes as scratches |
| Gaia 8k (reprojected), 1024 at 0.6, 2048 at 0.4 | Best band and clouds; grey speckle sky without decades |
| Gaia 8k, 2048, 0.4 deg, decades 3.5 | Sky right; lanes still scattered ellipses, 4:1 brush strokes |
| Gaia 8k, 512/1024/2048, per-pixel error allocation | Sharper and half the render cost; splats chase stars, lanes as hairs |
| Same with 0.3 blur, aspect 2.5, brightness-weighted sampling, broad seeds | Best lanes of that line, hairs gone, halo covered in confetti; retired |
| Peer: plain 2048 and 4096 on the cartesian 2k map | 4096 worse than 2048; count alone does not help the plain fit |
| Peer: adaptive 256 to 2048 in stages (v1, v2) | Band error down 9.9 percent at 37 percent fewer bytes; the breakthrough |
| Peer: adaptive to 3072 and 4096, then 0.3 floor (v3, v4) | Shipped asset; 19 percent better than plain 2048 at the same bytes |
| Peer: fixed-count split/recycle (v6) | Leave-one-out pruning plus splits and residual-shaped re-seeds: 0.37 percent better band than an equally trained control; not worth the peak-work rise |
| Peer: multiscale gradient loss (detail refine) | Ties its control within 0.1 percent; the gain was the gentler learning rate, not the extra term |
| Peer: cell-work penalty in the loss (cost refine) | Moderate penalty: 6.3 percent less mean work, 2.6 percent fewer bytes, band error unchanged; short of its 10 percent target, kept as an efficiency option |
| Peer: GaussianImage++ style filter release, spatial spread of new centres, per-primitive filters | Blurrier band or higher peak work; none promoted |
| Peer: 32x16 cell table for the same records | A third fewer bytes for a fifth more candidates per pixel; the renderer reads the grid size from the file, frame time unmeasured |
| Gaia 2k, full ladder to 4096 with the border columns and rows replaced | Running: the seam fix |

## What is promising

1. The staged adaptive recipe is the baseline now; every peer experiment on
   top of it (recycling, edge loss, cost penalty, spread, filters) moved the
   needle by under one percent. The peer's own conclusion: the next lever is
   representation capacity at fine scales (the floor under the stable
   schedule), not more loss terms.
2. Preparation, untested with the staged recipe: undoing the map's log
   stretch (`--decades`) for a photograph's dynamic range, and the
   square-root brightness weight (`--core-weight`) to hold the core. The
   shipped asset's halo is the map's stretched one, which is why the panel's
   brightness default fell to 0.1 and a contrast exponent was added; a fit
   on an unstretched target might not need either.
3. Count and floor: 4096 at 0.3 degrees is where a quarter of the splats sat
   on the floor; a 0.25 floor with more steps, or 6144 splats, is the direct
   test. Render cost follows the cell lists, and adaptive lists are short.
4. Measure the frame: every peer number is a work proxy. The pass cost of
   the shipped asset in the sky-filling view is still to be measured here.
5. Not promising: finer floors alone on the plain fit, more plain steps,
   per-pixel error allocation, top-K normalised blending, uniform spreading
   of new centres.

Open for review: the share-alike licence of the Gaia map against ESO's plain
attribution, and where the cartesian 2k JPEG came from (its URL belongs in
the manifest).

## Running it

```
python tools/bake-splats.py --source .tools/asset-sources/esa_gaia_dr2_allsky_brightness_colour_cartesian_2k.jpg \
    --output <name>.bin --preview <name>.png --compact
python tools/bake-splats.py --from-records <name>.bin --output build/release/assets/materials/milky_way_splats.bin
```

The first runs the full ladder to 4096 splats at a 0.3 degree floor (about
6,100 updates; `--counts` and `--stage-steps` set a custom ladder, `--resume
<name>.pt --counts 6144 --stage-steps 1500` extends one). The second builds
the cell table and installs it for a playtest; the demo reads the file from
`build/release/assets/materials/`, which only a build refreshes from
`assets/materials/`. `tools/check-splats.py` validates a record file,
`tools/analyze-splats.py` reports its width statistics,
`tools/splat-table-tradeoffs.py` the cost of alternative grids, and
`tools/render-splats.py` draws the exported records with their cutoffs.
`tools/import-assets.ps1 -Only milky_way` refits the shipped asset with the
script's defaults and writes the manifest. The 4080 handoff in
`.scratch/milky-way/HANDOFF.md` has the experiment list.
