# Render performance: motion-vectors

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `41d90963d66e3a7c242b3dae014e61092b9e5929`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| earth/window | 1600×900 | 3.909 | 3.282 | 3.839 | 0.582 | 1.001 | 0.981 | 0.633 |
| earth/fullscreen | 3440×1440 | 8.937 | 8.178 | 9.006 | 0.716 | 2.734 | 2.698 | 1.989 |
| giant/window | 1600×900 | 4.750 | 3.993 | 4.657 | 0.630 | 0.962 | 1.767 | 0.628 |
| giant/fullscreen | 3440×1440 | 11.842 | 11.065 | 12.333 | 0.777 | 2.686 | 5.541 | 2.005 |
| moon/window | 1600×900 | 2.591 | 1.979 | 2.308 | 0.627 | 0.653 | 0.076 | 0.623 |
| moon/fullscreen | 3440×1440 | 5.681 | 5.006 | 5.620 | 0.647 | 2.008 | 0.355 | 1.988 |
| mars/window | 1600×900 | 2.948 | 2.331 | 2.614 | 0.628 | 0.668 | 0.438 | 0.597 |
| mars/fullscreen | 3440×1440 | 6.717 | 6.013 | 6.776 | 0.687 | 2.077 | 1.335 | 1.912 |
| dawn/window | 1600×900 | 3.628 | 2.911 | 3.347 | 0.512 | 0.797 | 0.921 | 0.671 |
| dawn/fullscreen | 3440×1440 | 8.241 | 7.521 | 8.396 | 0.645 | 2.228 | 2.557 | 2.015 |
| belt/window | 1600×900 | 4.640 | 3.936 | 4.638 | 0.608 | 1.133 | 1.489 | 0.694 |
| belt/fullscreen | 3440×1440 | 12.008 | 11.186 | 18.503 | 0.756 | 3.462 | 4.678 | 2.203 |
| belt-sun/window | 1600×900 | 3.358 | 2.753 | 3.216 | 0.610 | 0.612 | 0.780 | 0.644 |
| belt-sun/fullscreen | 3440×1440 | 7.983 | 7.283 | 8.214 | 0.761 | 2.054 | 2.429 | 2.007 |

## Children

The scopes inside the groups: indicative, since a scope reads where its commands were issued rather than an exact cost.

