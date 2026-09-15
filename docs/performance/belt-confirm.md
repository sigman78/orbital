# Render performance: belt-confirm

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `6adcd2fdfd0b035f8fef465ac37072af5579d547`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1920×1080 | 6.549 | 5.834 | 6.385 | 0.644 | 1.447 | 2.183 | 1.075 |
| belt/fullscreen | 3440×1440 | 13.087 | 12.154 | 15.719 | 0.708 | 3.243 | 5.512 | 2.412 |

## Children

The scopes inside the groups: indicative, since a scope reads where its commands were issued rather than an exact cost.

| Scene / mode | Scope | Median | p95 | Run range |
| --- | --- | ---: | ---: | ---: |
| belt/window | gpu_cull_ms | 0.196 | 0.198 | 0.196 to 0.197 |
| belt/window | gpu_body_shadow_ms | 0.019 | 0.021 | 0.019 to 0.019 |
| belt/window | gpu_belt_light_ms | 0.424 | 0.729 | 0.366 to 0.483 |
| belt/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt/window | gpu_surface_sky_ms | 0.159 | 0.345 | 0.159 to 0.160 |
| belt/window | gpu_surface_bodies_ms | 0.549 | 0.934 | 0.547 to 0.551 |
| belt/window | gpu_surface_rocks_ms | 0.494 | 0.848 | 0.494 to 0.495 |
| belt/window | gpu_surface_splats_ms | 0.050 | 0.263 | 0.050 to 0.050 |
| belt/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_atmospheres_ms | 1.138 | 1.618 | 1.134 to 1.142 |
| belt/window | gpu_belt_dust_ms | 0.919 | 1.380 | 0.915 to 0.922 |
| belt/window | gpu_splat_mask_ms | 0.062 | 0.065 | 0.062 to 0.062 |
| belt/window | gpu_temporal_ms | 0.300 | 0.524 | 0.299 to 0.301 |
| belt/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_bloom_ms | 0.138 | 0.139 | 0.137 to 0.138 |
| belt/window | gpu_sun_visibility_ms | 0.028 | 0.029 | 0.028 to 0.028 |
| belt/window | gpu_flare_ms | 0.012 | 0.139 | 0.012 to 0.012 |
| belt/window | gpu_composite_ms | 0.228 | 0.615 | 0.227 to 0.229 |
| belt/window | gpu_spatial_aa_ms | 0.196 | 0.530 | 0.196 to 0.197 |
| belt/window | gpu_meter_ms | 0.010 | 0.144 | 0.009 to 0.010 |
| belt/window | gpu_present_ms | 0.035 | 0.036 | 0.035 to 0.035 |
| belt/fullscreen | gpu_cull_ms | 0.207 | 0.210 | 0.207 to 0.208 |
| belt/fullscreen | gpu_body_shadow_ms | 0.019 | 0.021 | 0.019 to 0.020 |
| belt/fullscreen | gpu_belt_light_ms | 0.480 | 0.679 | 0.479 to 0.481 |
| belt/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.197 | 0.001 to 0.001 |
| belt/fullscreen | gpu_surface_sky_ms | 0.496 | 1.032 | 0.495 to 0.497 |
| belt/fullscreen | gpu_surface_bodies_ms | 0.912 | 1.669 | 0.910 to 0.913 |
| belt/fullscreen | gpu_surface_rocks_ms | 1.305 | 2.046 | 1.296 to 1.314 |
| belt/fullscreen | gpu_surface_splats_ms | 0.067 | 0.253 | 0.067 to 0.067 |
| belt/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_atmospheres_ms | 2.879 | 6.003 | 2.797 to 2.962 |
| belt/fullscreen | gpu_belt_dust_ms | 2.503 | 3.703 | 2.471 to 2.534 |
| belt/fullscreen | gpu_splat_mask_ms | 0.086 | 0.089 | 0.085 to 0.086 |
| belt/fullscreen | gpu_temporal_ms | 0.751 | 1.096 | 0.702 to 0.799 |
| belt/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_bloom_ms | 0.274 | 0.417 | 0.273 to 0.274 |
| belt/fullscreen | gpu_sun_visibility_ms | 0.030 | 0.030 | 0.030 to 0.030 |
| belt/fullscreen | gpu_flare_ms | 0.023 | 0.152 | 0.023 to 0.023 |
| belt/fullscreen | gpu_composite_ms | 0.538 | 0.941 | 0.537 to 0.539 |
| belt/fullscreen | gpu_spatial_aa_ms | 0.444 | 0.784 | 0.443 to 0.444 |
| belt/fullscreen | gpu_meter_ms | 0.018 | 0.023 | 0.018 to 0.018 |
| belt/fullscreen | gpu_present_ms | 0.077 | 0.078 | 0.077 to 0.077 |

## Same-session runs of the flagged revisions

