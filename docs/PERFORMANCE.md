# Render performance tracking

CPU allocation and startup-storage measurements are recorded separately in [STORAGE_PROFILE.md](STORAGE_PROFILE.md). They do not replace the GPU scene baselines below.

Run the same fixed workload before and after renderer changes. Keep the summary JSON/Markdown for each accepted milestone in `docs/performance/`; raw per-frame CSVs and launch logs stay under `.scratch/performance/`. Compare with both the preceding milestone and the original baseline to expose cumulative costs.

```powershell
./tools/build.ps1
python tools/benchmark-suite.py --label my-change --output .scratch/performance/my-change --baseline docs/performance/taa-stage-1-2.json
```

Additional `--baseline` arguments compare against more milestones in the same report. The output directory must be new. Review `summary.md`, then copy `summary.json` and `summary.md` into `docs/performance/` using a descriptive milestone name. Baselines are explicit files and are never automatically replaced or promoted.

The default workload is all six bookmarks (Earth, giant, Moon, Mars, Dawn and belt) plus the sun-through-belt view, each at 1600x900 and native borderless fullscreen. It freezes seed/time/exposure adaptation, explicitly selects TAA + SMAA, dust, disc LOD, splat mode, tone curve and galaxy mode, uses the default quality tier, disables vsync/HUD, and launches runs sequentially. Three complete rounds of 360 frames discard the first 120 frames of every launch, including the fullscreen transition at frame 30. This is 42 launches and takes several minutes. Simulation animation and camera motion are deliberately outside this initial suite.

Close other demo instances and GPU-heavy applications. Use the same display, power mode and build configuration. Do not rebuild or edit runtime shaders/assets during a run. The script records GPU/driver details (driver query when `nvidia-smi` is available), OS/CPU, actual drawable dimensions, settings, Git revision/dirty status and executable/shader/asset hashes. Dirty-source baselines are labeled as such; binary hashes identify the measured artifact, and a source revision alone does not prove which executable was built. Cached asset changes require a new compatible baseline. Defaults not exposed by CLI, including quality internals, remain part of the versioned implementation under test.

Comparisons reject mismatched hardware/software environment, settings/protocol, assets, scene sets or dimensions rather than silently treating them as regressions. Driver changes intentionally start a new comparable series. `--scenes belt dawn` is useful for local investigations, but that subset cannot be compared directly against a full-suite baseline. The same applies to changed frame/repeat counts or window dimensions.

## Timing definitions

| Metric | What it measures |
| --- | --- |
| CPU + wait | Renderer call including GPU wait/acquire/submit; not isolated CPU work or full application-frame wall time |
| CPU preparation | Frame/instance preparation |
| GPU total | Timestamp span covering the rendered frame |
| Cull/maps | Culling, shadows and belt-map generation |
| Surface | Galaxy/background, stars, opaque surfaces, billboards and clouds |
| Atmosphere group | Planet atmospheres, belt dust/disc, motion streaks and splat metadata |
| Post | TAA, bloom, sun visibility, composite, spatial AA, periodic metering and presentation |
| Meter | The exposure meter pass and its readback copy, on the frames it runs (every sixteenth); zero otherwise. Inside Post, reported separately in the CSV as gpu_meter_ms |

These are existing GPU **pass groups**, not individual shader timings. They add no new rendering instrumentation. Split a group into finer timestamps when a regression points there. Independent medians do not necessarily sum exactly to the median total. All columns are milliseconds; use GPU time to identify rendering costs instead of interpreting a rounded FPS counter as isolated GPU performance.

Each case records the median of the three run medians, median of run p95 values and min/max run medians. Every raw frame is retained locally. An increase must exceed both 5% and 0.2 ms to receive a flag. Overlapping run-median ranges are labeled a noisy increase; separated ranges are labeled a regression candidate to reproduce. These are investigation thresholds, not statistical confidence intervals or automatic CI failures. Small individually unflagged changes remain visible in the deltas against the original baseline.

Analysis tests: `python tests/benchmark_suite_tests.py`. No GPU is needed for those tests.

## Initial baseline

`taa-stage-1-2` records the current TAA coordinate and fractional-occlusion implementation, before further performance optimization. It includes uncommitted rendering changes and is explicitly marked dirty. See the JSON for exact binary/shader/asset identity. Earlier ad hoc bloom/TAA measurements remain in [TAA_COORDINATES.md](TAA_COORDINATES.md); their workload differs and they are not interchangeable with this suite.

Recorded baseline: [timing table](performance/taa-stage-1-2.md), [machine-readable report](performance/taa-stage-1-2.json).

## Initial quick-win experiments

Two sequential runs per setting at the frozen fullscreen belt bookmark (500 frames, first 120 discarded), using the same executable. These are exploratory variants, not interchangeable with the default suite protocol. Captures and CSVs are under `.scratch/performance/quick-wins/`.

| Setting | GPU median range | Post median range |
| --- | ---: | ---: |
| default | 14.381-14.606 ms | 2.405-2.481 ms |
| fxaa | 14.267-14.302 ms | 2.078-2.095 ms |
| splat4 | 14.136-14.447 ms | 2.492-2.502 ms |
| both | 13.960-14.049 ms | 2.088-2.100 ms |

`fxaa` replaces SMAA with FXAA; `splat4` raises the mesh-to-billboard cutoff from 2.5 to 4 pixels; `both` combines them. Both together save roughly 0.4-0.6 ms (about 3-4%) in this scene. FXAA post cost falls consistently, while the larger cutoff alone is closer to run-to-run noise. Static captures retain the broad scene appearance, but moving-image shimmer and small-rock detail need evaluation before changing defaults. No renderer defaults were changed.

Further candidates to measure are conservative atmosphere screen bounds (skip pixels outside projected shells) and an early TAA-off copy path. These have no intended quality loss but no measured benefit yet; the copy path only benefits TAA-off. Reduced dust sample counts may save more, but require temporal-noise testing rather than assuming equal quality.

## Cumulative check after the lens, HDR and exposure work (2026-09-15)

One suite run over all seven scenes at revision `0db771e` (the lens flare stack, dirty glass, the night-side and tone fixes, HDR output, the reworked auto exposure and the telescope zoom, all merged), recorded as [session-followups](performance/session-followups.md) ([JSON](performance/session-followups.json)): the first all-scene baseline since `taa-stage-1-2`, for later comparisons. The belt cases are compared with `gpu-scopes-after`, the last belt-only milestone, in [session-followups-belt](performance/session-followups-belt.md); the suite's asset hash check was waived for that comparison because the asset set gained `lens_dirt.png`, which no benchmark scene reads.

| Belt case | GPU before | GPU after | Change | Where |
| --- | ---: | ---: | ---: | --- |
| window 1600x900 | 5.277 ms | 5.642 ms | +0.365 ms (+6.9%) | post +0.156 ms (the flare pass and the wider meter); surface and atmosphere within threshold |
| fullscreen 3440x1440 | 14.528 ms | 15.408 ms | +0.880 ms (+6.1%) | surface +0.468 and atmosphere +0.449 ms flagged; post +0.051 within threshold |

The window increase is the expected price of the flare pass and the meter. The fullscreen surface and atmosphere increases are not explained by this work (those passes changed only by the ambient fill multiply and the Earth night floor) and their run ranges are separated rather than overlapping, so they are flagged as regression candidates to reproduce with a second run before anything is attributed. The auto exposure is off in the suite (`--time 0`), so its cost is the meter pass alone.
