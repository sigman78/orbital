# Render performance: body-tiers

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `3ee7c1c91221a4d1b6092ee989abc782cbc0eb7f`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| earth/window | 1600×900 | 3.829 | 3.110 | 3.577 | 0.554 | 0.957 | 0.973 | 0.618 |
| earth/fullscreen | 3440×1440 | 8.755 | 7.883 | 9.004 | 0.695 | 2.642 | 2.655 | 1.917 |
| giant/window | 1600×900 | 4.842 | 3.993 | 4.532 | 0.600 | 0.893 | 1.874 | 0.613 |
| giant/fullscreen | 3440×1440 | 11.951 | 11.114 | 14.012 | 0.733 | 2.527 | 5.885 | 1.948 |
| moon/window | 1600×900 | 2.551 | 1.898 | 2.220 | 0.624 | 0.608 | 0.060 | 0.604 |
| moon/fullscreen | 3440×1440 | 5.444 | 4.760 | 5.231 | 0.645 | 1.876 | 0.310 | 1.922 |
| mars/window | 1600×900 | 2.903 | 2.238 | 2.486 | 0.627 | 0.616 | 0.418 | 0.579 |
| mars/fullscreen | 3440×1440 | 6.460 | 5.655 | 6.186 | 0.671 | 1.836 | 1.279 | 1.845 |
| dawn/window | 1600×900 | 3.577 | 2.940 | 3.268 | 0.629 | 0.762 | 0.894 | 0.656 |
| dawn/fullscreen | 3440×1440 | 7.994 | 7.223 | 7.953 | 0.642 | 2.100 | 2.454 | 1.953 |
| belt/window | 1600×900 | 4.611 | 3.839 | 4.360 | 0.584 | 1.033 | 1.537 | 0.680 |
| belt/fullscreen | 3440×1440 | 11.739 | 10.786 | 15.438 | 0.714 | 3.186 | 4.789 | 2.135 |
| belt-sun/window | 1600×900 | 3.346 | 2.687 | 2.840 | 0.583 | 0.531 | 0.845 | 0.625 |
| belt-sun/fullscreen | 3440×1440 | 7.815 | 6.864 | 7.641 | 0.602 | 1.715 | 2.590 | 1.929 |

## Children

The scopes inside the groups: indicative, since a scope reads where its commands were issued rather than an exact cost.

