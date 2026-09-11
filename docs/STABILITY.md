# Fixed-view capture stability

Latest dense/faceted/cluster-culled Release recheck: mean maximum-channel delta
0.3264, maximum182, 2.6046% pixels changed by >1 code value. This supersedes
the intermediate measurements below and is still not a motion-stability pass.

Ran `tools/check-stability.ps1` against `build/release/orbital.exe` with two
independent 1280x720 captures using `--bookmark 4 --time 0 --no-hud --frames
120`. The resulting captures were compared pixel-by-pixel:

| Metric | Observed |
| --- | ---: |
| Mean absolute RGB delta | 0.1383 |
| Maximum RGB delta | 212 |
| Pixels changed (>1) | 1.0637% |

This is a fixed-view, fixed-time phase check, with the second run using four
additional frames (120 versus 124) to exercise temporal phase stability. It
does not guarantee stability for camera motion, animation, or other simulation
times.

The window smoke script was also run against the existing debug executable,
including three resizes, minimize/restore, and key transitions (`1`–`5`, `T`,
`F`, Space, F2). It completed successfully and wrote
`captures/debug-window.bmp`. The phase measurements above use the rebuilt
Release executable with the display-space dither correction. Large outliers
remain and require localization; this is not a claim that all flicker is fixed.

## Latest release phase check

After star filtering and density/roughness changes (65k versus 35k baseline),
the same sequential check measured `mean_abs_delta=0.3877`,
`max_delta=184`, and `percent_changed=2.9659`. This is not a controlled A/B
comparison because scene density and roughness changed between builds; it is a
current-build observation only.