| Scene / mode | Scope | Median | p95 | Run range |
| --- | --- | ---: | ---: | ---: |
| earth/window | gpu_cull_ms | 0.195 | 0.197 | 0.195 to 0.196 |
| earth/window | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.018 |
| earth/window | gpu_belt_light_ms | 0.362 | 0.494 | 0.361 to 0.365 |
| earth/window | gpu_belt_disc_ms | 0.000 | 0.115 | 0.000 to 0.000 |
| earth/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| earth/window | gpu_surface_sky_ms | 0.217 | 0.220 | 0.216 to 0.219 |
| earth/window | gpu_surface_bodies_ms | 0.319 | 0.442 | 0.318 to 0.323 |
| earth/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/window | gpu_surface_splats_ms | 0.004 | 0.007 | 0.004 to 0.004 |
| earth/window | gpu_surface_clouds_ms | 0.175 | 0.176 | 0.174 to 0.176 |
| earth/window | gpu_surface_motion_ms | 0.048 | 0.049 | 0.048 to 0.049 |
| earth/window | gpu_atmospheres_ms | 0.767 | 0.779 | 0.762 to 0.773 |
| earth/window | gpu_belt_dust_ms | 0.203 | 0.210 | 0.203 to 0.205 |
| earth/window | gpu_splat_mask_ms | 0.008 | 0.009 | 0.008 to 0.008 |
| earth/window | gpu_temporal_ms | 0.208 | 0.209 | 0.207 to 0.209 |
| earth/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| earth/window | gpu_bloom_ms | 0.083 | 0.086 | 0.083 to 0.084 |
| earth/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| earth/window | gpu_composite_ms | 0.169 | 0.170 | 0.169 to 0.171 |
| earth/window | gpu_spatial_aa_ms | 0.131 | 0.133 | 0.131 to 0.131 |
| earth/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| earth/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| earth/fullscreen | gpu_cull_ms | 0.202 | 0.205 | 0.201 to 0.203 |
| earth/fullscreen | gpu_body_shadow_ms | 0.019 | 0.019 | 0.018 to 0.019 |
| earth/fullscreen | gpu_belt_light_ms | 0.493 | 0.505 | 0.489 to 0.495 |
| earth/fullscreen | gpu_belt_disc_ms | 0.000 | 0.119 | 0.000 to 0.000 |
| earth/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.126 | 0.001 to 0.001 |
| earth/fullscreen | gpu_surface_sky_ms | 0.933 | 0.943 | 0.924 to 0.939 |
| earth/fullscreen | gpu_surface_bodies_ms | 0.660 | 0.891 | 0.652 to 0.664 |
| earth/fullscreen | gpu_surface_rocks_ms | 0.024 | 0.026 | 0.024 to 0.024 |
| earth/fullscreen | gpu_surface_splats_ms | 0.004 | 0.005 | 0.004 to 0.004 |
| earth/fullscreen | gpu_surface_clouds_ms | 0.379 | 0.384 | 0.376 to 0.383 |
| earth/fullscreen | gpu_surface_motion_ms | 0.148 | 0.151 | 0.147 to 0.150 |
| earth/fullscreen | gpu_atmospheres_ms | 2.096 | 2.316 | 2.081 to 2.110 |
| earth/fullscreen | gpu_belt_dust_ms | 0.586 | 0.616 | 0.580 to 0.591 |
| earth/fullscreen | gpu_splat_mask_ms | 0.008 | 0.009 | 0.008 to 0.008 |
| earth/fullscreen | gpu_temporal_ms | 0.688 | 0.843 | 0.683 to 0.692 |
| earth/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| earth/fullscreen | gpu_bloom_ms | 0.244 | 0.246 | 0.242 to 0.245 |
| earth/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| earth/fullscreen | gpu_composite_ms | 0.567 | 0.574 | 0.563 to 0.570 |
| earth/fullscreen | gpu_spatial_aa_ms | 0.387 | 0.394 | 0.385 to 0.389 |
| earth/fullscreen | gpu_meter_ms | 0.014 | 0.017 | 0.014 to 0.014 |
| earth/fullscreen | gpu_present_ms | 0.075 | 0.076 | 0.074 to 0.075 |
| giant/window | gpu_cull_ms | 0.248 | 0.251 | 0.247 to 0.248 |
| giant/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| giant/window | gpu_belt_light_ms | 0.360 | 0.485 | 0.359 to 0.364 |
| giant/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| giant/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| giant/window | gpu_surface_sky_ms | 0.251 | 0.253 | 0.249 to 0.253 |
| giant/window | gpu_surface_bodies_ms | 0.289 | 0.297 | 0.287 to 0.291 |
| giant/window | gpu_surface_rocks_ms | 0.037 | 0.039 | 0.037 to 0.037 |
| giant/window | gpu_surface_splats_ms | 0.104 | 0.106 | 0.103 to 0.104 |
| giant/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/window | gpu_surface_motion_ms | 0.050 | 0.051 | 0.050 to 0.050 |
| giant/window | gpu_atmospheres_ms | 0.538 | 0.542 | 0.534 to 0.541 |
| giant/window | gpu_belt_dust_ms | 1.108 | 1.126 | 1.102 to 1.115 |
| giant/window | gpu_splat_mask_ms | 0.119 | 0.121 | 0.118 to 0.120 |
| giant/window | gpu_temporal_ms | 0.211 | 0.213 | 0.210 to 0.213 |
| giant/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/window | gpu_bloom_ms | 0.082 | 0.084 | 0.082 to 0.083 |
| giant/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| giant/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| giant/window | gpu_composite_ms | 0.169 | 0.171 | 0.168 to 0.171 |
| giant/window | gpu_spatial_aa_ms | 0.123 | 0.125 | 0.123 to 0.124 |
| giant/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| giant/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| giant/fullscreen | gpu_cull_ms | 0.269 | 0.274 | 0.268 to 0.270 |
| giant/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| giant/fullscreen | gpu_belt_light_ms | 0.486 | 0.502 | 0.485 to 0.489 |
| giant/fullscreen | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| giant/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.126 | 0.001 to 0.001 |
| giant/fullscreen | gpu_surface_sky_ms | 0.986 | 1.269 | 0.975 to 0.992 |
| giant/fullscreen | gpu_surface_bodies_ms | 0.656 | 0.866 | 0.649 to 0.657 |
| giant/fullscreen | gpu_surface_rocks_ms | 0.113 | 0.119 | 0.112 to 0.114 |
| giant/fullscreen | gpu_surface_splats_ms | 0.188 | 0.193 | 0.187 to 0.189 |
| giant/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/fullscreen | gpu_surface_motion_ms | 0.161 | 0.164 | 0.160 to 0.162 |
| giant/fullscreen | gpu_atmospheres_ms | 1.489 | 1.693 | 1.470 to 1.498 |
| giant/fullscreen | gpu_belt_dust_ms | 3.829 | 4.618 | 3.810 to 3.849 |
| giant/fullscreen | gpu_splat_mask_ms | 0.196 | 0.199 | 0.196 to 0.197 |
| giant/fullscreen | gpu_temporal_ms | 0.708 | 0.867 | 0.703 to 0.711 |
| giant/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/fullscreen | gpu_bloom_ms | 0.253 | 0.257 | 0.252 to 0.255 |
| giant/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| giant/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| giant/fullscreen | gpu_composite_ms | 0.567 | 0.578 | 0.564 to 0.570 |
| giant/fullscreen | gpu_spatial_aa_ms | 0.371 | 0.377 | 0.369 to 0.372 |
| giant/fullscreen | gpu_meter_ms | 0.016 | 0.018 | 0.016 to 0.016 |
| giant/fullscreen | gpu_present_ms | 0.076 | 0.077 | 0.076 to 0.076 |
| moon/window | gpu_cull_ms | 0.126 | 0.127 | 0.126 to 0.126 |
| moon/window | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.018 |
| moon/window | gpu_belt_light_ms | 0.481 | 0.495 | 0.480 to 0.484 |
| moon/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| moon/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/window | gpu_surface_sky_ms | 0.227 | 0.362 | 0.227 to 0.229 |
| moon/window | gpu_surface_bodies_ms | 0.131 | 0.132 | 0.131 to 0.132 |
| moon/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_surface_splats_ms | 0.000 | 0.001 | 0.000 to 0.001 |
| moon/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/window | gpu_surface_motion_ms | 0.048 | 0.049 | 0.048 to 0.049 |
| moon/window | gpu_atmospheres_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/window | gpu_belt_dust_ms | 0.070 | 0.070 | 0.069 to 0.070 |
| moon/window | gpu_splat_mask_ms | 0.002 | 0.005 | 0.002 to 0.002 |
| moon/window | gpu_temporal_ms | 0.204 | 0.208 | 0.204 to 0.205 |
| moon/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/window | gpu_bloom_ms | 0.076 | 0.078 | 0.076 to 0.077 |
| moon/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| moon/window | gpu_composite_ms | 0.169 | 0.170 | 0.168 to 0.170 |
| moon/window | gpu_spatial_aa_ms | 0.133 | 0.136 | 0.133 to 0.133 |
| moon/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| moon/window | gpu_present_ms | 0.025 | 0.026 | 0.025 to 0.026 |
| moon/fullscreen | gpu_cull_ms | 0.132 | 0.135 | 0.131 to 0.132 |
| moon/fullscreen | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.018 |
| moon/fullscreen | gpu_belt_light_ms | 0.495 | 0.502 | 0.492 to 0.497 |
| moon/fullscreen | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| moon/fullscreen | gpu_surface_prepass_ms | 0.113 | 0.115 | 0.112 to 0.114 |
| moon/fullscreen | gpu_surface_sky_ms | 0.926 | 0.940 | 0.916 to 0.931 |
| moon/fullscreen | gpu_surface_bodies_ms | 0.264 | 0.269 | 0.262 to 0.266 |
| moon/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_surface_splats_ms | 0.000 | 0.001 | 0.000 to 0.001 |
| moon/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/fullscreen | gpu_surface_motion_ms | 0.144 | 0.147 | 0.143 to 0.145 |
| moon/fullscreen | gpu_atmospheres_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/fullscreen | gpu_belt_dust_ms | 0.345 | 0.351 | 0.342 to 0.348 |
| moon/fullscreen | gpu_splat_mask_ms | 0.005 | 0.006 | 0.005 to 0.005 |
| moon/fullscreen | gpu_temporal_ms | 0.677 | 0.691 | 0.673 to 0.681 |
| moon/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/fullscreen | gpu_bloom_ms | 0.244 | 0.248 | 0.242 to 0.246 |
| moon/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| moon/fullscreen | gpu_composite_ms | 0.565 | 0.772 | 0.561 to 0.568 |
| moon/fullscreen | gpu_spatial_aa_ms | 0.400 | 0.406 | 0.397 to 0.401 |
| moon/fullscreen | gpu_meter_ms | 0.014 | 0.016 | 0.014 to 0.014 |
| moon/fullscreen | gpu_present_ms | 0.074 | 0.075 | 0.073 to 0.074 |
| mars/window | gpu_cull_ms | 0.126 | 0.126 | 0.125 to 0.126 |
| mars/window | gpu_body_shadow_ms | 0.019 | 0.019 | 0.018 to 0.019 |
| mars/window | gpu_belt_light_ms | 0.482 | 0.486 | 0.481 to 0.484 |
| mars/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| mars/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| mars/window | gpu_surface_sky_ms | 0.255 | 0.258 | 0.255 to 0.257 |
| mars/window | gpu_surface_bodies_ms | 0.118 | 0.120 | 0.118 to 0.119 |
| mars/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_surface_splats_ms | 0.000 | 0.001 | 0.000 to 0.000 |
| mars/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/window | gpu_surface_motion_ms | 0.051 | 0.052 | 0.051 to 0.051 |
| mars/window | gpu_atmospheres_ms | 0.365 | 0.370 | 0.364 to 0.367 |
| mars/window | gpu_belt_dust_ms | 0.069 | 0.070 | 0.069 to 0.070 |
| mars/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| mars/window | gpu_temporal_ms | 0.201 | 0.202 | 0.200 to 0.202 |
| mars/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/window | gpu_bloom_ms | 0.076 | 0.077 | 0.075 to 0.076 |
| mars/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| mars/window | gpu_composite_ms | 0.169 | 0.169 | 0.168 to 0.170 |
| mars/window | gpu_spatial_aa_ms | 0.113 | 0.118 | 0.112 to 0.113 |
| mars/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| mars/window | gpu_present_ms | 0.025 | 0.026 | 0.025 to 0.025 |
| mars/fullscreen | gpu_cull_ms | 0.174 | 0.176 | 0.173 to 0.174 |
| mars/fullscreen | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| mars/fullscreen | gpu_belt_light_ms | 0.494 | 0.561 | 0.487 to 0.497 |
| mars/fullscreen | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| mars/fullscreen | gpu_surface_prepass_ms | 0.112 | 0.126 | 0.002 to 0.113 |
| mars/fullscreen | gpu_surface_sky_ms | 1.018 | 1.228 | 1.010 to 1.024 |
| mars/fullscreen | gpu_surface_bodies_ms | 0.244 | 0.249 | 0.243 to 0.246 |
| mars/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/fullscreen | gpu_surface_splats_ms | 0.002 | 0.003 | 0.002 to 0.002 |
| mars/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/fullscreen | gpu_surface_motion_ms | 0.152 | 0.155 | 0.151 to 0.153 |
| mars/fullscreen | gpu_atmospheres_ms | 0.949 | 0.996 | 0.946 to 0.955 |
| mars/fullscreen | gpu_belt_dust_ms | 0.374 | 0.398 | 0.373 to 0.375 |
| mars/fullscreen | gpu_splat_mask_ms | 0.006 | 0.007 | 0.006 to 0.006 |
| mars/fullscreen | gpu_temporal_ms | 0.668 | 0.682 | 0.666 to 0.673 |
| mars/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/fullscreen | gpu_bloom_ms | 0.242 | 0.249 | 0.241 to 0.245 |
| mars/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| mars/fullscreen | gpu_composite_ms | 0.564 | 0.577 | 0.562 to 0.568 |
| mars/fullscreen | gpu_spatial_aa_ms | 0.337 | 0.343 | 0.336 to 0.338 |
| mars/fullscreen | gpu_meter_ms | 0.014 | 0.017 | 0.014 to 0.014 |
| mars/fullscreen | gpu_present_ms | 0.074 | 0.075 | 0.074 to 0.074 |
| dawn/window | gpu_cull_ms | 0.126 | 0.127 | 0.126 to 0.126 |
| dawn/window | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.019 |
| dawn/window | gpu_belt_light_ms | 0.366 | 0.496 | 0.365 to 0.368 |
| dawn/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| dawn/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| dawn/window | gpu_surface_sky_ms | 0.133 | 0.136 | 0.133 to 0.134 |
| dawn/window | gpu_surface_bodies_ms | 0.338 | 0.341 | 0.338 to 0.340 |
| dawn/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/window | gpu_surface_splats_ms | 0.000 | 0.001 | 0.000 to 0.001 |
| dawn/window | gpu_surface_clouds_ms | 0.196 | 0.198 | 0.196 to 0.197 |
| dawn/window | gpu_surface_motion_ms | 0.048 | 0.049 | 0.048 to 0.049 |
| dawn/window | gpu_atmospheres_ms | 0.848 | 0.855 | 0.847 to 0.854 |
| dawn/window | gpu_belt_dust_ms | 0.068 | 0.069 | 0.068 to 0.069 |
| dawn/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| dawn/window | gpu_temporal_ms | 0.211 | 0.212 | 0.210 to 0.212 |
| dawn/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| dawn/window | gpu_bloom_ms | 0.092 | 0.094 | 0.092 to 0.092 |
| dawn/window | gpu_sun_visibility_ms | 0.028 | 0.028 | 0.028 to 0.028 |
| dawn/window | gpu_flare_ms | 0.009 | 0.010 | 0.009 to 0.009 |
| dawn/window | gpu_composite_ms | 0.176 | 0.177 | 0.175 to 0.177 |
| dawn/window | gpu_spatial_aa_ms | 0.118 | 0.119 | 0.118 to 0.118 |
| dawn/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| dawn/window | gpu_present_ms | 0.025 | 0.026 | 0.025 to 0.025 |
| dawn/fullscreen | gpu_cull_ms | 0.128 | 0.129 | 0.127 to 0.128 |
| dawn/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| dawn/fullscreen | gpu_belt_light_ms | 0.495 | 0.512 | 0.492 to 0.501 |
| dawn/fullscreen | gpu_belt_disc_ms | 0.000 | 0.118 | 0.000 to 0.000 |
| dawn/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_sky_ms | 0.638 | 0.652 | 0.635 to 0.640 |
| dawn/fullscreen | gpu_surface_bodies_ms | 0.737 | 1.049 | 0.720 to 0.857 |
| dawn/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_clouds_ms | 0.458 | 0.482 | 0.454 to 0.463 |
| dawn/fullscreen | gpu_surface_motion_ms | 0.139 | 0.143 | 0.139 to 0.140 |
| dawn/fullscreen | gpu_atmospheres_ms | 2.316 | 2.631 | 2.297 to 2.324 |
| dawn/fullscreen | gpu_belt_dust_ms | 0.230 | 0.239 | 0.229 to 0.232 |
| dawn/fullscreen | gpu_splat_mask_ms | 0.005 | 0.006 | 0.005 to 0.005 |
| dawn/fullscreen | gpu_temporal_ms | 0.700 | 0.857 | 0.697 to 0.705 |
| dawn/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| dawn/fullscreen | gpu_bloom_ms | 0.221 | 0.227 | 0.221 to 0.223 |
| dawn/fullscreen | gpu_sun_visibility_ms | 0.030 | 0.031 | 0.030 to 0.031 |
| dawn/fullscreen | gpu_flare_ms | 0.024 | 0.024 | 0.024 to 0.024 |
| dawn/fullscreen | gpu_composite_ms | 0.589 | 0.606 | 0.588 to 0.593 |
| dawn/fullscreen | gpu_spatial_aa_ms | 0.359 | 0.369 | 0.357 to 0.360 |
| dawn/fullscreen | gpu_meter_ms | 0.015 | 0.018 | 0.015 to 0.015 |
| dawn/fullscreen | gpu_present_ms | 0.074 | 0.075 | 0.074 to 0.074 |
| belt/window | gpu_cull_ms | 0.226 | 0.228 | 0.226 to 0.227 |
| belt/window | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| belt/window | gpu_belt_light_ms | 0.361 | 0.481 | 0.360 to 0.363 |
| belt/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt/window | gpu_surface_sky_ms | 0.109 | 0.112 | 0.109 to 0.110 |
| belt/window | gpu_surface_bodies_ms | 0.434 | 0.441 | 0.433 to 0.437 |
| belt/window | gpu_surface_rocks_ms | 0.343 | 0.350 | 0.342 to 0.345 |
| belt/window | gpu_surface_splats_ms | 0.048 | 0.050 | 0.048 to 0.049 |
| belt/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_surface_motion_ms | 0.078 | 0.080 | 0.078 to 0.078 |
| belt/window | gpu_atmospheres_ms | 0.830 | 0.839 | 0.829 to 0.836 |
| belt/window | gpu_belt_dust_ms | 0.595 | 0.793 | 0.593 to 0.598 |
| belt/window | gpu_splat_mask_ms | 0.060 | 0.061 | 0.060 to 0.061 |
| belt/window | gpu_temporal_ms | 0.218 | 0.219 | 0.218 to 0.219 |
| belt/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_bloom_ms | 0.085 | 0.086 | 0.085 to 0.085 |
| belt/window | gpu_sun_visibility_ms | 0.028 | 0.028 | 0.028 to 0.028 |
| belt/window | gpu_flare_ms | 0.009 | 0.009 | 0.009 to 0.009 |
| belt/window | gpu_composite_ms | 0.176 | 0.176 | 0.175 to 0.177 |
| belt/window | gpu_spatial_aa_ms | 0.140 | 0.142 | 0.139 to 0.140 |
| belt/window | gpu_meter_ms | 0.007 | 0.010 | 0.007 to 0.007 |
| belt/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| belt/fullscreen | gpu_cull_ms | 0.248 | 0.251 | 0.247 to 0.248 |
| belt/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| belt/fullscreen | gpu_belt_light_ms | 0.487 | 0.500 | 0.486 to 0.488 |
| belt/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.101 | 0.001 to 0.001 |
| belt/fullscreen | gpu_surface_sky_ms | 0.508 | 0.672 | 0.505 to 0.511 |
| belt/fullscreen | gpu_surface_bodies_ms | 1.020 | 3.105 | 1.011 to 1.020 |
| belt/fullscreen | gpu_surface_rocks_ms | 1.290 | 1.833 | 1.285 to 1.291 |
| belt/fullscreen | gpu_surface_splats_ms | 0.069 | 0.085 | 0.069 to 0.069 |
| belt/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_surface_motion_ms | 0.230 | 0.234 | 0.229 to 0.231 |
| belt/fullscreen | gpu_atmospheres_ms | 2.581 | 5.500 | 2.548 to 2.587 |
| belt/fullscreen | gpu_belt_dust_ms | 1.977 | 2.876 | 1.967 to 1.981 |
| belt/fullscreen | gpu_splat_mask_ms | 0.086 | 0.088 | 0.086 to 0.086 |
| belt/fullscreen | gpu_temporal_ms | 0.733 | 1.086 | 0.727 to 0.733 |
| belt/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_bloom_ms | 0.275 | 0.282 | 0.274 to 0.276 |
| belt/fullscreen | gpu_sun_visibility_ms | 0.030 | 0.031 | 0.030 to 0.030 |
| belt/fullscreen | gpu_flare_ms | 0.024 | 0.024 | 0.024 to 0.024 |
| belt/fullscreen | gpu_composite_ms | 0.593 | 0.949 | 0.589 to 0.594 |
| belt/fullscreen | gpu_spatial_aa_ms | 0.440 | 0.466 | 0.438 to 0.440 |
| belt/fullscreen | gpu_meter_ms | 0.017 | 0.020 | 0.017 to 0.017 |
| belt/fullscreen | gpu_present_ms | 0.077 | 0.077 | 0.076 to 0.077 |
| belt-sun/window | gpu_cull_ms | 0.228 | 0.230 | 0.227 to 0.228 |
| belt-sun/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| belt-sun/window | gpu_belt_light_ms | 0.359 | 0.479 | 0.357 to 0.359 |
| belt-sun/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt-sun/window | gpu_surface_sky_ms | 0.232 | 0.367 | 0.230 to 0.232 |
| belt-sun/window | gpu_surface_bodies_ms | 0.001 | 0.005 | 0.001 to 0.001 |
| belt-sun/window | gpu_surface_rocks_ms | 0.143 | 0.147 | 0.141 to 0.143 |
| belt-sun/window | gpu_surface_splats_ms | 0.075 | 0.076 | 0.075 to 0.075 |
| belt-sun/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_surface_motion_ms | 0.079 | 0.079 | 0.078 to 0.079 |
| belt-sun/window | gpu_atmospheres_ms | 0.045 | 0.045 | 0.045 to 0.045 |
| belt-sun/window | gpu_belt_dust_ms | 0.653 | 0.849 | 0.647 to 0.653 |
| belt-sun/window | gpu_splat_mask_ms | 0.079 | 0.080 | 0.078 to 0.079 |
| belt-sun/window | gpu_temporal_ms | 0.207 | 0.208 | 0.205 to 0.207 |
| belt-sun/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_bloom_ms | 0.076 | 0.078 | 0.076 to 0.076 |
| belt-sun/window | gpu_sun_visibility_ms | 0.028 | 0.029 | 0.028 to 0.028 |
| belt-sun/window | gpu_flare_ms | 0.005 | 0.006 | 0.005 to 0.006 |
| belt-sun/window | gpu_composite_ms | 0.171 | 0.172 | 0.170 to 0.171 |
| belt-sun/window | gpu_spatial_aa_ms | 0.119 | 0.122 | 0.118 to 0.119 |
| belt-sun/window | gpu_meter_ms | 0.007 | 0.010 | 0.007 to 0.007 |
| belt-sun/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| belt-sun/fullscreen | gpu_cull_ms | 0.245 | 0.250 | 0.244 to 0.246 |
| belt-sun/fullscreen | gpu_body_shadow_ms | 0.020 | 0.020 | 0.020 to 0.020 |
| belt-sun/fullscreen | gpu_belt_light_ms | 0.494 | 0.505 | 0.490 to 0.500 |
| belt-sun/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt-sun/fullscreen | gpu_surface_sky_ms | 0.888 | 1.124 | 0.885 to 0.890 |
| belt-sun/fullscreen | gpu_surface_bodies_ms | 0.014 | 0.024 | 0.014 to 0.014 |
| belt-sun/fullscreen | gpu_surface_rocks_ms | 0.500 | 0.717 | 0.499 to 0.518 |
| belt-sun/fullscreen | gpu_surface_splats_ms | 0.115 | 0.118 | 0.115 to 0.116 |
| belt-sun/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_surface_motion_ms | 0.252 | 0.258 | 0.250 to 0.252 |
| belt-sun/fullscreen | gpu_atmospheres_ms | 0.284 | 0.393 | 0.281 to 0.284 |
| belt-sun/fullscreen | gpu_belt_dust_ms | 2.031 | 2.248 | 2.016 to 2.032 |
| belt-sun/fullscreen | gpu_splat_mask_ms | 0.111 | 0.113 | 0.111 to 0.111 |
| belt-sun/fullscreen | gpu_temporal_ms | 0.684 | 0.699 | 0.681 to 0.685 |
| belt-sun/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_bloom_ms | 0.219 | 0.224 | 0.217 to 0.219 |
| belt-sun/fullscreen | gpu_sun_visibility_ms | 0.032 | 0.033 | 0.031 to 0.032 |
| belt-sun/fullscreen | gpu_flare_ms | 0.015 | 0.015 | 0.014 to 0.015 |
| belt-sun/fullscreen | gpu_composite_ms | 0.573 | 0.616 | 0.570 to 0.573 |
| belt-sun/fullscreen | gpu_spatial_aa_ms | 0.386 | 0.400 | 0.384 to 0.387 |
| belt-sun/fullscreen | gpu_meter_ms | 0.016 | 0.019 | 0.016 to 0.016 |
| belt-sun/fullscreen | gpu_present_ms | 0.076 | 0.077 | 0.076 to 0.076 |