| Scene / mode | Scope | Median | p95 | Run range |
| --- | --- | ---: | ---: | ---: |
| earth/window | gpu_cull_ms | 0.166 | 0.168 | 0.165 to 0.166 |
| earth/window | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| earth/window | gpu_belt_light_ms | 0.367 | 0.497 | 0.367 to 0.369 |
| earth/window | gpu_belt_disc_ms | 0.000 | 0.115 | 0.000 to 0.000 |
| earth/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| earth/window | gpu_surface_sky_ms | 0.214 | 0.217 | 0.213 to 0.215 |
| earth/window | gpu_surface_bodies_ms | 0.327 | 0.457 | 0.326 to 0.329 |
| earth/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/window | gpu_surface_splats_ms | 0.004 | 0.009 | 0.004 to 0.004 |
| earth/window | gpu_surface_clouds_ms | 0.174 | 0.177 | 0.174 to 0.175 |
| earth/window | gpu_atmospheres_ms | 0.754 | 0.767 | 0.752 to 0.758 |
| earth/window | gpu_belt_dust_ms | 0.206 | 0.220 | 0.206 to 0.206 |
| earth/window | gpu_splat_mask_ms | 0.008 | 0.009 | 0.008 to 0.008 |
| earth/window | gpu_temporal_ms | 0.199 | 0.206 | 0.199 to 0.200 |
| earth/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| earth/window | gpu_bloom_ms | 0.088 | 0.090 | 0.088 to 0.088 |
| earth/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| earth/window | gpu_composite_ms | 0.156 | 0.158 | 0.156 to 0.157 |
| earth/window | gpu_spatial_aa_ms | 0.134 | 0.139 | 0.133 to 0.134 |
| earth/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| earth/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| earth/fullscreen | gpu_cull_ms | 0.171 | 0.173 | 0.170 to 0.171 |
| earth/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| earth/fullscreen | gpu_belt_light_ms | 0.501 | 0.514 | 0.373 to 0.506 |
| earth/fullscreen | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| earth/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.132 | 0.001 to 0.127 |
| earth/fullscreen | gpu_surface_sky_ms | 0.921 | 1.107 | 0.912 to 0.925 |
| earth/fullscreen | gpu_surface_bodies_ms | 0.664 | 0.788 | 0.658 to 0.667 |
| earth/fullscreen | gpu_surface_rocks_ms | 0.025 | 0.026 | 0.025 to 0.025 |
| earth/fullscreen | gpu_surface_splats_ms | 0.004 | 0.007 | 0.003 to 0.004 |
| earth/fullscreen | gpu_surface_clouds_ms | 0.376 | 0.387 | 0.373 to 0.378 |
| earth/fullscreen | gpu_atmospheres_ms | 2.056 | 2.585 | 2.040 to 2.070 |
| earth/fullscreen | gpu_belt_dust_ms | 0.584 | 0.648 | 0.580 to 0.587 |
| earth/fullscreen | gpu_splat_mask_ms | 0.008 | 0.009 | 0.008 to 0.008 |
| earth/fullscreen | gpu_temporal_ms | 0.653 | 0.672 | 0.649 to 0.656 |
| earth/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| earth/fullscreen | gpu_bloom_ms | 0.244 | 0.250 | 0.242 to 0.245 |
| earth/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| earth/fullscreen | gpu_composite_ms | 0.522 | 0.537 | 0.518 to 0.525 |
| earth/fullscreen | gpu_spatial_aa_ms | 0.394 | 0.411 | 0.392 to 0.396 |
| earth/fullscreen | gpu_meter_ms | 0.015 | 0.018 | 0.015 to 0.015 |
| earth/fullscreen | gpu_present_ms | 0.076 | 0.077 | 0.075 to 0.076 |
| giant/window | gpu_cull_ms | 0.211 | 0.213 | 0.211 to 0.211 |
| giant/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| giant/window | gpu_belt_light_ms | 0.367 | 0.503 | 0.365 to 0.482 |
| giant/window | gpu_belt_disc_ms | 0.000 | 0.115 | 0.000 to 0.000 |
| giant/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| giant/window | gpu_surface_sky_ms | 0.248 | 0.250 | 0.246 to 0.248 |
| giant/window | gpu_surface_bodies_ms | 0.266 | 0.278 | 0.265 to 0.266 |
| giant/window | gpu_surface_rocks_ms | 0.038 | 0.041 | 0.038 to 0.038 |
| giant/window | gpu_surface_splats_ms | 0.104 | 0.109 | 0.104 to 0.104 |
| giant/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/window | gpu_atmospheres_ms | 0.527 | 0.534 | 0.524 to 0.528 |
| giant/window | gpu_belt_dust_ms | 1.224 | 1.236 | 1.215 to 1.224 |
| giant/window | gpu_splat_mask_ms | 0.120 | 0.122 | 0.119 to 0.120 |
| giant/window | gpu_temporal_ms | 0.204 | 0.210 | 0.204 to 0.205 |
| giant/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/window | gpu_bloom_ms | 0.084 | 0.086 | 0.084 to 0.084 |
| giant/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| giant/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| giant/window | gpu_composite_ms | 0.157 | 0.158 | 0.156 to 0.157 |
| giant/window | gpu_spatial_aa_ms | 0.126 | 0.128 | 0.125 to 0.126 |
| giant/window | gpu_meter_ms | 0.007 | 0.010 | 0.007 to 0.007 |
| giant/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| giant/fullscreen | gpu_cull_ms | 0.223 | 0.226 | 0.223 to 0.224 |
| giant/fullscreen | gpu_body_shadow_ms | 0.020 | 0.021 | 0.019 to 0.020 |
| giant/fullscreen | gpu_belt_light_ms | 0.488 | 0.508 | 0.483 to 0.493 |
| giant/fullscreen | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| giant/fullscreen | gpu_surface_prepass_ms | 0.002 | 0.119 | 0.002 to 0.002 |
| giant/fullscreen | gpu_surface_sky_ms | 0.973 | 1.378 | 0.964 to 0.975 |
| giant/fullscreen | gpu_surface_bodies_ms | 0.606 | 1.572 | 0.601 to 0.607 |
| giant/fullscreen | gpu_surface_rocks_ms | 0.114 | 0.131 | 0.114 to 0.115 |
| giant/fullscreen | gpu_surface_splats_ms | 0.186 | 0.213 | 0.186 to 0.186 |
| giant/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/fullscreen | gpu_atmospheres_ms | 1.459 | 3.300 | 1.444 to 1.460 |
| giant/fullscreen | gpu_belt_dust_ms | 4.220 | 5.301 | 4.174 to 4.228 |
| giant/fullscreen | gpu_splat_mask_ms | 0.193 | 0.217 | 0.191 to 0.194 |
| giant/fullscreen | gpu_temporal_ms | 0.678 | 0.820 | 0.673 to 0.679 |
| giant/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/fullscreen | gpu_bloom_ms | 0.255 | 0.259 | 0.253 to 0.256 |
| giant/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| giant/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| giant/fullscreen | gpu_composite_ms | 0.526 | 0.786 | 0.520 to 0.526 |
| giant/fullscreen | gpu_spatial_aa_ms | 0.380 | 0.411 | 0.377 to 0.380 |
| giant/fullscreen | gpu_meter_ms | 0.016 | 0.019 | 0.016 to 0.016 |
| giant/fullscreen | gpu_present_ms | 0.077 | 0.078 | 0.077 to 0.077 |
| moon/window | gpu_cull_ms | 0.114 | 0.115 | 0.113 to 0.114 |
| moon/window | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.019 |
| moon/window | gpu_belt_light_ms | 0.489 | 0.495 | 0.368 to 0.493 |
| moon/window | gpu_belt_disc_ms | 0.000 | 0.115 | 0.000 to 0.000 |
| moon/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/window | gpu_surface_sky_ms | 0.224 | 0.225 | 0.222 to 0.224 |
| moon/window | gpu_surface_bodies_ms | 0.136 | 0.137 | 0.135 to 0.136 |
| moon/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/window | gpu_atmospheres_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/window | gpu_belt_dust_ms | 0.054 | 0.054 | 0.053 to 0.054 |
| moon/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| moon/window | gpu_temporal_ms | 0.194 | 0.195 | 0.193 to 0.194 |
| moon/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/window | gpu_bloom_ms | 0.079 | 0.081 | 0.079 to 0.079 |
| moon/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| moon/window | gpu_composite_ms | 0.156 | 0.157 | 0.155 to 0.156 |
| moon/window | gpu_spatial_aa_ms | 0.135 | 0.137 | 0.134 to 0.135 |
| moon/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| moon/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| moon/fullscreen | gpu_cull_ms | 0.120 | 0.123 | 0.119 to 0.120 |
| moon/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| moon/fullscreen | gpu_belt_light_ms | 0.504 | 0.509 | 0.494 to 0.506 |
| moon/fullscreen | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| moon/fullscreen | gpu_surface_prepass_ms | 0.117 | 0.121 | 0.002 to 0.118 |
| moon/fullscreen | gpu_surface_sky_ms | 0.920 | 0.923 | 0.909 to 0.921 |
| moon/fullscreen | gpu_surface_bodies_ms | 0.272 | 0.274 | 0.270 to 0.273 |
| moon/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_surface_splats_ms | 0.000 | 0.001 | 0.000 to 0.001 |
| moon/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/fullscreen | gpu_atmospheres_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/fullscreen | gpu_belt_dust_ms | 0.301 | 0.303 | 0.298 to 0.301 |
| moon/fullscreen | gpu_splat_mask_ms | 0.005 | 0.006 | 0.005 to 0.005 |
| moon/fullscreen | gpu_temporal_ms | 0.641 | 0.646 | 0.637 to 0.642 |
| moon/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/fullscreen | gpu_bloom_ms | 0.246 | 0.248 | 0.244 to 0.247 |
| moon/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| moon/fullscreen | gpu_composite_ms | 0.523 | 0.525 | 0.518 to 0.523 |
| moon/fullscreen | gpu_spatial_aa_ms | 0.410 | 0.414 | 0.407 to 0.410 |
| moon/fullscreen | gpu_meter_ms | 0.014 | 0.022 | 0.014 to 0.014 |
| moon/fullscreen | gpu_present_ms | 0.075 | 0.076 | 0.075 to 0.075 |
| mars/window | gpu_cull_ms | 0.114 | 0.114 | 0.113 to 0.114 |
| mars/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| mars/window | gpu_belt_light_ms | 0.493 | 0.495 | 0.488 to 0.493 |
| mars/window | gpu_belt_disc_ms | 0.000 | 0.115 | 0.000 to 0.000 |
| mars/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| mars/window | gpu_surface_sky_ms | 0.252 | 0.254 | 0.250 to 0.252 |
| mars/window | gpu_surface_bodies_ms | 0.120 | 0.121 | 0.119 to 0.120 |
| mars/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_surface_splats_ms | 0.000 | 0.001 | 0.000 to 0.001 |
| mars/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/window | gpu_atmospheres_ms | 0.358 | 0.362 | 0.355 to 0.358 |
| mars/window | gpu_belt_dust_ms | 0.054 | 0.055 | 0.053 to 0.054 |
| mars/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| mars/window | gpu_temporal_ms | 0.189 | 0.190 | 0.188 to 0.189 |
| mars/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/window | gpu_bloom_ms | 0.078 | 0.079 | 0.077 to 0.078 |
| mars/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| mars/window | gpu_composite_ms | 0.156 | 0.157 | 0.155 to 0.156 |
| mars/window | gpu_spatial_aa_ms | 0.114 | 0.116 | 0.114 to 0.115 |
| mars/window | gpu_meter_ms | 0.007 | 0.013 | 0.007 to 0.007 |
| mars/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| mars/fullscreen | gpu_cull_ms | 0.153 | 0.155 | 0.152 to 0.153 |
| mars/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| mars/fullscreen | gpu_belt_light_ms | 0.497 | 0.509 | 0.368 to 0.505 |
| mars/fullscreen | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| mars/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.120 | 0.001 to 0.116 |
| mars/fullscreen | gpu_surface_sky_ms | 1.011 | 1.175 | 0.999 to 1.012 |
| mars/fullscreen | gpu_surface_bodies_ms | 0.245 | 0.247 | 0.243 to 0.245 |
| mars/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/fullscreen | gpu_surface_splats_ms | 0.002 | 0.003 | 0.002 to 0.002 |
| mars/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/fullscreen | gpu_atmospheres_ms | 0.933 | 0.975 | 0.925 to 0.935 |
| mars/fullscreen | gpu_belt_dust_ms | 0.337 | 0.346 | 0.334 to 0.338 |
| mars/fullscreen | gpu_splat_mask_ms | 0.006 | 0.007 | 0.006 to 0.006 |
| mars/fullscreen | gpu_temporal_ms | 0.630 | 0.637 | 0.626 to 0.631 |
| mars/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/fullscreen | gpu_bloom_ms | 0.245 | 0.247 | 0.242 to 0.245 |
| mars/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| mars/fullscreen | gpu_composite_ms | 0.523 | 0.528 | 0.518 to 0.523 |
| mars/fullscreen | gpu_spatial_aa_ms | 0.345 | 0.349 | 0.343 to 0.345 |
| mars/fullscreen | gpu_meter_ms | 0.014 | 0.017 | 0.014 to 0.014 |
| mars/fullscreen | gpu_present_ms | 0.075 | 0.076 | 0.075 to 0.075 |
| dawn/window | gpu_cull_ms | 0.113 | 0.114 | 0.113 to 0.113 |
| dawn/window | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| dawn/window | gpu_belt_light_ms | 0.373 | 0.498 | 0.369 to 0.493 |
| dawn/window | gpu_belt_disc_ms | 0.000 | 0.115 | 0.000 to 0.000 |
| dawn/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| dawn/window | gpu_surface_sky_ms | 0.132 | 0.135 | 0.131 to 0.132 |
| dawn/window | gpu_surface_bodies_ms | 0.352 | 0.355 | 0.349 to 0.352 |
| dawn/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/window | gpu_surface_splats_ms | 0.000 | 0.001 | 0.000 to 0.001 |
| dawn/window | gpu_surface_clouds_ms | 0.196 | 0.198 | 0.194 to 0.196 |
| dawn/window | gpu_atmospheres_ms | 0.837 | 0.843 | 0.829 to 0.837 |
| dawn/window | gpu_belt_dust_ms | 0.052 | 0.053 | 0.052 to 0.052 |
| dawn/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| dawn/window | gpu_temporal_ms | 0.203 | 0.204 | 0.202 to 0.203 |
| dawn/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| dawn/window | gpu_bloom_ms | 0.097 | 0.099 | 0.097 to 0.097 |
| dawn/window | gpu_sun_visibility_ms | 0.028 | 0.028 | 0.028 to 0.028 |
| dawn/window | gpu_flare_ms | 0.009 | 0.010 | 0.009 to 0.009 |
| dawn/window | gpu_composite_ms | 0.162 | 0.163 | 0.161 to 0.162 |
| dawn/window | gpu_spatial_aa_ms | 0.121 | 0.122 | 0.120 to 0.121 |
| dawn/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| dawn/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| dawn/fullscreen | gpu_cull_ms | 0.114 | 0.116 | 0.114 to 0.114 |
| dawn/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| dawn/fullscreen | gpu_belt_light_ms | 0.504 | 0.518 | 0.498 to 0.506 |
| dawn/fullscreen | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| dawn/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_sky_ms | 0.632 | 0.647 | 0.630 to 0.635 |
| dawn/fullscreen | gpu_surface_bodies_ms | 0.750 | 0.904 | 0.732 to 0.759 |
| dawn/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_clouds_ms | 0.459 | 0.473 | 0.450 to 0.461 |
| dawn/fullscreen | gpu_atmospheres_ms | 2.269 | 2.529 | 2.259 to 2.280 |
| dawn/fullscreen | gpu_belt_dust_ms | 0.176 | 0.180 | 0.175 to 0.177 |
| dawn/fullscreen | gpu_splat_mask_ms | 0.005 | 0.006 | 0.005 to 0.005 |
| dawn/fullscreen | gpu_temporal_ms | 0.667 | 0.680 | 0.665 to 0.670 |
| dawn/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| dawn/fullscreen | gpu_bloom_ms | 0.227 | 0.231 | 0.226 to 0.228 |
| dawn/fullscreen | gpu_sun_visibility_ms | 0.031 | 0.032 | 0.030 to 0.031 |
| dawn/fullscreen | gpu_flare_ms | 0.024 | 0.024 | 0.024 to 0.024 |
| dawn/fullscreen | gpu_composite_ms | 0.544 | 0.556 | 0.541 to 0.547 |
| dawn/fullscreen | gpu_spatial_aa_ms | 0.368 | 0.376 | 0.366 to 0.369 |
| dawn/fullscreen | gpu_meter_ms | 0.015 | 0.018 | 0.015 to 0.015 |
| dawn/fullscreen | gpu_present_ms | 0.075 | 0.076 | 0.075 to 0.075 |
| belt/window | gpu_cull_ms | 0.196 | 0.198 | 0.195 to 0.196 |
| belt/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| belt/window | gpu_belt_light_ms | 0.367 | 0.498 | 0.364 to 0.492 |
| belt/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt/window | gpu_surface_sky_ms | 0.108 | 0.110 | 0.106 to 0.108 |
| belt/window | gpu_surface_bodies_ms | 0.402 | 0.408 | 0.399 to 0.402 |
| belt/window | gpu_surface_rocks_ms | 0.353 | 0.359 | 0.351 to 0.354 |
| belt/window | gpu_surface_splats_ms | 0.049 | 0.051 | 0.048 to 0.049 |
| belt/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_atmospheres_ms | 0.814 | 0.819 | 0.808 to 0.814 |
| belt/window | gpu_belt_dust_ms | 0.658 | 0.779 | 0.652 to 0.659 |
| belt/window | gpu_splat_mask_ms | 0.060 | 0.062 | 0.060 to 0.060 |
| belt/window | gpu_temporal_ms | 0.212 | 0.214 | 0.211 to 0.212 |
| belt/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_bloom_ms | 0.087 | 0.089 | 0.086 to 0.087 |
| belt/window | gpu_sun_visibility_ms | 0.028 | 0.029 | 0.028 to 0.028 |
| belt/window | gpu_flare_ms | 0.009 | 0.010 | 0.009 to 0.009 |
| belt/window | gpu_composite_ms | 0.162 | 0.163 | 0.161 to 0.162 |
| belt/window | gpu_spatial_aa_ms | 0.142 | 0.144 | 0.141 to 0.142 |
| belt/window | gpu_meter_ms | 0.007 | 0.011 | 0.007 to 0.008 |
| belt/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| belt/fullscreen | gpu_cull_ms | 0.209 | 0.212 | 0.208 to 0.210 |
| belt/fullscreen | gpu_body_shadow_ms | 0.020 | 0.021 | 0.019 to 0.020 |
| belt/fullscreen | gpu_belt_light_ms | 0.485 | 0.508 | 0.371 to 0.488 |
| belt/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.109 | 0.001 to 0.002 |
| belt/fullscreen | gpu_surface_sky_ms | 0.508 | 0.843 | 0.505 to 0.508 |
| belt/fullscreen | gpu_surface_bodies_ms | 0.950 | 2.238 | 0.943 to 0.950 |
| belt/fullscreen | gpu_surface_rocks_ms | 1.320 | 1.596 | 1.313 to 1.326 |
| belt/fullscreen | gpu_surface_splats_ms | 0.068 | 0.076 | 0.068 to 0.068 |
| belt/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_atmospheres_ms | 2.512 | 5.535 | 2.499 to 2.522 |
| belt/fullscreen | gpu_belt_dust_ms | 2.176 | 2.613 | 2.164 to 2.182 |
| belt/fullscreen | gpu_splat_mask_ms | 0.086 | 0.088 | 0.086 to 0.086 |
| belt/fullscreen | gpu_temporal_ms | 0.707 | 0.877 | 0.701 to 0.708 |
| belt/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_bloom_ms | 0.278 | 0.289 | 0.276 to 0.278 |
| belt/fullscreen | gpu_sun_visibility_ms | 0.031 | 0.031 | 0.030 to 0.031 |
| belt/fullscreen | gpu_flare_ms | 0.024 | 0.024 | 0.023 to 0.024 |
| belt/fullscreen | gpu_composite_ms | 0.545 | 0.833 | 0.543 to 0.546 |
| belt/fullscreen | gpu_spatial_aa_ms | 0.449 | 0.498 | 0.445 to 0.450 |
| belt/fullscreen | gpu_meter_ms | 0.017 | 0.020 | 0.017 to 0.017 |
| belt/fullscreen | gpu_present_ms | 0.077 | 0.078 | 0.077 to 0.077 |
| belt-sun/window | gpu_cull_ms | 0.198 | 0.200 | 0.197 to 0.198 |
| belt-sun/window | gpu_body_shadow_ms | 0.019 | 0.021 | 0.019 to 0.019 |
| belt-sun/window | gpu_belt_light_ms | 0.364 | 0.502 | 0.361 to 0.486 |
| belt-sun/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt-sun/window | gpu_surface_sky_ms | 0.229 | 0.236 | 0.228 to 0.230 |
| belt-sun/window | gpu_surface_bodies_ms | 0.001 | 0.006 | 0.001 to 0.002 |
| belt-sun/window | gpu_surface_rocks_ms | 0.145 | 0.148 | 0.143 to 0.145 |
| belt-sun/window | gpu_surface_splats_ms | 0.073 | 0.075 | 0.073 to 0.074 |
| belt-sun/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_atmospheres_ms | 0.044 | 0.045 | 0.044 to 0.044 |
| belt-sun/window | gpu_belt_dust_ms | 0.719 | 0.730 | 0.714 to 0.719 |
| belt-sun/window | gpu_splat_mask_ms | 0.079 | 0.079 | 0.078 to 0.079 |
| belt-sun/window | gpu_temporal_ms | 0.196 | 0.197 | 0.195 to 0.196 |
| belt-sun/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_bloom_ms | 0.079 | 0.081 | 0.078 to 0.079 |
| belt-sun/window | gpu_sun_visibility_ms | 0.028 | 0.029 | 0.028 to 0.028 |
| belt-sun/window | gpu_flare_ms | 0.006 | 0.006 | 0.005 to 0.006 |
| belt-sun/window | gpu_composite_ms | 0.158 | 0.159 | 0.157 to 0.158 |
| belt-sun/window | gpu_spatial_aa_ms | 0.120 | 0.122 | 0.120 to 0.120 |
| belt-sun/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| belt-sun/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| belt-sun/fullscreen | gpu_cull_ms | 0.206 | 0.209 | 0.206 to 0.207 |
| belt-sun/fullscreen | gpu_body_shadow_ms | 0.020 | 0.020 | 0.020 to 0.020 |
| belt-sun/fullscreen | gpu_belt_light_ms | 0.374 | 0.508 | 0.373 to 0.489 |
| belt-sun/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt-sun/fullscreen | gpu_surface_sky_ms | 0.883 | 0.898 | 0.879 to 0.883 |
| belt-sun/fullscreen | gpu_surface_bodies_ms | 0.010 | 0.016 | 0.009 to 0.010 |
| belt-sun/fullscreen | gpu_surface_rocks_ms | 0.487 | 0.621 | 0.487 to 0.488 |
| belt-sun/fullscreen | gpu_surface_splats_ms | 0.114 | 0.117 | 0.114 to 0.115 |
| belt-sun/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_atmospheres_ms | 0.279 | 0.281 | 0.278 to 0.280 |
| belt-sun/fullscreen | gpu_belt_dust_ms | 2.202 | 2.352 | 2.192 to 2.204 |
| belt-sun/fullscreen | gpu_splat_mask_ms | 0.105 | 0.108 | 0.105 to 0.106 |
| belt-sun/fullscreen | gpu_temporal_ms | 0.643 | 0.654 | 0.641 to 0.644 |
| belt-sun/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_bloom_ms | 0.220 | 0.223 | 0.219 to 0.220 |
| belt-sun/fullscreen | gpu_sun_visibility_ms | 0.032 | 0.033 | 0.032 to 0.032 |
| belt-sun/fullscreen | gpu_flare_ms | 0.014 | 0.015 | 0.014 to 0.015 |
| belt-sun/fullscreen | gpu_composite_ms | 0.527 | 0.534 | 0.525 to 0.527 |
| belt-sun/fullscreen | gpu_spatial_aa_ms | 0.392 | 0.399 | 0.391 to 0.392 |
| belt-sun/fullscreen | gpu_meter_ms | 0.016 | 0.019 | 0.016 to 0.016 |
| belt-sun/fullscreen | gpu_present_ms | 0.077 | 0.078 | 0.076 to 0.077 |

