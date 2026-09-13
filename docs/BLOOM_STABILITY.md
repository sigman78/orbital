# Bloom and sun-occlusion stability

The bloom filter previously combined a horizontal blur with a quarter-resolution
reduction, sampling only one source row per output row. Thin occluders and bright
points could fall between those samples. At larger frame heights the fixed nine
taps were spread farther apart; a small source could therefore produce separated
lobes or a grid instead of a continuous halo.

Bloom now integrates the complete source footprint before reducing resolution,
with fractional area weights for odd dimensions and the soft threshold applied
per source texel. Two normalized Gaussian passes then sample adjacent bloom
texels. The tap count grows with the halo radius; its size relative to the picture
is retained. The sequence is prefilter A, horizontal B, vertical A, with an explicit
read-before-write barrier when reusing A. No additional bloom image is allocated.

The broad sun glare, ghosts and starburst had a separate sensitivity: twelve binary
depth samples controlled their global intensity. They now share a 1×1 visibility
pass with 256 fixed disc samples. Each sample interpolates binary visibility from
four depth texels, rather than thresholding interpolated depth, and follows the
projection jitter. The dense estimate is computed once per frame, including when
bloom is off. It does not add temporal lag or change the scene's temporal AA.

## Reproduce

`--belt-sun-view` starts behind the gas giant's tilted belt, with the centre ray
crossing the ring toward the sun. It overrides the initial bookmark/tour/galaxy
view, leaves free flight available, and works with frozen time or live motion.

```powershell
./build/release/orbital.exe --belt-sun-view --time 0
./build/release/orbital.exe --belt-sun-view --time 0 --fullscreen-at 30
python tools/check-bloom.py
```

The GPU check requires the release Windows demo, Slang, numpy and Pillow. It runs
an isolated copy under `.scratch/bloom-regression`, with synthetic input injected
into the production bloom shader. It checks all sixteen source-pixel phases,
monotonic halo profiles, odd dimensions and a real fullscreen transition. A second
fixture exercises the production sun-visibility helper against an opaque
half-plane moving in fifth-pixel steps. Production assets and binaries are not
modified by the check.

## Validation (GTX 1080 Ti)

- Release build, all nine CTest suites, and all 29 independent shader helpers pass.
- GPU bright-point tests pass at 1600×900, 1603×903 and fullscreen 3440×1440.
  Integrated halo energy is identical across all sixteen phases at the divisible
  sizes; the odd-size spread is 0.15%, including screenshot quantization. All
  horizontal and vertical marginal profiles have no significant secondary peaks.
  A negative control running the old filter misses 12 of the 16 bright source
  pixels entirely, confirming that this fixture detects the original defect.
- The synthetic half-plane visibility rises from 0.497 to 0.617 over one pixel;
  each 0.2-pixel step changes it by at most 0.026, below the old single-tap 0.083
  jump. This is a spatial filtering test, not a claim that all motion is flicker-free.
- Matched Dawn captures cover default size, odd dimensions and the fullscreen
  transition. A fixed-time sun-through-belt sequence covers frames 120–127 (one
  complete jitter cycle), plus fullscreen. In a 30–180 pixel annulus around the
  sun, the standard deviation of mean RGB display brightness falls from 1.917
  to 0.978 levels (49% lower); mean per-pixel temporal deviation falls from 2.077
  to 1.158 levels (44% lower). Geometry/TAA variation remains. The old shader was
  run in an isolated copy with an identity pass adapting it to the new pass order.
  Local captures, logs and `belt-temporal.json` are under `.scratch/bloom-fix/`.
  The new halo is brighter where the old interpolated-depth threshold incorrectly
  classified partially open samples as blocked.

The separate analytic planet-occlusion gate and existing geometry/temporal-AA
behavior are unchanged. Remaining motion artifacts should be isolated from bloom
and lens visibility rather than hidden with a longer temporal filter.

## Post shader organization

Each independent pass now has a dedicated entry point: `post/bloom.slang`,
`post/composite.slang`, `post/sun_visibility.slang`, `post/meter.slang`,
`post/present.slang`, and `aa/fxaa_pass.slang`. Bloom alone retains a three-stage
mode selector, with its values shared with C++ in `post/bloom_shared.h`.
`post/hdr.slang` shares resolved-HDR sampling; `post/sun_occlusion.slang` owns the
visibility estimate independently of the lens appearance helpers.

The composite still combines aberration, bloom addition, lens effects, exposure,
tone mapping, vignette and grain in one draw. The split changes no image allocation,
pass order or draw count. Sun visibility now has its own pipeline instead of using
the bloom pipeline; other post pipelines already existed. The old `post.slang`
entry point and broad `PostMode` enum are removed.

The GPU regression produces the same source-phase energy and visibility results
after the split. Fixed-time comparisons cover the belt, fullscreen, Earth with
FXAA, and the tour. Belt pixel differences average 0.0032 display levels, matching
an unchanged-baseline repeat (0.0032); Earth/FXAA and tour differ by at most one
level, averaging below 0.0002. These are not bit-exact scene captures; there is no
intended appearance change. Captures are local under `.scratch/post-split/`.
