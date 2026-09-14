# Stable TAA coordinates and independent splat coverage

Implemented the first two stages of [the TAA review](TAA_REVIEW.md).

The resolve reconstructs current HDR with five-fetch Catmull–Rom at output position plus current jitter. Neighborhood samples use the same coordinate convention. Reprojection removes previous projection jitter before accessing history, which now lives on the stable output grid. This removes jitter from the motion-dependent blend weight. TAA off uses zero projection jitter. Temporal mode changes, FOV changes and explicit camera cuts invalidate history, in addition to existing resize and large-jump handling.

The former R8 splat mask is now RGBA16F metadata: R stores coverage-weighted linear depth and A stores coverage. Its pass uses actual billboard footprint/alpha, premultiplied blending and read-only depth testing. Opaque scene depth is no longer overwritten. The pass runs with TAA either on or off because lens visibility also needs it. Sun visibility multiplies binary opaque visibility by one minus accumulated splat coverage, before bilinear filtering and disc integration.

Overlapping billboard depths blend in the same draw order as billboard color and are normalized by coverage for temporal reprojection. This approximates mixed-depth pixels; it does not replace the later coverage-aware history/atmosphere work. Splat history exemptions and luminance blending remain. Contrast tuning, object motion vectors and separate atmosphere accumulation are outside these first two stages.

## Validation

- Release build, all nine CTest suites and all 29 independent shader-helper checks. Camera tests cover continuous motion versus a tiny explicit cut and invalid bookmark requests.
- `python tools/check-taa.py`: production TAA over a stationary band-limited scene, eight jitter phases, on/off. No displacement was measurable in the 8-bit captures (tolerance 0.04 pixels). A negative control substituting the old resolve measured 0.4403 pixels between two phases, proving the fixture detects the original wobble.
- The GPU test checks visibility at coverage 0, 0.25, 0.5 and 1 with TAA on and off; all are within 0.01 of expected transmittance. This isolates the visibility helper, not arbitrary overlapping transparent layers.
- `python tools/check-bloom.py`: source-phase energy, halo continuity at 1600×900, odd 1603×903 and fullscreen 3440×1440, plus moving half-plane visibility pass. One initial launch failed while another GPU harness ran; the complete serial rerun passed.
- Real scene captures in `.scratch/taa-fix/`: sun-through-belt with TAA on/off, giant bookmark and fullscreen. The synthetic fixture demonstrates coordinate stability; it is not a subjective 30-fps playback assessment or a promise to eliminate residual belt shimmer.

## Cost and remaining work

The metadata image grows from one to eight bytes per pixel: approximately 9.6 MiB extra at 1600×900 and 33.1 MiB at 3440×1440. There is no extra texture descriptor or history image. Current reconstruction uses five filtered fetches instead of one load; neighborhood fetches are bilinear. Metadata blending costs bandwidth, and its draw now also runs with TAA off. Sun integration adds coverage reads. Matched fullscreen profiling is recorded below.

Follow up with history validation/coverage policy, asteroid contrast and multilayer atmosphere reprojection as described in the review. The original full-quad occlusion behavior should not be restored to address residual shimmer.

## Performance follow-up (GTX 1080 Ti)

Belt bookmark (`--bookmark 5`), frozen time 0, default quality and SMAA, no HUD, 500 frames per run; discard the first 100. Fullscreen transitions from 1600x900 to 3440x1440 at frame 30. Sequential runs avoid competing app instances. CSV/logs are in `.scratch/taa-performance/`.

| Case | Median CPU submit + wait | Median GPU |
| --- | ---: | ---: |
| Current, 1600x900 | 6.11 ms (~164 fps equivalent) | 5.44 ms |
| Current, fullscreen, two runs | 15.28-15.37 ms (~65 fps equivalent) | 14.46-14.63 ms |
| Saved pre-TAA-fix build, fullscreen, two runs | 15.03-15.54 ms (~64-67 fps equivalent) | 14.20-14.71 ms |
| Current, fullscreen, TAA off | 15.27 ms | 14.49 ms |
| Current, fullscreen, dust off | 13.34 ms | 12.54 ms |

The saved comparison is `.scratch/post-split/before`, which already has the bloom fix but precedes the post shader split and these TAA changes. It is not an isolated one-commit TAA comparison. Across these runs, no large new fullscreen regression is reproduced. Differences are within a few tenths of a millisecond and overlap run-to-run variation.