The +0.47 ms surface and +0.45 ms atmosphere increases flagged in session-followups-belt (fullscreen, 0db771e against 79ccaad) were re-measured with both revisions built and run in this session, four runs each (two passes of two repeats, the passes interleaved with this report's runs), same protocol. Medians of the run medians, then every run, milliseconds.

### belt/window (1920×1080)

| Metric | Revision | Median | Runs |
| --- | --- | ---: | --- |
| GPU | 79ccaad (gpu-scopes-after) | 7.261 | 7.181, 7.160, 7.422, 7.341 |
| GPU | 0db771e (session-followups-belt) | 6.954 | 6.896, 6.908, 7.000, 7.282 |
| GPU | 6adcd2f (this report) | 5.834 | 5.828, 5.841 |
| Cull/maps | 79ccaad (gpu-scopes-after) | 0.589 | 0.587, 0.589, 0.588, 0.589 |
| Cull/maps | 0db771e (session-followups-belt) | 0.588 | 0.587, 0.587, 0.590, 0.590 |
| Cull/maps | 6adcd2f (this report) | 0.644 | 0.586, 0.701 |
| Surface | 79ccaad (gpu-scopes-after) | 2.972 | 2.877, 2.917, 3.223, 3.026 |
| Surface | 0db771e (session-followups-belt) | 2.917 | 2.908, 2.909, 2.925, 2.950 |
| Surface | 6adcd2f (this report) | 1.447 | 1.502, 1.393 |
| Atmosphere group | 79ccaad (gpu-scopes-after) | 2.445 | 2.409, 2.397, 2.481, 2.498 |
| Atmosphere group | 0db771e (session-followups-belt) | 2.478 | 2.424, 2.461, 2.494, 2.500 |
| Atmosphere group | 6adcd2f (this report) | 2.183 | 2.233, 2.132 |
| Post | 79ccaad (gpu-scopes-after) | 1.156 | 1.180, 1.064, 1.141, 1.172 |
| Post | 0db771e (session-followups-belt) | 0.944 | 0.942, 0.943, 0.944, 0.950 |
| Post | 6adcd2f (this report) | 1.075 | 1.070, 1.081 |
| CPU + wait | 79ccaad (gpu-scopes-after) | 8.008 | 7.999, 7.939, 8.138, 8.016 |
| CPU + wait | 0db771e (session-followups-belt) | 7.825 | 7.790, 7.861, 7.776, 8.041 |
| CPU + wait | 6adcd2f (this report) | 6.549 | 6.547, 6.550 |

### belt/fullscreen (3440×1440)

| Metric | Revision | Median | Runs |
| --- | --- | ---: | --- |
| GPU | 79ccaad (gpu-scopes-after) | 14.788 | 14.702, 13.428, 14.950, 14.874 |
| GPU | 0db771e (session-followups-belt) | 14.701 | 14.722, 14.679, 13.433, 14.772 |
| GPU | 6adcd2f (this report) | 12.154 | 12.020, 12.288 |
| Cull/maps | 79ccaad (gpu-scopes-after) | 0.606 | 0.604, 0.621, 0.603, 0.607 |
| Cull/maps | 0db771e (session-followups-belt) | 0.605 | 0.604, 0.604, 0.606, 0.607 |
| Cull/maps | 6adcd2f (this report) | 0.708 | 0.707, 0.709 |
| Surface | 79ccaad (gpu-scopes-after) | 6.162 | 6.158, 3.368, 6.166, 6.344 |
| Surface | 0db771e (session-followups-belt) | 6.076 | 6.058, 6.131, 5.652, 6.093 |
| Surface | 6adcd2f (this report) | 3.243 | 3.166, 3.320 |
| Atmosphere group | 79ccaad (gpu-scopes-after) | 5.767 | 5.687, 7.003, 5.812, 5.722 |
| Atmosphere group | 0db771e (session-followups-belt) | 5.569 | 5.581, 5.557, 5.075, 5.688 |
| Atmosphere group | 6adcd2f (this report) | 5.512 | 5.373, 5.650 |
| Post | 79ccaad (gpu-scopes-after) | 2.240 | 2.231, 2.406, 2.250, 2.191 |
| Post | 0db771e (session-followups-belt) | 2.555 | 2.554, 2.556, 2.121, 2.565 |
| Post | 6adcd2f (this report) | 2.412 | 2.533, 2.291 |
| CPU + wait | 79ccaad (gpu-scopes-after) | 15.635 | 15.523, 14.236, 15.816, 15.747 |
| CPU + wait | 0db771e (session-followups-belt) | 15.518 | 15.559, 15.476, 14.189, 15.660 |
| CPU + wait | 6adcd2f (this report) | 13.087 | 12.971, 13.204 |

The flagged increases do not reproduce: at 3440×1440 the surface group reads 6.16 against 6.08 ms and the atmosphere group 5.77 against 5.57 ms across the two old revisions, and at 1920×1080 the two are within 0.05 ms on both groups. What does move between them is the post group, +0.31 ms at fullscreen (the flare pass) and −0.21 ms at 1920×1080 (the meter's change of form), neither of which was flagged. The old builds land one run in four about 1.3 ms low at fullscreen (13.4 ms), and 79ccaad once split 2.8 ms differently between its surface and atmosphere scopes with the frame total unchanged, which is the spread the single-pass flag was made of. The scratch directories `.scratch/performance/belt-confirm-{before,before-2,mid,mid-2}` hold the old builds' reports.