## Compared with static-heap

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| earth/window | cpu_submit_and_wait_ms | +0.041 | +1.1 | within threshold |
| earth/window | gpu_ms | +0.122 | +3.9 | within threshold |
| earth/window | cpu_prepare_ms | -0.011 | -4.3 | within threshold |
| earth/window | gpu_cull_shadow_ms | +0.027 | +4.8 | within threshold |
| earth/window | gpu_surface_ms | +0.041 | +4.3 | within threshold |
| earth/window | gpu_atmosphere_ms | -0.009 | -0.9 | within threshold |
| earth/window | gpu_post_ms | -0.008 | -1.3 | within threshold |
| earth/window | gpu_cull_ms | +0.029 | +17.2 | within threshold |
| earth/window | gpu_body_shadow_ms | -0.001 | -3.3 | within threshold |
| earth/window | gpu_belt_light_ms | -0.005 | -1.4 | within threshold |
| earth/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_sky_ms | -0.002 | -0.7 | within threshold |
| earth/window | gpu_surface_bodies_ms | -0.005 | -1.6 | within threshold |
| earth/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_splats_ms | -0.001 | -20.0 | within threshold |
| earth/window | gpu_surface_clouds_ms | -0.001 | -0.6 | within threshold |
| earth/window | gpu_atmospheres_ms | -0.006 | -0.8 | within threshold |
| earth/window | gpu_belt_dust_ms | -0.002 | -1.0 | within threshold |
| earth/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_temporal_ms | +0.003 | +1.5 | within threshold |
| earth/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_bloom_ms | -0.005 | -5.8 | within threshold |
| earth/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_composite_ms | -0.003 | -1.8 | within threshold |
| earth/window | gpu_spatial_aa_ms | -0.003 | -2.3 | within threshold |
| earth/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | cpu_submit_and_wait_ms | +0.111 | +1.3 | within threshold |
| earth/fullscreen | gpu_ms | +0.200 | +2.5 | within threshold |
| earth/fullscreen | cpu_prepare_ms | -0.026 | -8.0 | within threshold |
| earth/fullscreen | gpu_cull_shadow_ms | +0.027 | +3.9 | within threshold |
| earth/fullscreen | gpu_surface_ms | +0.172 | +6.7 | within threshold |
| earth/fullscreen | gpu_atmosphere_ms | +0.003 | +0.1 | within threshold |
| earth/fullscreen | gpu_post_ms | +0.002 | +0.1 | within threshold |
| earth/fullscreen | gpu_cull_ms | +0.032 | +18.8 | within threshold |
| earth/fullscreen | gpu_body_shadow_ms | -0.000 | -0.9 | within threshold |
| earth/fullscreen | gpu_belt_light_ms | -0.005 | -1.0 | within threshold |
| earth/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_sky_ms | +0.002 | +0.2 | within threshold |
| earth/fullscreen | gpu_surface_bodies_ms | +0.001 | +0.2 | within threshold |
| earth/fullscreen | gpu_surface_rocks_ms | -0.001 | -4.2 | within threshold |
| earth/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_clouds_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_atmospheres_ms | +0.003 | +0.1 | within threshold |
| earth/fullscreen | gpu_belt_dust_ms | -0.001 | -0.2 | within threshold |
| earth/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_temporal_ms | +0.018 | +2.8 | within threshold |
| earth/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_bloom_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_composite_ms | -0.006 | -1.1 | within threshold |
| earth/fullscreen | gpu_spatial_aa_ms | -0.007 | -1.8 | within threshold |
| earth/fullscreen | gpu_meter_ms | -0.001 | -6.7 | within threshold |
| earth/fullscreen | gpu_present_ms | -0.001 | -1.4 | within threshold |
| giant/window | cpu_submit_and_wait_ms | +0.022 | +0.5 | within threshold |
| giant/window | gpu_ms | +0.056 | +1.4 | within threshold |
| giant/window | cpu_prepare_ms | -0.084 | -25.5 | within threshold |
| giant/window | gpu_cull_shadow_ms | +0.031 | +5.3 | within threshold |
| giant/window | gpu_surface_ms | +0.040 | +4.3 | within threshold |
| giant/window | gpu_atmosphere_ms | -0.013 | -0.7 | within threshold |
| giant/window | gpu_post_ms | -0.004 | -0.6 | within threshold |
| giant/window | gpu_cull_ms | +0.037 | +17.4 | within threshold |
| giant/window | gpu_body_shadow_ms | -0.000 | -1.3 | within threshold |
| giant/window | gpu_belt_light_ms | -0.005 | -1.4 | within threshold |
| giant/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_surface_sky_ms | -0.001 | -0.4 | within threshold |
| giant/window | gpu_surface_bodies_ms | -0.005 | -1.7 | within threshold |
| giant/window | gpu_surface_rocks_ms | -0.001 | -2.7 | within threshold |
| giant/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_atmospheres_ms | -0.003 | -0.6 | within threshold |
| giant/window | gpu_belt_dust_ms | -0.009 | -0.8 | within threshold |
| giant/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_temporal_ms | +0.004 | +2.0 | within threshold |
| giant/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_bloom_ms | -0.003 | -3.6 | within threshold |
| giant/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_composite_ms | -0.003 | -1.8 | within threshold |
| giant/window | gpu_spatial_aa_ms | -0.003 | -2.4 | within threshold |
| giant/window | gpu_meter_ms | -0.001 | -12.5 | within threshold |
| giant/window | gpu_present_ms | -0.001 | -3.7 | within threshold |
| giant/fullscreen | cpu_submit_and_wait_ms | +0.141 | +1.2 | within threshold |
| giant/fullscreen | gpu_ms | +0.210 | +1.9 | within threshold |
| giant/fullscreen | cpu_prepare_ms | -0.003 | -1.0 | within threshold |
| giant/fullscreen | gpu_cull_shadow_ms | +0.043 | +5.9 | within threshold |
| giant/fullscreen | gpu_surface_ms | +0.120 | +4.7 | within threshold |
| giant/fullscreen | gpu_atmosphere_ms | +0.005 | +0.1 | within threshold |
| giant/fullscreen | gpu_post_ms | -0.003 | -0.2 | within threshold |
| giant/fullscreen | gpu_cull_ms | +0.045 | +20.1 | within threshold |
| giant/fullscreen | gpu_body_shadow_ms | -0.000 | -1.9 | within threshold |
| giant/fullscreen | gpu_belt_light_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_surface_sky_ms | +0.004 | +0.4 | within threshold |
| giant/fullscreen | gpu_surface_bodies_ms | -0.002 | -0.3 | within threshold |
| giant/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_surface_splats_ms | +0.002 | +1.1 | within threshold |
| giant/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_atmospheres_ms | -0.007 | -0.5 | within threshold |
| giant/fullscreen | gpu_belt_dust_ms | -0.017 | -0.5 | within threshold |
| giant/fullscreen | gpu_splat_mask_ms | +0.006 | +3.2 | within threshold |
| giant/fullscreen | gpu_temporal_ms | +0.019 | +2.8 | within threshold |
| giant/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_bloom_ms | -0.002 | -0.8 | within threshold |
| giant/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_composite_ms | -0.009 | -1.6 | within threshold |
| giant/fullscreen | gpu_spatial_aa_ms | -0.009 | -2.4 | within threshold |
| giant/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_present_ms | -0.002 | -2.6 | within threshold |
| moon/window | cpu_submit_and_wait_ms | +0.013 | +0.5 | within threshold |
| moon/window | gpu_ms | +0.056 | +2.9 | within threshold |
| moon/window | cpu_prepare_ms | -0.013 | -5.5 | within threshold |
| moon/window | gpu_cull_shadow_ms | +0.120 | +23.6 | within threshold |
| moon/window | gpu_surface_ms | +0.044 | +7.2 | within threshold |
| moon/window | gpu_atmosphere_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_post_ms | -0.004 | -0.7 | within threshold |
| moon/window | gpu_cull_ms | +0.012 | +10.2 | within threshold |
| moon/window | gpu_body_shadow_ms | -0.000 | -1.6 | within threshold |
| moon/window | gpu_belt_light_ms | +0.115 | +31.3 | within threshold |
| moon/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_sky_ms | -0.001 | -0.4 | within threshold |
| moon/window | gpu_surface_bodies_ms | -0.002 | -1.5 | within threshold |
| moon/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| moon/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_belt_dust_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_temporal_ms | +0.004 | +2.1 | within threshold |
| moon/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_bloom_ms | -0.003 | -3.9 | within threshold |
| moon/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_composite_ms | -0.002 | -1.2 | within threshold |
| moon/window | gpu_spatial_aa_ms | -0.002 | -1.5 | within threshold |
| moon/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_present_ms | -0.001 | -4.0 | within threshold |
| moon/fullscreen | cpu_submit_and_wait_ms | +0.122 | +2.2 | within threshold |
| moon/fullscreen | gpu_ms | +0.192 | +4.0 | within threshold |
| moon/fullscreen | cpu_prepare_ms | -0.022 | -8.1 | within threshold |
| moon/fullscreen | gpu_cull_shadow_ms | +0.009 | +1.4 | within threshold |
| moon/fullscreen | gpu_surface_ms | +0.160 | +8.6 | within threshold |
| moon/fullscreen | gpu_atmosphere_ms | +0.002 | +0.6 | within threshold |
| moon/fullscreen | gpu_post_ms | +0.009 | +0.4 | within threshold |
| moon/fullscreen | gpu_cull_ms | +0.012 | +10.1 | within threshold |
| moon/fullscreen | gpu_body_shadow_ms | -0.000 | -0.9 | within threshold |
| moon/fullscreen | gpu_belt_light_ms | -0.004 | -0.8 | within threshold |
| moon/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_sky_ms | +0.003 | +0.3 | within threshold |
| moon/fullscreen | gpu_surface_bodies_ms | -0.002 | -0.8 | within threshold |
| moon/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| moon/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_belt_dust_ms | +0.001 | +0.3 | within threshold |
| moon/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_temporal_ms | +0.019 | +3.0 | within threshold |
| moon/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_bloom_ms | -0.001 | -0.4 | within threshold |
| moon/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_composite_ms | -0.004 | -0.7 | within threshold |
| moon/fullscreen | gpu_spatial_aa_ms | -0.007 | -1.8 | within threshold |
| moon/fullscreen | gpu_meter_ms | -0.000 | -2.1 | within threshold |
| moon/fullscreen | gpu_present_ms | -0.001 | -1.4 | within threshold |
| mars/window | cpu_submit_and_wait_ms | +0.041 | +1.4 | within threshold |
| mars/window | gpu_ms | +0.054 | +2.4 | within threshold |
| mars/window | cpu_prepare_ms | -0.013 | -5.2 | within threshold |
| mars/window | gpu_cull_shadow_ms | +0.011 | +1.9 | within threshold |
| mars/window | gpu_surface_ms | +0.048 | +7.8 | within threshold |
| mars/window | gpu_atmosphere_ms | -0.002 | -0.5 | within threshold |
| mars/window | gpu_post_ms | -0.003 | -0.5 | within threshold |
| mars/window | gpu_cull_ms | +0.011 | +10.0 | within threshold |
| mars/window | gpu_body_shadow_ms | -0.001 | -2.8 | within threshold |
| mars/window | gpu_belt_light_ms | +0.116 | +31.6 | within threshold |
| mars/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_sky_ms | -0.002 | -0.8 | within threshold |
| mars/window | gpu_surface_bodies_ms | -0.002 | -1.7 | within threshold |
| mars/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| mars/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_atmospheres_ms | -0.002 | -0.6 | within threshold |
| mars/window | gpu_belt_dust_ms | -0.001 | -1.5 | within threshold |
| mars/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_temporal_ms | +0.004 | +2.1 | within threshold |
| mars/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_bloom_ms | -0.002 | -2.6 | within threshold |
| mars/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_composite_ms | -0.002 | -1.2 | within threshold |
| mars/window | gpu_spatial_aa_ms | -0.002 | -1.3 | within threshold |
| mars/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_present_ms | -0.001 | -4.0 | within threshold |
| mars/fullscreen | cpu_submit_and_wait_ms | +0.108 | +1.6 | within threshold |
| mars/fullscreen | gpu_ms | +0.206 | +3.6 | within threshold |
| mars/fullscreen | cpu_prepare_ms | -0.101 | -27.5 | within threshold |
| mars/fullscreen | gpu_cull_shadow_ms | +0.019 | +2.8 | within threshold |
| mars/fullscreen | gpu_surface_ms | +0.229 | +12.4 | regression |
| mars/fullscreen | gpu_atmosphere_ms | +0.001 | +0.1 | within threshold |
| mars/fullscreen | gpu_post_ms | -0.001 | -0.1 | within threshold |
| mars/fullscreen | gpu_cull_ms | +0.020 | +13.3 | within threshold |
| mars/fullscreen | gpu_body_shadow_ms | -0.000 | -2.1 | within threshold |
| mars/fullscreen | gpu_belt_light_ms | +0.001 | +0.2 | within threshold |
| mars/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_surface_prepass_ms | +0.111 | +10850.0 | within threshold |
| mars/fullscreen | gpu_surface_sky_ms | -0.001 | -0.1 | within threshold |
| mars/fullscreen | gpu_surface_bodies_ms | -0.003 | -1.2 | within threshold |
| mars/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_belt_dust_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_temporal_ms | +0.018 | +2.8 | within threshold |
| mars/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_bloom_ms | -0.002 | -0.8 | within threshold |
| mars/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_composite_ms | -0.008 | -1.4 | within threshold |
| mars/fullscreen | gpu_spatial_aa_ms | -0.007 | -2.1 | within threshold |
| mars/fullscreen | gpu_meter_ms | -0.000 | -0.1 | within threshold |
| mars/fullscreen | gpu_present_ms | -0.001 | -1.4 | within threshold |
| dawn/window | cpu_submit_and_wait_ms | +0.023 | +0.6 | within threshold |
| dawn/window | gpu_ms | +0.043 | +1.5 | within threshold |
| dawn/window | cpu_prepare_ms | -0.014 | -5.3 | within threshold |
| dawn/window | gpu_cull_shadow_ms | +0.007 | +1.4 | within threshold |
| dawn/window | gpu_surface_ms | +0.044 | +5.9 | within threshold |
| dawn/window | gpu_atmosphere_ms | -0.006 | -0.7 | within threshold |
| dawn/window | gpu_post_ms | -0.008 | -1.2 | within threshold |
| dawn/window | gpu_cull_ms | +0.012 | +10.6 | within threshold |
| dawn/window | gpu_body_shadow_ms | -0.000 | -1.2 | within threshold |
| dawn/window | gpu_belt_light_ms | -0.004 | -1.1 | within threshold |
| dawn/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_sky_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_bodies_ms | -0.004 | -1.2 | within threshold |
| dawn/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| dawn/window | gpu_surface_clouds_ms | -0.002 | -1.0 | within threshold |
| dawn/window | gpu_atmospheres_ms | -0.005 | -0.6 | within threshold |
| dawn/window | gpu_belt_dust_ms | -0.001 | -1.5 | within threshold |
| dawn/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_temporal_ms | +0.004 | +2.0 | within threshold |
| dawn/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_bloom_ms | -0.006 | -6.2 | within threshold |
| dawn/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_composite_ms | -0.002 | -1.1 | within threshold |
| dawn/window | gpu_spatial_aa_ms | -0.003 | -2.5 | within threshold |
| dawn/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_present_ms | -0.001 | -4.0 | within threshold |
| dawn/fullscreen | cpu_submit_and_wait_ms | +0.131 | +1.6 | within threshold |
| dawn/fullscreen | gpu_ms | +0.154 | +2.1 | within threshold |
| dawn/fullscreen | cpu_prepare_ms | +0.005 | +1.7 | within threshold |
| dawn/fullscreen | gpu_cull_shadow_ms | +0.002 | +0.4 | within threshold |
| dawn/fullscreen | gpu_surface_ms | +0.067 | +3.1 | within threshold |
| dawn/fullscreen | gpu_atmosphere_ms | +0.002 | +0.1 | within threshold |
| dawn/fullscreen | gpu_post_ms | -0.002 | -0.1 | within threshold |
| dawn/fullscreen | gpu_cull_ms | +0.013 | +11.2 | within threshold |
| dawn/fullscreen | gpu_body_shadow_ms | -0.000 | -1.2 | within threshold |
| dawn/fullscreen | gpu_belt_light_ms | -0.012 | -2.4 | within threshold |
| dawn/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_sky_ms | +0.003 | +0.5 | within threshold |
| dawn/fullscreen | gpu_surface_bodies_ms | -0.059 | -7.5 | within threshold |
| dawn/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_clouds_ms | -0.003 | -0.7 | within threshold |
| dawn/fullscreen | gpu_atmospheres_ms | -0.001 | -0.0 | within threshold |
| dawn/fullscreen | gpu_belt_dust_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_temporal_ms | +0.020 | +3.0 | within threshold |
| dawn/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_bloom_ms | -0.006 | -2.7 | within threshold |
| dawn/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_composite_ms | -0.006 | -1.0 | within threshold |
| dawn/fullscreen | gpu_spatial_aa_ms | -0.007 | -2.0 | within threshold |
| dawn/fullscreen | gpu_meter_ms | -0.001 | -4.4 | within threshold |
| dawn/fullscreen | gpu_present_ms | -0.001 | -1.4 | within threshold |
| belt/window | cpu_submit_and_wait_ms | -0.001 | -0.0 | within threshold |
| belt/window | gpu_ms | +0.085 | +2.2 | within threshold |
| belt/window | cpu_prepare_ms | -0.089 | -26.7 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.024 | +4.1 | within threshold |
| belt/window | gpu_surface_ms | +0.071 | +6.7 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.010 | -0.7 | within threshold |
| belt/window | gpu_post_ms | -0.004 | -0.6 | within threshold |
| belt/window | gpu_cull_ms | +0.030 | +15.1 | within threshold |
| belt/window | gpu_body_shadow_ms | -0.000 | -1.8 | within threshold |
| belt/window | gpu_belt_light_ms | -0.004 | -1.1 | within threshold |
| belt/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_sky_ms | -0.001 | -0.9 | within threshold |
| belt/window | gpu_surface_bodies_ms | -0.004 | -0.9 | within threshold |
| belt/window | gpu_surface_rocks_ms | -0.002 | -0.6 | within threshold |
| belt/window | gpu_surface_splats_ms | -0.001 | -2.1 | within threshold |
| belt/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_atmospheres_ms | -0.005 | -0.6 | within threshold |
| belt/window | gpu_belt_dust_ms | -0.005 | -0.9 | within threshold |
| belt/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_temporal_ms | +0.004 | +1.9 | within threshold |
| belt/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_bloom_ms | -0.003 | -3.5 | within threshold |
| belt/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_composite_ms | -0.002 | -1.1 | within threshold |
| belt/window | gpu_spatial_aa_ms | -0.002 | -1.4 | within threshold |
| belt/window | gpu_meter_ms | -0.001 | -12.1 | within threshold |
| belt/window | gpu_present_ms | -0.001 | -2.7 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.278 | +2.4 | within threshold |
| belt/fullscreen | gpu_ms | +0.377 | +3.5 | within threshold |
| belt/fullscreen | cpu_prepare_ms | -0.042 | -12.3 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.059 | +8.5 | within threshold |
| belt/fullscreen | gpu_surface_ms | +0.255 | +8.0 | regression |
| belt/fullscreen | gpu_atmosphere_ms | +0.007 | +0.2 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.008 | +0.3 | within threshold |
| belt/fullscreen | gpu_cull_ms | +0.039 | +18.5 | within threshold |
| belt/fullscreen | gpu_body_shadow_ms | -0.000 | -2.3 | within threshold |
| belt/fullscreen | gpu_belt_light_ms | +0.021 | +4.5 | within threshold |
| belt/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_sky_ms | -0.001 | -0.1 | within threshold |
| belt/fullscreen | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_rocks_ms | -0.002 | -0.1 | within threshold |
| belt/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_atmospheres_ms | -0.002 | -0.1 | within threshold |
| belt/fullscreen | gpu_belt_dust_ms | -0.006 | -0.3 | within threshold |
| belt/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_temporal_ms | +0.022 | +3.0 | within threshold |
| belt/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_bloom_ms | -0.001 | -0.4 | within threshold |
| belt/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_composite_ms | -0.004 | -0.7 | within threshold |
| belt/fullscreen | gpu_spatial_aa_ms | -0.008 | -1.7 | within threshold |
| belt/fullscreen | gpu_meter_ms | -0.001 | -5.6 | within threshold |
| belt/fullscreen | gpu_present_ms | -0.001 | -1.5 | within threshold |
| belt-sun/window | cpu_submit_and_wait_ms | +0.105 | +3.2 | within threshold |
| belt-sun/window | gpu_ms | +0.206 | +8.1 | regression |
| belt-sun/window | cpu_prepare_ms | -0.009 | -3.6 | within threshold |
| belt-sun/window | gpu_cull_shadow_ms | +0.026 | +4.5 | within threshold |
| belt-sun/window | gpu_surface_ms | +0.081 | +15.2 | within threshold |
| belt-sun/window | gpu_atmosphere_ms | -0.002 | -0.3 | within threshold |
| belt-sun/window | gpu_post_ms | -0.001 | -0.2 | within threshold |
| belt-sun/window | gpu_cull_ms | +0.030 | +14.9 | within threshold |
| belt-sun/window | gpu_body_shadow_ms | -0.000 | -0.8 | within threshold |
| belt-sun/window | gpu_belt_light_ms | -0.003 | -0.8 | within threshold |
| belt-sun/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_sky_ms | +0.001 | +0.4 | within threshold |
| belt-sun/window | gpu_surface_bodies_ms | -0.001 | -50.0 | within threshold |
| belt-sun/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_splats_ms | +0.002 | +2.8 | within threshold |
| belt-sun/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_belt_dust_ms | -0.003 | -0.5 | within threshold |
| belt-sun/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_temporal_ms | +0.006 | +3.1 | within threshold |
| belt-sun/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_bloom_ms | -0.003 | -3.9 | within threshold |
| belt-sun/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_flare_ms | -0.001 | -16.7 | within threshold |
| belt-sun/window | gpu_composite_ms | -0.002 | -1.2 | within threshold |
| belt-sun/window | gpu_spatial_aa_ms | -0.001 | -0.9 | within threshold |
| belt-sun/window | gpu_meter_ms | -0.001 | -12.5 | within threshold |
| belt-sun/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | cpu_submit_and_wait_ms | +0.264 | +3.4 | within threshold |
| belt-sun/fullscreen | gpu_ms | +0.481 | +7.1 | regression |
| belt-sun/fullscreen | cpu_prepare_ms | -0.050 | -14.8 | within threshold |
| belt-sun/fullscreen | gpu_cull_shadow_ms | +0.050 | +7.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_ms | +0.336 | +19.6 | regression |
| belt-sun/fullscreen | gpu_atmosphere_ms | +0.016 | +0.7 | within threshold |
| belt-sun/fullscreen | gpu_post_ms | +0.008 | +0.4 | within threshold |
| belt-sun/fullscreen | gpu_cull_ms | +0.038 | +18.3 | within threshold |
| belt-sun/fullscreen | gpu_body_shadow_ms | -0.000 | -0.9 | within threshold |
| belt-sun/fullscreen | gpu_belt_light_ms | +0.011 | +2.2 | within threshold |
| belt-sun/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_sky_ms | +0.002 | +0.2 | within threshold |
| belt-sun/fullscreen | gpu_surface_bodies_ms | +0.001 | +7.7 | within threshold |
| belt-sun/fullscreen | gpu_surface_rocks_ms | +0.018 | +3.7 | within threshold |
| belt-sun/fullscreen | gpu_surface_splats_ms | +0.002 | +1.8 | within threshold |
| belt-sun/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_atmospheres_ms | +0.002 | +0.7 | within threshold |
| belt-sun/fullscreen | gpu_belt_dust_ms | +0.009 | +0.5 | within threshold |
| belt-sun/fullscreen | gpu_splat_mask_ms | +0.005 | +4.9 | within threshold |
| belt-sun/fullscreen | gpu_temporal_ms | +0.023 | +3.4 | within threshold |
| belt-sun/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_bloom_ms | -0.002 | -0.9 | within threshold |
| belt-sun/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_composite_ms | -0.005 | -0.8 | within threshold |
| belt-sun/fullscreen | gpu_spatial_aa_ms | -0.007 | -1.8 | within threshold |
| belt-sun/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_present_ms | -0.001 | -1.3 | within threshold |

