# Validation record

## Environment

Windows x64; MSVC 19.44.35222; Ninja/CMake. Available GPU: NVIDIA GTX 1080 Ti, driver 582.66, Vulkan device API 1.4.312. RTX 4080 is a target, not a tested device. Pinned Slang v2026.14.1 compiles the default shader set; the app uses the conventional NoGraphicsAPI backend.

## Observed results

- Latest Release build and five CTest suites (system, geometry, assets, camera, materials) passed on 2026-09-11.
- Debug build and 30-frame capture smoke passed, without an installed validation layer.
- Slang runtime captures at 1600x900 (Earth) and 1280x720 (Jupiter/belt) rendered successfully. Developer captures: `captures/slang.bmp`, `captures/jovian.bmp`, `captures/belt.bmp`.
- Initial timing samples around 1.6–1.8 ms GPU at 1600x900 are exploratory, not a final 1080p acceptance benchmark. CPU submission/wait measurements must not be labeled isolated GPU time.

## Pending / limitations

- Portable Vulkan core/synchronization validation passed a 30-frame Debug run and the resize/minimize/input lifecycle smoke. Explicit layer insertion was confirmed by loader logs; see FOUNDATION.md. Seven Release shader modules passed SPIRV-Tools validation. A final dense/faceted/high-belt Debug recheck also passed 30 frames with zero core/synchronization findings, and Debug CTest passed 5/5. Evidence: `captures/final-validation-bookmark4.*`.
- Different-jitter-phase measurement before star filtering: mean maximum-channel delta 0.1383, maximum 212, 1.0637% pixels changed by >1 code value. Largest outlier localized to an isolated background star at (1017,107), with limb and small-body residuals. Star filtering was added afterward; remeasurement is pending. Identical captures at the same frame count are reproducibility evidence only.
- A 90-second installed-build 1080p tour completed 14,840 frames without failure. Excluding 60 warmup frames: GPU median 0.930 ms / p95 2.698 ms; CPU submission/wait median 6.026 ms / p95 6.218 ms. CSV: `captures/tour-1080.csv`. This is one tour cycle, not a long-duration soak.
- A 600-frame 2560x1440 high-quality belt run completed: after 60 warmup frames, GPU median 5.223 ms / p95 5.378 ms; CPU submission/wait median 6.038 ms / p95 11.456 ms. CSV: `captures/high-1440.csv`. These measurements precede the final star-footprint filtering refinement; they are not RTX 4080 results.
- Longer sustained testing, final performance acceptance and RTX 4080 testing remain outstanding.
- Final dense/faceted/cluster-culled 2560x1440 high belt: 600 frames completed, 540 post-warmup samples; GPU median 4.55 ms / p95 5.23 ms, CPU submission/wait median 6.46 ms / p95 7.25 ms. CSV: `captures/final-high-1440.csv`. This is a short local GTX 1080 Ti sample, not a controlled before/after benchmark.
- Latest Release build passed all five suites including strengthened rock topology/normal/bounds tests. Captured `captures/faceted-belt.bmp` shows the new geometry and density. Final phase check: mean max-channel delta .3264, maximum182, 2.6046% pixels >1 code value. Residual temporal artifacts remain.
- Inspect latest seam correction around longitude wrap and poles under motion. A single view does not prove all seams eliminated.
- Internal HDR only, SDR presentation; approximate atmosphere and shell clouds; TAA without object motion vectors; deliberately compressed scene distances. No hardware ray tracing. Subpixel asteroids use filtered area-weighted billboards and do not cast individual shadows.

Commands/scripts and measured updates belong here or in [STABILITY.md](STABILITY.md); never turn a planned check into a claimed pass.