## Compared with atmosphere-bounds

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| earth/window | cpu_submit_and_wait_ms | +0.009 | +0.2 | within threshold |
| earth/window | gpu_ms | +0.009 | +0.3 | within threshold |
| earth/window | cpu_prepare_ms | +0.024 | +9.3 | within threshold |
| earth/window | gpu_cull_shadow_ms | +0.004 | +0.7 | within threshold |
| earth/window | gpu_surface_ms | -0.003 | -0.3 | within threshold |
| earth/window | gpu_atmosphere_ms | +0.003 | +0.3 | within threshold |
| earth/window | gpu_post_ms | +0.005 | +0.8 | within threshold |
| earth/window | gpu_cull_ms | +0.002 | +1.4 | within threshold |
| earth/window | gpu_body_shadow_ms | +0.001 | +4.1 | within threshold |
| earth/window | gpu_belt_light_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_sky_ms | +0.002 | +1.0 | within threshold |
| earth/window | gpu_surface_bodies_ms | -0.008 | -2.4 | within threshold |
| earth/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_clouds_ms | +0.001 | +0.6 | within threshold |
| earth/window | gpu_atmospheres_ms | +0.003 | +0.4 | within threshold |
| earth/window | gpu_belt_dust_ms | +0.001 | +0.5 | within threshold |
| earth/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_temporal_ms | +0.001 | +0.5 | within threshold |
| earth/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_bloom_ms | +0.002 | +2.4 | within threshold |
| earth/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_composite_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_spatial_aa_ms | +0.001 | +0.8 | within threshold |
| earth/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | cpu_submit_and_wait_ms | +0.118 | +1.4 | within threshold |
| earth/fullscreen | gpu_ms | +0.084 | +1.1 | within threshold |
| earth/fullscreen | cpu_prepare_ms | +0.009 | +2.6 | within threshold |
| earth/fullscreen | gpu_cull_shadow_ms | +0.009 | +1.3 | within threshold |
| earth/fullscreen | gpu_surface_ms | +0.081 | +3.2 | within threshold |
| earth/fullscreen | gpu_atmosphere_ms | +0.014 | +0.5 | within threshold |
| earth/fullscreen | gpu_post_ms | +0.016 | +0.8 | within threshold |
| earth/fullscreen | gpu_cull_ms | +0.003 | +1.7 | within threshold |
| earth/fullscreen | gpu_body_shadow_ms | +0.001 | +4.2 | within threshold |
| earth/fullscreen | gpu_belt_light_ms | +0.004 | +0.7 | within threshold |
| earth/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_sky_ms | +0.008 | +0.9 | within threshold |
| earth/fullscreen | gpu_surface_bodies_ms | -0.018 | -2.7 | within threshold |
| earth/fullscreen | gpu_surface_rocks_ms | +0.001 | +4.3 | within threshold |
| earth/fullscreen | gpu_surface_splats_ms | +0.001 | +33.3 | within threshold |
| earth/fullscreen | gpu_surface_clouds_ms | +0.002 | +0.5 | within threshold |
| earth/fullscreen | gpu_atmospheres_ms | +0.012 | +0.6 | within threshold |
| earth/fullscreen | gpu_belt_dust_ms | +0.002 | +0.4 | within threshold |
| earth/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_temporal_ms | +0.005 | +0.8 | within threshold |
| earth/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_bloom_ms | +0.002 | +0.6 | within threshold |
| earth/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_composite_ms | +0.003 | +0.6 | within threshold |
| earth/fullscreen | gpu_spatial_aa_ms | +0.004 | +1.0 | within threshold |
| earth/fullscreen | gpu_meter_ms | +0.001 | +7.1 | within threshold |
| earth/fullscreen | gpu_present_ms | +0.000 | +0.0 | within threshold |
| giant/window | cpu_submit_and_wait_ms | -0.011 | -0.2 | within threshold |
| giant/window | gpu_ms | -0.004 | -0.1 | within threshold |
| giant/window | cpu_prepare_ms | -0.092 | -25.5 | within threshold |
| giant/window | gpu_cull_shadow_ms | +0.007 | +1.2 | within threshold |
| giant/window | gpu_surface_ms | -0.043 | -4.6 | within threshold |
| giant/window | gpu_atmosphere_ms | +0.015 | +0.8 | within threshold |
| giant/window | gpu_post_ms | +0.006 | +1.0 | within threshold |
| giant/window | gpu_cull_ms | +0.003 | +1.5 | within threshold |
| giant/window | gpu_body_shadow_ms | +0.000 | +2.6 | within threshold |
| giant/window | gpu_belt_light_ms | +0.002 | +0.6 | within threshold |
| giant/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_surface_sky_ms | +0.003 | +1.3 | within threshold |
| giant/window | gpu_surface_bodies_ms | -0.048 | -15.2 | within threshold |
| giant/window | gpu_surface_rocks_ms | -0.002 | -5.1 | within threshold |
| giant/window | gpu_surface_splats_ms | +0.001 | +1.0 | within threshold |
| giant/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_atmospheres_ms | +0.004 | +0.8 | within threshold |
| giant/window | gpu_belt_dust_ms | +0.011 | +0.9 | within threshold |
| giant/window | gpu_splat_mask_ms | +0.001 | +0.9 | within threshold |
| giant/window | gpu_temporal_ms | +0.002 | +1.0 | within threshold |
| giant/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_bloom_ms | +0.001 | +1.2 | within threshold |
| giant/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_composite_ms | +0.001 | +0.7 | within threshold |
| giant/window | gpu_spatial_aa_ms | +0.002 | +1.7 | within threshold |
| giant/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | cpu_submit_and_wait_ms | +0.120 | +1.0 | within threshold |
| giant/fullscreen | gpu_ms | +0.220 | +2.0 | within threshold |
| giant/fullscreen | cpu_prepare_ms | +0.015 | +5.1 | within threshold |
| giant/fullscreen | gpu_cull_shadow_ms | +0.010 | +1.3 | within threshold |
| giant/fullscreen | gpu_surface_ms | +0.014 | +0.6 | within threshold |
| giant/fullscreen | gpu_atmosphere_ms | +0.093 | +1.6 | within threshold |
| giant/fullscreen | gpu_post_ms | +0.029 | +1.5 | within threshold |
| giant/fullscreen | gpu_cull_ms | +0.003 | +1.4 | within threshold |
| giant/fullscreen | gpu_body_shadow_ms | +0.001 | +3.2 | within threshold |
| giant/fullscreen | gpu_belt_light_ms | +0.006 | +1.3 | within threshold |
| giant/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_surface_sky_ms | +0.014 | +1.5 | within threshold |
| giant/fullscreen | gpu_surface_bodies_ms | -0.018 | -3.0 | within threshold |
| giant/fullscreen | gpu_surface_rocks_ms | -0.001 | -0.9 | within threshold |
| giant/fullscreen | gpu_surface_splats_ms | +0.001 | +0.6 | within threshold |
| giant/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_atmospheres_ms | +0.031 | +2.2 | within threshold |
| giant/fullscreen | gpu_belt_dust_ms | +0.050 | +1.2 | within threshold |
| giant/fullscreen | gpu_splat_mask_ms | +0.001 | +0.5 | within threshold |
| giant/fullscreen | gpu_temporal_ms | +0.008 | +1.2 | within threshold |
| giant/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_bloom_ms | +0.003 | +1.2 | within threshold |
| giant/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_composite_ms | +0.007 | +1.4 | within threshold |
| giant/fullscreen | gpu_spatial_aa_ms | +0.007 | +1.9 | within threshold |
| giant/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_present_ms | +0.000 | +0.0 | within threshold |
| moon/window | cpu_submit_and_wait_ms | +0.022 | +0.9 | within threshold |
| moon/window | gpu_ms | +0.015 | +0.8 | within threshold |
| moon/window | cpu_prepare_ms | -0.001 | -0.5 | within threshold |
| moon/window | gpu_cull_shadow_ms | +0.005 | +0.8 | within threshold |
| moon/window | gpu_surface_ms | +0.002 | +0.3 | within threshold |
| moon/window | gpu_atmosphere_ms | +0.001 | +1.7 | within threshold |
| moon/window | gpu_post_ms | +0.006 | +1.0 | within threshold |
| moon/window | gpu_cull_ms | +0.002 | +1.7 | within threshold |
| moon/window | gpu_body_shadow_ms | +0.001 | +4.1 | within threshold |
| moon/window | gpu_belt_light_ms | +0.123 | +33.5 | within threshold |
| moon/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_sky_ms | +0.004 | +1.9 | within threshold |
| moon/window | gpu_surface_bodies_ms | -0.003 | -2.2 | within threshold |
| moon/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_splats_ms | +0.001 | +100.0 | within threshold |
| moon/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_belt_dust_ms | +0.001 | +1.9 | within threshold |
| moon/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_temporal_ms | +0.001 | +0.5 | within threshold |
| moon/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_bloom_ms | +0.001 | +1.3 | within threshold |
| moon/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_composite_ms | +0.001 | +0.7 | within threshold |
| moon/window | gpu_spatial_aa_ms | +0.001 | +0.8 | within threshold |
| moon/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | cpu_submit_and_wait_ms | +0.030 | +0.5 | within threshold |
| moon/fullscreen | gpu_ms | +0.153 | +3.3 | within threshold |
| moon/fullscreen | cpu_prepare_ms | -0.036 | -11.3 | within threshold |
| moon/fullscreen | gpu_cull_shadow_ms | +0.014 | +2.2 | within threshold |
| moon/fullscreen | gpu_surface_ms | +0.113 | +6.4 | within threshold |
| moon/fullscreen | gpu_atmosphere_ms | +0.003 | +1.0 | within threshold |
| moon/fullscreen | gpu_post_ms | +0.024 | +1.2 | within threshold |
| moon/fullscreen | gpu_cull_ms | +0.002 | +2.1 | within threshold |
| moon/fullscreen | gpu_body_shadow_ms | +0.001 | +4.8 | within threshold |
| moon/fullscreen | gpu_belt_light_ms | +0.011 | +2.3 | within threshold |
| moon/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_surface_prepass_ms | +0.116 | +11300.0 | within threshold |
| moon/fullscreen | gpu_surface_sky_ms | +0.010 | +1.1 | within threshold |
| moon/fullscreen | gpu_surface_bodies_ms | +0.002 | +0.8 | within threshold |
| moon/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| moon/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_belt_dust_ms | +0.004 | +1.4 | within threshold |
| moon/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_temporal_ms | +0.005 | +0.8 | within threshold |
| moon/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_bloom_ms | +0.002 | +0.8 | within threshold |
| moon/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_composite_ms | +0.006 | +1.2 | within threshold |
| moon/fullscreen | gpu_spatial_aa_ms | +0.006 | +1.5 | within threshold |
| moon/fullscreen | gpu_meter_ms | +0.000 | +0.3 | within threshold |
| moon/fullscreen | gpu_present_ms | +0.000 | +0.0 | within threshold |
| mars/window | cpu_submit_and_wait_ms | +0.006 | +0.2 | within threshold |
| mars/window | gpu_ms | +0.020 | +0.9 | within threshold |
| mars/window | cpu_prepare_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_cull_shadow_ms | +0.018 | +2.9 | within threshold |
| mars/window | gpu_surface_ms | +0.001 | +0.2 | within threshold |
| mars/window | gpu_atmosphere_ms | +0.004 | +1.0 | within threshold |
| mars/window | gpu_post_ms | +0.006 | +1.1 | within threshold |
| mars/window | gpu_cull_ms | +0.002 | +1.9 | within threshold |
| mars/window | gpu_body_shadow_ms | +0.001 | +4.2 | within threshold |
| mars/window | gpu_belt_light_ms | +0.127 | +34.7 | within threshold |
| mars/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_sky_ms | +0.003 | +1.2 | within threshold |
| mars/window | gpu_surface_bodies_ms | -0.003 | -2.5 | within threshold |
| mars/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| mars/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_atmospheres_ms | +0.003 | +0.9 | within threshold |
| mars/window | gpu_belt_dust_ms | +0.001 | +1.9 | within threshold |
| mars/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_temporal_ms | +0.002 | +1.1 | within threshold |
| mars/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_bloom_ms | +0.002 | +2.7 | within threshold |
| mars/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_composite_ms | +0.001 | +0.7 | within threshold |
| mars/window | gpu_spatial_aa_ms | +0.001 | +0.9 | within threshold |
| mars/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | cpu_submit_and_wait_ms | +0.051 | +0.8 | within threshold |
| mars/fullscreen | gpu_ms | -0.011 | -0.2 | within threshold |
| mars/fullscreen | cpu_prepare_ms | +0.100 | +37.0 | within threshold |
| mars/fullscreen | gpu_cull_shadow_ms | -0.000 | -0.0 | within threshold |
| mars/fullscreen | gpu_surface_ms | -0.072 | -3.8 | within threshold |
| mars/fullscreen | gpu_atmosphere_ms | +0.011 | +0.9 | within threshold |
| mars/fullscreen | gpu_post_ms | +0.024 | +1.3 | within threshold |
| mars/fullscreen | gpu_cull_ms | +0.002 | +1.4 | within threshold |
| mars/fullscreen | gpu_body_shadow_ms | +0.001 | +4.1 | within threshold |
| mars/fullscreen | gpu_belt_light_ms | -0.004 | -0.8 | within threshold |
| mars/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_surface_prepass_ms | -0.114 | -99.1 | within threshold |
| mars/fullscreen | gpu_surface_sky_ms | +0.013 | +1.3 | within threshold |
| mars/fullscreen | gpu_surface_bodies_ms | +0.001 | +0.4 | within threshold |
| mars/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_atmospheres_ms | +0.008 | +0.9 | within threshold |
| mars/fullscreen | gpu_belt_dust_ms | +0.002 | +0.6 | within threshold |
| mars/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_temporal_ms | +0.006 | +0.9 | within threshold |
| mars/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_bloom_ms | +0.003 | +1.3 | within threshold |
| mars/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_composite_ms | +0.006 | +1.2 | within threshold |
| mars/fullscreen | gpu_spatial_aa_ms | +0.005 | +1.5 | within threshold |
| mars/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_present_ms | +0.000 | +0.0 | within threshold |
| dawn/window | cpu_submit_and_wait_ms | +0.072 | +2.1 | within threshold |
| dawn/window | gpu_ms | +0.142 | +5.1 | within threshold |
| dawn/window | cpu_prepare_ms | +0.002 | +0.8 | within threshold |
| dawn/window | gpu_cull_shadow_ms | +0.128 | +25.7 | within threshold |
| dawn/window | gpu_surface_ms | +0.008 | +1.1 | within threshold |
| dawn/window | gpu_atmosphere_ms | +0.008 | +0.9 | within threshold |
| dawn/window | gpu_post_ms | +0.005 | +0.8 | within threshold |
| dawn/window | gpu_cull_ms | +0.002 | +1.8 | within threshold |
| dawn/window | gpu_body_shadow_ms | +0.001 | +4.0 | within threshold |
| dawn/window | gpu_belt_light_ms | +0.004 | +1.1 | within threshold |
| dawn/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_sky_ms | +0.001 | +0.8 | within threshold |
| dawn/window | gpu_surface_bodies_ms | +0.003 | +0.9 | within threshold |
| dawn/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| dawn/window | gpu_surface_clouds_ms | +0.002 | +1.1 | within threshold |
| dawn/window | gpu_atmospheres_ms | +0.007 | +0.9 | within threshold |
| dawn/window | gpu_belt_dust_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_temporal_ms | +0.002 | +1.0 | within threshold |
| dawn/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_bloom_ms | +0.001 | +1.1 | within threshold |
| dawn/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_composite_ms | +0.001 | +0.6 | within threshold |
| dawn/window | gpu_spatial_aa_ms | +0.002 | +1.7 | within threshold |
| dawn/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_present_ms | +0.000 | +0.9 | within threshold |
| dawn/fullscreen | cpu_submit_and_wait_ms | +0.062 | +0.8 | within threshold |
| dawn/fullscreen | gpu_ms | +0.207 | +3.0 | within threshold |
| dawn/fullscreen | cpu_prepare_ms | -0.089 | -22.0 | within threshold |
| dawn/fullscreen | gpu_cull_shadow_ms | +0.133 | +26.2 | within threshold |
| dawn/fullscreen | gpu_surface_ms | +0.076 | +3.7 | within threshold |
| dawn/fullscreen | gpu_atmosphere_ms | +0.027 | +1.1 | within threshold |
| dawn/fullscreen | gpu_post_ms | +0.018 | +1.0 | within threshold |
| dawn/fullscreen | gpu_cull_ms | +0.002 | +1.9 | within threshold |
| dawn/fullscreen | gpu_body_shadow_ms | +0.001 | +3.8 | within threshold |
| dawn/fullscreen | gpu_belt_light_ms | +0.130 | +34.8 | within threshold |
| dawn/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_sky_ms | +0.005 | +0.7 | within threshold |
| dawn/fullscreen | gpu_surface_bodies_ms | +0.017 | +2.4 | within threshold |
| dawn/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_clouds_ms | +0.008 | +1.8 | within threshold |
| dawn/fullscreen | gpu_atmospheres_ms | +0.025 | +1.1 | within threshold |
| dawn/fullscreen | gpu_belt_dust_ms | +0.002 | +1.2 | within threshold |
| dawn/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_temporal_ms | +0.007 | +1.1 | within threshold |
| dawn/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_bloom_ms | +0.003 | +1.4 | within threshold |
| dawn/fullscreen | gpu_sun_visibility_ms | +0.001 | +3.4 | within threshold |
| dawn/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_composite_ms | +0.004 | +0.8 | within threshold |
| dawn/fullscreen | gpu_spatial_aa_ms | +0.006 | +1.7 | within threshold |
| dawn/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_present_ms | +0.001 | +1.4 | within threshold |
| belt/window | cpu_submit_and_wait_ms | -0.141 | -3.0 | within threshold |
| belt/window | gpu_ms | +0.027 | +0.7 | within threshold |
| belt/window | cpu_prepare_ms | -0.121 | -27.6 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.005 | +0.9 | within threshold |
| belt/window | gpu_surface_ms | +0.001 | +0.1 | within threshold |
| belt/window | gpu_atmosphere_ms | +0.012 | +0.8 | within threshold |
| belt/window | gpu_post_ms | +0.007 | +1.1 | within threshold |
| belt/window | gpu_cull_ms | +0.003 | +1.4 | within threshold |
| belt/window | gpu_body_shadow_ms | +0.000 | +2.2 | within threshold |
| belt/window | gpu_belt_light_ms | +0.002 | +0.6 | within threshold |
| belt/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_sky_ms | +0.002 | +1.9 | within threshold |
| belt/window | gpu_surface_bodies_ms | -0.007 | -1.8 | within threshold |
| belt/window | gpu_surface_rocks_ms | +0.003 | +0.9 | within threshold |
| belt/window | gpu_surface_splats_ms | +0.001 | +2.1 | within threshold |
| belt/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_atmospheres_ms | +0.007 | +0.9 | within threshold |
| belt/window | gpu_belt_dust_ms | +0.005 | +0.8 | within threshold |
| belt/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_temporal_ms | +0.002 | +1.0 | within threshold |
| belt/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_bloom_ms | +0.002 | +2.4 | within threshold |
| belt/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_composite_ms | +0.001 | +0.6 | within threshold |
| belt/window | gpu_spatial_aa_ms | +0.001 | +0.7 | within threshold |
| belt/window | gpu_meter_ms | -0.000 | -3.3 | within threshold |
| belt/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.275 | +2.4 | within threshold |
| belt/fullscreen | gpu_ms | +0.200 | +1.9 | within threshold |
| belt/fullscreen | cpu_prepare_ms | -0.026 | -7.5 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.007 | +1.0 | within threshold |
| belt/fullscreen | gpu_surface_ms | +0.125 | +4.1 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.075 | +1.6 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.031 | +1.5 | within threshold |
| belt/fullscreen | gpu_cull_ms | +0.004 | +1.9 | within threshold |
| belt/fullscreen | gpu_body_shadow_ms | +0.001 | +3.8 | within threshold |
| belt/fullscreen | gpu_belt_light_ms | +0.005 | +1.0 | within threshold |
| belt/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_sky_ms | +0.008 | +1.6 | within threshold |
| belt/fullscreen | gpu_surface_bodies_ms | +0.035 | +3.9 | within threshold |
| belt/fullscreen | gpu_surface_rocks_ms | +0.016 | +1.3 | within threshold |
| belt/fullscreen | gpu_surface_splats_ms | +0.001 | +1.5 | within threshold |
| belt/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_atmospheres_ms | +0.045 | +1.8 | within threshold |
| belt/fullscreen | gpu_belt_dust_ms | +0.016 | +0.8 | within threshold |
| belt/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_temporal_ms | +0.007 | +1.0 | within threshold |
| belt/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_bloom_ms | +0.003 | +1.1 | within threshold |
| belt/fullscreen | gpu_sun_visibility_ms | +0.001 | +3.4 | within threshold |
| belt/fullscreen | gpu_flare_ms | +0.001 | +4.5 | within threshold |
| belt/fullscreen | gpu_composite_ms | +0.006 | +1.1 | within threshold |
| belt/fullscreen | gpu_spatial_aa_ms | +0.009 | +2.0 | within threshold |
| belt/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | cpu_submit_and_wait_ms | +0.077 | +2.4 | within threshold |
| belt-sun/window | gpu_ms | +0.121 | +4.7 | within threshold |
| belt-sun/window | cpu_prepare_ms | -0.000 | -0.1 | within threshold |
| belt-sun/window | gpu_cull_shadow_ms | +0.006 | +1.0 | within threshold |
| belt-sun/window | gpu_surface_ms | +0.005 | +1.0 | within threshold |
| belt-sun/window | gpu_atmosphere_ms | +0.006 | +0.7 | within threshold |
| belt-sun/window | gpu_post_ms | +0.006 | +1.0 | within threshold |
| belt-sun/window | gpu_cull_ms | +0.003 | +1.3 | within threshold |
| belt-sun/window | gpu_body_shadow_ms | +0.001 | +2.9 | within threshold |
| belt-sun/window | gpu_belt_light_ms | +0.002 | +0.6 | within threshold |
| belt-sun/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_sky_ms | +0.002 | +0.9 | within threshold |
| belt-sun/window | gpu_surface_bodies_ms | -0.001 | -50.0 | within threshold |
| belt-sun/window | gpu_surface_rocks_ms | +0.001 | +0.7 | within threshold |
| belt-sun/window | gpu_surface_splats_ms | +0.002 | +2.1 | within threshold |
| belt-sun/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_belt_dust_ms | +0.006 | +0.9 | within threshold |
| belt-sun/window | gpu_splat_mask_ms | +0.001 | +1.3 | within threshold |
| belt-sun/window | gpu_temporal_ms | +0.001 | +0.5 | within threshold |
| belt-sun/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_bloom_ms | +0.002 | +2.7 | within threshold |
| belt-sun/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_flare_ms | +0.001 | +20.0 | within threshold |
| belt-sun/window | gpu_composite_ms | +0.001 | +0.7 | within threshold |
| belt-sun/window | gpu_spatial_aa_ms | +0.001 | +0.9 | within threshold |
| belt-sun/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | cpu_submit_and_wait_ms | +0.105 | +1.4 | within threshold |
| belt-sun/fullscreen | gpu_ms | -0.030 | -0.4 | within threshold |
| belt-sun/fullscreen | cpu_prepare_ms | +0.010 | +2.7 | within threshold |
| belt-sun/fullscreen | gpu_cull_shadow_ms | -0.002 | -0.3 | within threshold |
| belt-sun/fullscreen | gpu_surface_ms | +0.003 | +0.1 | within threshold |
| belt-sun/fullscreen | gpu_atmosphere_ms | +0.023 | +0.9 | within threshold |
| belt-sun/fullscreen | gpu_post_ms | +0.017 | +0.9 | within threshold |
| belt-sun/fullscreen | gpu_cull_ms | +0.002 | +1.0 | within threshold |
| belt-sun/fullscreen | gpu_body_shadow_ms | +0.001 | +2.8 | within threshold |
| belt-sun/fullscreen | gpu_belt_light_ms | -0.002 | -0.5 | within threshold |
| belt-sun/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_sky_ms | +0.008 | +0.9 | within threshold |
| belt-sun/fullscreen | gpu_surface_bodies_ms | -0.004 | -28.6 | within threshold |
| belt-sun/fullscreen | gpu_surface_rocks_ms | +0.003 | +0.6 | within threshold |
| belt-sun/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_atmospheres_ms | +0.002 | +0.7 | within threshold |
| belt-sun/fullscreen | gpu_belt_dust_ms | +0.019 | +0.9 | within threshold |
| belt-sun/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_temporal_ms | +0.005 | +0.7 | within threshold |
| belt-sun/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_bloom_ms | +0.002 | +0.9 | within threshold |
| belt-sun/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_composite_ms | +0.005 | +1.0 | within threshold |
| belt-sun/fullscreen | gpu_spatial_aa_ms | +0.004 | +1.1 | within threshold |
| belt-sun/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_present_ms | +0.000 | +0.6 | within threshold |