## Compared with headless-baseline

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| earth/window | cpu_submit_and_wait_ms | +0.031 | +0.8 | within threshold |
| earth/window | gpu_ms | +0.163 | +5.2 | within threshold |
| earth/window | cpu_prepare_ms | -0.011 | -4.2 | within threshold |
| earth/window | gpu_cull_shadow_ms | +0.043 | +7.9 | within threshold |
| earth/window | gpu_surface_ms | +0.049 | +5.1 | within threshold |
| earth/window | gpu_atmosphere_ms | -0.039 | -3.8 | within threshold |
| earth/window | gpu_post_ms | +0.031 | +5.1 | within threshold |
| earth/window | gpu_cull_ms | +0.036 | +22.8 | within threshold |
| earth/window | gpu_body_shadow_ms | +0.000 | +2.5 | within threshold |
| earth/window | gpu_belt_light_ms | +0.001 | +0.3 | within threshold |
| earth/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_sky_ms | +0.007 | +3.4 | within threshold |
| earth/window | gpu_surface_bodies_ms | -0.012 | -3.7 | within threshold |
| earth/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_clouds_ms | +0.003 | +1.8 | within threshold |
| earth/window | gpu_atmospheres_ms | -0.034 | -4.2 | within threshold |
| earth/window | gpu_belt_dust_ms | -0.001 | -0.5 | within threshold |
| earth/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_temporal_ms | +0.013 | +6.8 | within threshold |
| earth/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_bloom_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_composite_ms | +0.016 | +10.7 | within threshold |
| earth/window | gpu_spatial_aa_ms | +0.001 | +0.8 | within threshold |
| earth/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | cpu_submit_and_wait_ms | +0.075 | +0.8 | within threshold |
| earth/fullscreen | gpu_ms | +0.172 | +2.1 | within threshold |
| earth/fullscreen | cpu_prepare_ms | -0.009 | -2.9 | within threshold |
| earth/fullscreen | gpu_cull_shadow_ms | +0.044 | +6.6 | within threshold |
| earth/fullscreen | gpu_surface_ms | +0.204 | +8.1 | regression |
| earth/fullscreen | gpu_atmosphere_ms | -0.246 | -8.4 | within threshold |
| earth/fullscreen | gpu_post_ms | +0.124 | +6.6 | within threshold |
| earth/fullscreen | gpu_cull_ms | +0.040 | +24.7 | within threshold |
| earth/fullscreen | gpu_body_shadow_ms | +0.001 | +5.6 | within threshold |
| earth/fullscreen | gpu_belt_light_ms | +0.006 | +1.3 | within threshold |
| earth/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_surface_prepass_ms | -0.001 | -33.3 | within threshold |
| earth/fullscreen | gpu_surface_sky_ms | +0.028 | +3.1 | within threshold |
| earth/fullscreen | gpu_surface_bodies_ms | -0.014 | -2.1 | within threshold |
| earth/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_splats_ms | +0.001 | +33.3 | within threshold |
| earth/fullscreen | gpu_surface_clouds_ms | +0.009 | +2.5 | within threshold |
| earth/fullscreen | gpu_atmospheres_ms | -0.262 | -11.1 | within threshold |
| earth/fullscreen | gpu_belt_dust_ms | +0.014 | +2.5 | within threshold |
| earth/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_temporal_ms | +0.054 | +8.6 | within threshold |
| earth/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_bloom_ms | +0.004 | +1.7 | within threshold |
| earth/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_flare_ms | +0.001 | +20.0 | within threshold |
| earth/fullscreen | gpu_composite_ms | +0.057 | +11.2 | within threshold |
| earth/fullscreen | gpu_spatial_aa_ms | +0.006 | +1.6 | within threshold |
| earth/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_present_ms | +0.001 | +1.4 | within threshold |
| giant/window | cpu_submit_and_wait_ms | -0.009 | -0.2 | within threshold |
| giant/window | gpu_ms | +0.017 | +0.4 | within threshold |
| giant/window | cpu_prepare_ms | -0.010 | -3.8 | within threshold |
| giant/window | gpu_cull_shadow_ms | +0.046 | +8.0 | within threshold |
| giant/window | gpu_surface_ms | +0.035 | +3.8 | within threshold |
| giant/window | gpu_atmosphere_ms | -0.097 | -5.2 | within threshold |
| giant/window | gpu_post_ms | +0.032 | +5.3 | within threshold |
| giant/window | gpu_cull_ms | +0.045 | +21.9 | within threshold |
| giant/window | gpu_body_shadow_ms | +0.001 | +3.7 | within threshold |
| giant/window | gpu_belt_light_ms | +0.001 | +0.3 | within threshold |
| giant/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_surface_sky_ms | +0.008 | +3.4 | within threshold |
| giant/window | gpu_surface_bodies_ms | -0.022 | -6.9 | within threshold |
| giant/window | gpu_surface_rocks_ms | -0.002 | -5.3 | within threshold |
| giant/window | gpu_surface_splats_ms | +0.001 | +1.0 | within threshold |
| giant/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_atmospheres_ms | -0.002 | -0.4 | within threshold |
| giant/window | gpu_belt_dust_ms | -0.095 | -7.9 | within threshold |
| giant/window | gpu_splat_mask_ms | +0.001 | +0.9 | within threshold |
| giant/window | gpu_temporal_ms | +0.012 | +6.2 | within threshold |
| giant/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_bloom_ms | +0.001 | +1.3 | within threshold |
| giant/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_composite_ms | +0.016 | +10.7 | within threshold |
| giant/window | gpu_spatial_aa_ms | +0.001 | +0.8 | within threshold |
| giant/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | cpu_submit_and_wait_ms | -0.117 | -1.0 | within threshold |
| giant/fullscreen | gpu_ms | +0.034 | +0.3 | within threshold |
| giant/fullscreen | cpu_prepare_ms | -0.070 | -18.7 | within threshold |
| giant/fullscreen | gpu_cull_shadow_ms | +0.076 | +10.9 | within threshold |
| giant/fullscreen | gpu_surface_ms | +0.257 | +10.6 | regression |
| giant/fullscreen | gpu_atmosphere_ms | -0.423 | -7.1 | within threshold |
| giant/fullscreen | gpu_post_ms | +0.117 | +6.2 | within threshold |
| giant/fullscreen | gpu_cull_ms | +0.054 | +25.0 | within threshold |
| giant/fullscreen | gpu_body_shadow_ms | +0.001 | +3.8 | within threshold |
| giant/fullscreen | gpu_belt_light_ms | +0.115 | +31.0 | within threshold |
| giant/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_surface_sky_ms | +0.032 | +3.3 | within threshold |
| giant/fullscreen | gpu_surface_bodies_ms | +0.040 | +6.5 | within threshold |
| giant/fullscreen | gpu_surface_rocks_ms | -0.001 | -0.9 | within threshold |
| giant/fullscreen | gpu_surface_splats_ms | +0.006 | +3.4 | within threshold |
| giant/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_atmospheres_ms | -0.154 | -9.4 | within threshold |
| giant/fullscreen | gpu_belt_dust_ms | -0.301 | -7.3 | within threshold |
| giant/fullscreen | gpu_splat_mask_ms | +0.008 | +4.4 | within threshold |
| giant/fullscreen | gpu_temporal_ms | +0.051 | +7.8 | within threshold |
| giant/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_bloom_ms | +0.004 | +1.6 | within threshold |
| giant/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_flare_ms | +0.001 | +9.1 | within threshold |
| giant/fullscreen | gpu_composite_ms | +0.056 | +11.0 | within threshold |
| giant/fullscreen | gpu_spatial_aa_ms | +0.005 | +1.4 | within threshold |
| giant/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_present_ms | +0.001 | +1.4 | within threshold |
| moon/window | cpu_submit_and_wait_ms | +0.094 | +3.8 | within threshold |
| moon/window | gpu_ms | +0.120 | +6.4 | within threshold |
| moon/window | cpu_prepare_ms | -0.010 | -4.0 | within threshold |
| moon/window | gpu_cull_shadow_ms | +0.018 | +3.0 | within threshold |
| moon/window | gpu_surface_ms | +0.051 | +8.5 | within threshold |
| moon/window | gpu_atmosphere_ms | +0.016 | +27.6 | within threshold |
| moon/window | gpu_post_ms | +0.034 | +5.7 | within threshold |
| moon/window | gpu_cull_ms | +0.019 | +17.9 | within threshold |
| moon/window | gpu_body_shadow_ms | +0.001 | +4.1 | within threshold |
| moon/window | gpu_belt_light_ms | +0.057 | +13.4 | within threshold |
| moon/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_sky_ms | +0.008 | +3.7 | within threshold |
| moon/window | gpu_surface_bodies_ms | -0.007 | -5.2 | within threshold |
| moon/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| moon/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_belt_dust_ms | +0.016 | +30.8 | within threshold |
| moon/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_temporal_ms | +0.014 | +7.6 | within threshold |
| moon/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_bloom_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_composite_ms | +0.016 | +10.7 | within threshold |
| moon/window | gpu_spatial_aa_ms | +0.002 | +1.6 | within threshold |
| moon/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | cpu_submit_and_wait_ms | +0.328 | +6.1 | regression |
| moon/fullscreen | gpu_ms | +0.435 | +9.5 | regression |
| moon/fullscreen | cpu_prepare_ms | -0.105 | -29.5 | within threshold |
| moon/fullscreen | gpu_cull_shadow_ms | +0.026 | +4.2 | within threshold |
| moon/fullscreen | gpu_surface_ms | +0.249 | +14.1 | regression |
| moon/fullscreen | gpu_atmosphere_ms | +0.050 | +16.4 | within threshold |
| moon/fullscreen | gpu_post_ms | +0.122 | +6.5 | within threshold |
| moon/fullscreen | gpu_cull_ms | +0.020 | +18.1 | within threshold |
| moon/fullscreen | gpu_body_shadow_ms | +0.001 | +5.0 | within threshold |
| moon/fullscreen | gpu_belt_light_ms | +0.006 | +1.3 | within threshold |
| moon/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_surface_prepass_ms | +0.112 | +10900.0 | within threshold |
| moon/fullscreen | gpu_surface_sky_ms | +0.022 | +2.4 | within threshold |
| moon/fullscreen | gpu_surface_bodies_ms | -0.003 | -1.1 | within threshold |
| moon/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| moon/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_belt_dust_ms | +0.049 | +16.6 | within threshold |
| moon/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_temporal_ms | +0.053 | +8.5 | within threshold |
| moon/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_bloom_ms | +0.003 | +1.3 | within threshold |
| moon/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_composite_ms | +0.056 | +11.1 | within threshold |
| moon/fullscreen | gpu_spatial_aa_ms | +0.006 | +1.6 | within threshold |
| moon/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_present_ms | +0.001 | +1.4 | within threshold |
| mars/window | cpu_submit_and_wait_ms | +0.120 | +4.2 | within threshold |
| mars/window | gpu_ms | +0.214 | +10.1 | regression |
| mars/window | cpu_prepare_ms | -0.011 | -4.5 | within threshold |
| mars/window | gpu_cull_shadow_ms | +0.140 | +28.8 | within threshold |
| mars/window | gpu_surface_ms | +0.057 | +9.4 | within threshold |
| mars/window | gpu_atmosphere_ms | +0.007 | +1.7 | within threshold |
| mars/window | gpu_post_ms | +0.034 | +6.0 | within threshold |
| mars/window | gpu_cull_ms | +0.019 | +17.8 | within threshold |
| mars/window | gpu_body_shadow_ms | +0.000 | +2.0 | within threshold |
| mars/window | gpu_belt_light_ms | +0.122 | +33.8 | within threshold |
| mars/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_sky_ms | +0.008 | +3.3 | within threshold |
| mars/window | gpu_surface_bodies_ms | -0.003 | -2.5 | within threshold |
| mars/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| mars/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_atmospheres_ms | -0.008 | -2.2 | within threshold |
| mars/window | gpu_belt_dust_ms | +0.015 | +28.8 | within threshold |
| mars/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_temporal_ms | +0.016 | +8.9 | within threshold |
| mars/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_bloom_ms | +0.001 | +1.4 | within threshold |
| mars/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_composite_ms | +0.017 | +11.5 | within threshold |
| mars/window | gpu_spatial_aa_ms | +0.002 | +1.9 | within threshold |
| mars/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | cpu_submit_and_wait_ms | +0.144 | +2.2 | within threshold |
| mars/fullscreen | gpu_ms | +0.331 | +5.8 | regression |
| mars/fullscreen | cpu_prepare_ms | -0.109 | -29.1 | within threshold |
| mars/fullscreen | gpu_cull_shadow_ms | +0.040 | +6.2 | within threshold |
| mars/fullscreen | gpu_surface_ms | +0.283 | +15.8 | regression |
| mars/fullscreen | gpu_atmosphere_ms | -0.137 | -9.3 | within threshold |
| mars/fullscreen | gpu_post_ms | +0.124 | +6.9 | within threshold |
| mars/fullscreen | gpu_cull_ms | +0.029 | +20.0 | within threshold |
| mars/fullscreen | gpu_body_shadow_ms | +0.000 | +2.5 | within threshold |
| mars/fullscreen | gpu_belt_light_ms | +0.127 | +34.6 | within threshold |
| mars/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_surface_prepass_ms | +0.111 | +10850.0 | within threshold |
| mars/fullscreen | gpu_surface_sky_ms | +0.026 | +2.6 | within threshold |
| mars/fullscreen | gpu_surface_bodies_ms | +0.004 | +1.7 | within threshold |
| mars/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_atmospheres_ms | -0.183 | -16.2 | within threshold |
| mars/fullscreen | gpu_belt_dust_ms | +0.043 | +13.0 | within threshold |
| mars/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_temporal_ms | +0.057 | +9.3 | within threshold |
| mars/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_bloom_ms | +0.002 | +0.9 | within threshold |
| mars/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_flare_ms | +0.001 | +9.1 | within threshold |
| mars/fullscreen | gpu_composite_ms | +0.056 | +11.1 | within threshold |
| mars/fullscreen | gpu_spatial_aa_ms | +0.005 | +1.5 | within threshold |
| mars/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_present_ms | +0.001 | +1.4 | within threshold |
| dawn/window | cpu_submit_and_wait_ms | +0.105 | +3.0 | within threshold |
| dawn/window | gpu_ms | +0.101 | +3.6 | within threshold |
| dawn/window | cpu_prepare_ms | -0.010 | -3.9 | within threshold |
| dawn/window | gpu_cull_shadow_ms | +0.021 | +4.3 | within threshold |
| dawn/window | gpu_surface_ms | +0.045 | +6.0 | within threshold |
| dawn/window | gpu_atmosphere_ms | -0.002 | -0.2 | within threshold |
| dawn/window | gpu_post_ms | +0.033 | +5.1 | within threshold |
| dawn/window | gpu_cull_ms | +0.019 | +18.3 | within threshold |
| dawn/window | gpu_body_shadow_ms | +0.001 | +4.4 | within threshold |
| dawn/window | gpu_belt_light_ms | +0.001 | +0.3 | within threshold |
| dawn/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_sky_ms | +0.002 | +1.6 | within threshold |
| dawn/window | gpu_surface_bodies_ms | -0.009 | -2.7 | within threshold |
| dawn/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| dawn/window | gpu_surface_clouds_ms | +0.003 | +1.6 | within threshold |
| dawn/window | gpu_atmospheres_ms | -0.017 | -2.0 | within threshold |
| dawn/window | gpu_belt_dust_ms | +0.015 | +29.4 | within threshold |
| dawn/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_temporal_ms | +0.013 | +6.7 | within threshold |
| dawn/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_bloom_ms | +0.001 | +1.1 | within threshold |
| dawn/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_composite_ms | +0.017 | +11.0 | within threshold |
| dawn/window | gpu_spatial_aa_ms | +0.001 | +0.9 | within threshold |
| dawn/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | cpu_submit_and_wait_ms | +0.015 | +0.2 | within threshold |
| dawn/fullscreen | gpu_ms | +0.208 | +2.8 | within threshold |
| dawn/fullscreen | cpu_prepare_ms | -0.087 | -22.9 | within threshold |
| dawn/fullscreen | gpu_cull_shadow_ms | +0.029 | +4.7 | within threshold |
| dawn/fullscreen | gpu_surface_ms | +0.224 | +11.2 | regression |
| dawn/fullscreen | gpu_atmosphere_ms | -0.233 | -8.4 | within threshold |
| dawn/fullscreen | gpu_post_ms | +0.121 | +6.4 | within threshold |
| dawn/fullscreen | gpu_cull_ms | +0.021 | +19.3 | within threshold |
| dawn/fullscreen | gpu_body_shadow_ms | +0.001 | +4.8 | within threshold |
| dawn/fullscreen | gpu_belt_light_ms | +0.122 | +32.7 | within threshold |
| dawn/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_sky_ms | +0.016 | +2.6 | within threshold |
| dawn/fullscreen | gpu_surface_bodies_ms | +0.013 | +1.8 | within threshold |
| dawn/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_clouds_ms | +0.012 | +2.6 | within threshold |
| dawn/fullscreen | gpu_atmospheres_ms | -0.293 | -11.2 | within threshold |
| dawn/fullscreen | gpu_belt_dust_ms | +0.057 | +33.1 | within threshold |
| dawn/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_temporal_ms | +0.053 | +8.2 | within threshold |
| dawn/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_bloom_ms | +0.003 | +1.4 | within threshold |
| dawn/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_flare_ms | +0.001 | +4.5 | within threshold |
| dawn/fullscreen | gpu_composite_ms | +0.059 | +11.2 | within threshold |
| dawn/fullscreen | gpu_spatial_aa_ms | +0.006 | +1.7 | within threshold |
| dawn/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_present_ms | +0.001 | +1.4 | within threshold |
| belt/window | cpu_submit_and_wait_ms | +0.061 | +1.3 | within threshold |
| belt/window | gpu_ms | +0.146 | +3.9 | within threshold |
| belt/window | cpu_prepare_ms | -0.016 | -6.1 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.038 | +6.8 | within threshold |
| belt/window | gpu_surface_ms | +0.103 | +10.0 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.029 | -1.9 | within threshold |
| belt/window | gpu_post_ms | +0.027 | +4.0 | within threshold |
| belt/window | gpu_cull_ms | +0.037 | +19.9 | within threshold |
| belt/window | gpu_body_shadow_ms | +0.001 | +3.7 | within threshold |
| belt/window | gpu_belt_light_ms | +0.001 | +0.3 | within threshold |
| belt/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_sky_ms | +0.003 | +2.9 | within threshold |
| belt/window | gpu_surface_bodies_ms | +0.027 | +6.5 | within threshold |
| belt/window | gpu_surface_rocks_ms | -0.005 | -1.5 | within threshold |
| belt/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_atmospheres_ms | +0.026 | +3.2 | within threshold |
| belt/window | gpu_belt_dust_ms | -0.055 | -8.5 | within threshold |
| belt/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_temporal_ms | +0.011 | +5.4 | within threshold |
| belt/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_bloom_ms | +0.001 | +1.2 | within threshold |
| belt/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_composite_ms | +0.017 | +11.0 | within threshold |
| belt/window | gpu_spatial_aa_ms | +0.002 | +1.1 | within threshold |
| belt/window | gpu_meter_ms | +0.000 | +0.4 | within threshold |
| belt/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.583 | +5.1 | regression |
| belt/fullscreen | gpu_ms | +0.752 | +7.2 | regression |
| belt/fullscreen | cpu_prepare_ms | -0.104 | -25.6 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.171 | +29.3 | within threshold |
| belt/fullscreen | gpu_surface_ms | +0.422 | +13.9 | regression |
| belt/fullscreen | gpu_atmosphere_ms | -0.023 | -0.5 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.127 | +6.1 | within threshold |
| belt/fullscreen | gpu_cull_ms | +0.047 | +23.3 | within threshold |
| belt/fullscreen | gpu_body_shadow_ms | +0.001 | +3.5 | within threshold |
| belt/fullscreen | gpu_belt_light_ms | +0.125 | +34.5 | within threshold |
| belt/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_sky_ms | +0.010 | +2.0 | within threshold |
| belt/fullscreen | gpu_surface_bodies_ms | +0.110 | +12.0 | within threshold |
| belt/fullscreen | gpu_surface_rocks_ms | -0.003 | -0.2 | within threshold |
| belt/fullscreen | gpu_surface_splats_ms | +0.002 | +3.1 | within threshold |
| belt/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_atmospheres_ms | +0.122 | +5.0 | within threshold |
| belt/fullscreen | gpu_belt_dust_ms | -0.174 | -8.1 | within threshold |
| belt/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_temporal_ms | +0.046 | +6.7 | within threshold |
| belt/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_bloom_ms | +0.003 | +0.9 | within threshold |
| belt/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_flare_ms | +0.001 | +4.5 | within threshold |
| belt/fullscreen | gpu_composite_ms | +0.061 | +11.6 | within threshold |
| belt/fullscreen | gpu_spatial_aa_ms | +0.007 | +1.7 | within threshold |
| belt/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_present_ms | +0.001 | +1.2 | within threshold |
| belt-sun/window | cpu_submit_and_wait_ms | +0.031 | +0.9 | within threshold |
| belt-sun/window | gpu_ms | +0.125 | +4.8 | within threshold |
| belt-sun/window | cpu_prepare_ms | -0.007 | -2.8 | within threshold |
| belt-sun/window | gpu_cull_shadow_ms | +0.042 | +7.4 | within threshold |
| belt-sun/window | gpu_surface_ms | +0.087 | +16.6 | within threshold |
| belt-sun/window | gpu_atmosphere_ms | -0.140 | -15.2 | within threshold |
| belt-sun/window | gpu_post_ms | +0.036 | +5.9 | within threshold |
| belt-sun/window | gpu_cull_ms | +0.038 | +20.0 | within threshold |
| belt-sun/window | gpu_body_shadow_ms | +0.001 | +4.1 | within threshold |
| belt-sun/window | gpu_belt_light_ms | +0.002 | +0.6 | within threshold |
| belt-sun/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_sky_ms | +0.005 | +2.3 | within threshold |
| belt-sun/window | gpu_surface_bodies_ms | -0.001 | -50.0 | within threshold |
| belt-sun/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_splats_ms | +0.003 | +4.3 | within threshold |
| belt-sun/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_atmospheres_ms | -0.085 | -65.4 | within threshold |
| belt-sun/window | gpu_belt_dust_ms | -0.057 | -8.1 | within threshold |
| belt-sun/window | gpu_splat_mask_ms | +0.001 | +1.3 | within threshold |
| belt-sun/window | gpu_temporal_ms | +0.015 | +8.0 | within threshold |
| belt-sun/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_bloom_ms | +0.001 | +1.4 | within threshold |
| belt-sun/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_composite_ms | +0.017 | +11.3 | within threshold |
| belt-sun/window | gpu_spatial_aa_ms | +0.002 | +1.8 | within threshold |
| belt-sun/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | cpu_submit_and_wait_ms | +0.005 | +0.1 | within threshold |
| belt-sun/fullscreen | gpu_ms | +0.111 | +1.6 | within threshold |
| belt-sun/fullscreen | cpu_prepare_ms | -0.020 | -6.6 | within threshold |
| belt-sun/fullscreen | gpu_cull_shadow_ms | +0.049 | +6.9 | within threshold |
| belt-sun/fullscreen | gpu_surface_ms | +0.345 | +20.2 | regression |
| belt-sun/fullscreen | gpu_atmosphere_ms | -0.417 | -14.6 | within threshold |
| belt-sun/fullscreen | gpu_post_ms | +0.129 | +6.9 | within threshold |
| belt-sun/fullscreen | gpu_cull_ms | +0.047 | +23.5 | within threshold |
| belt-sun/fullscreen | gpu_body_shadow_ms | +0.001 | +4.8 | within threshold |
| belt-sun/fullscreen | gpu_belt_light_ms | +0.002 | +0.3 | within threshold |
| belt-sun/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_sky_ms | +0.016 | +1.9 | within threshold |
| belt-sun/fullscreen | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_rocks_ms | +0.016 | +3.3 | within threshold |
| belt-sun/fullscreen | gpu_surface_splats_ms | +0.003 | +2.8 | within threshold |
| belt-sun/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_atmospheres_ms | -0.281 | -49.7 | within threshold |
| belt-sun/fullscreen | gpu_belt_dust_ms | -0.144 | -6.6 | within threshold |
| belt-sun/fullscreen | gpu_splat_mask_ms | +0.007 | +6.9 | within threshold |
| belt-sun/fullscreen | gpu_temporal_ms | +0.058 | +9.3 | within threshold |
| belt-sun/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_bloom_ms | +0.004 | +1.9 | within threshold |
| belt-sun/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_flare_ms | +0.001 | +7.1 | within threshold |
| belt-sun/fullscreen | gpu_composite_ms | +0.058 | +11.2 | within threshold |
| belt-sun/fullscreen | gpu_spatial_aa_ms | +0.006 | +1.6 | within threshold |
| belt-sun/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_present_ms | +0.001 | +1.4 | within threshold |