Current fullscreen GPU time is dominated by the surface block (5.85-6.04 ms) and atmosphere/dust/splat-metadata block (5.55-5.67 ms). Postprocessing is 2.10-2.40 ms; culling/shadows/maps approximately 0.61 ms. Disabling dust reduces the combined atmosphere block by about 2.2 ms. Disabling TAA reduced post time by about 0.45 ms in the first comparison, but changes scene sampling too and did not produce a comparable whole-frame improvement. Do not interpret this as the isolated cost of the fix.

Fullscreen has 3.44 times the default window's pixels. The present build reproduces both roughly 160 fps windowed and roughly 65 fps fullscreen. This explains the two performance levels under matched current settings; it does not establish the user's remembered earlier fullscreen resolution, scene time or quality. A vsync-enabled fullscreen control remained near 15.3 ms CPU / 14.5 ms GPU, so this reproduction is not simply a 60 Hz presentation cap.

### Exact pre-bloom revision comparison

Built unmodified revision `b0ca455` in `.scratch/pre-bloom-source`, with the same compiler/Vulkan dependencies. Benchmarked its executable/shaders against current using the same runtime assets and the same fullscreen belt command described above. Sequential order: pre-bloom, current, pre-bloom repeat.

| Case | CPU submit + wait | GPU | GPU post |
| --- | ---: | ---: | ---: |
| Pre-bloom, run 1 | 15.297 ms | 14.595 ms | 2.002 ms |
| Current | 15.436 ms | 14.522 ms | 2.150 ms |
| Pre-bloom, run 2 | 15.375 ms | 14.550 ms | 1.968 ms |

Both revisions run at approximately 65 fps equivalent in this matched fullscreen belt workload. Current postprocessing adds about 0.15-0.18 ms relative to these pre-bloom runs; whole GPU time is essentially unchanged. These compare all intervening changes, not bloom alone. The archived initial pre-bloom fullscreen result of 10.154 ms CPU / 9.453 ms GPU / 2.203 ms post was bookmark 4 (Dawn), so it must not be compared directly with the belt bookmark. No 160-to-60 fps regression is reproduced across the bloom/TAA revisions under these matched settings.

## 2026-09-14: high-contrast asteroid flicker

A stationary belt at 3440x1440 after a fullscreen transition showed persistent TAA flicker; toggling TAA off/on did not cure it. Resize already invalidates history. The resolve applied Catmull-Rom colour reconstruction to the history alpha/depth channel and compared that filtered depth with a single current surface. At silhouettes, interpolation and negative filter lobes produce depths that do not represent either surface, causing false history rejection as jitter changes coverage.

Depth rejection now uses the minimum/maximum of raw history depths in a 3x3 neighborhood around reprojection, allowing a one-pixel coverage shift between jitter phases. Colour reconstruction and clipping remain unchanged. History is still rejected when current predicted depth lies outside the neighborhood range plus the existing tolerance. This deliberately accepts mixed-depth edge neighborhoods; moving silhouettes still need visual assessment, and object motion vectors remain future work. Nine point depth reads are added for non-sky, non-splat pixels with valid history; no targets, descriptors, or passes are added.

Diagnostic captures are in `.scratch/taa-fullscreen/`: bookmark 5, simulation time 0, 1600x900 window, fullscreen at frame 20, TAA on/off, captures after 120 and 121 frames. At native 3440x1440, pixels whose maximum RGB channel difference exceeds 16/255 were: original 8,848; TAA off 3; depth rejection disabled 209; colour clipping disabled 8,806; final depth neighborhood 231. Final mean absolute RGB difference was 0.05354/255 versus 0.11323/255 originally. These two-phase captures isolate a substantial cause, not every temporal artifact or every jitter phase. The temporary diagnostic shader binaries were restored before building the final shader.

Validation: shader compilation and SPIR-V validation passed; `python tools/check-taa.py` measured zero displacement across all eight phases with TAA on/off and passed fractional sun-visibility checks. A 120-frame native-fullscreen belt pan/stop run passed Vulkan core/synchronization validation (`.scratch/taa-fullscreen/validation/`). No performance measurement was made for the extra depth reads.
