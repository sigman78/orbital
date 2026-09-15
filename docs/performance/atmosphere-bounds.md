# Render performance: atmosphere-bounds

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `1c07a186797d0571efdd9900802e0a11dbb06c82`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| earth/window | 1600×900 | 3.821 | 3.101 | 3.375 | 0.550 | 0.961 | 0.970 | 0.613 |
| earth/fullscreen | 3440×1440 | 8.638 | 7.799 | 8.158 | 0.686 | 2.562 | 2.641 | 1.901 |
| giant/window | 1600×900 | 4.853 | 3.997 | 4.364 | 0.593 | 0.936 | 1.859 | 0.607 |
| giant/fullscreen | 3440×1440 | 11.831 | 10.894 | 14.576 | 0.724 | 2.513 | 5.793 | 1.919 |
| moon/window | 1600×900 | 2.529 | 1.883 | 2.063 | 0.619 | 0.606 | 0.059 | 0.598 |
| moon/fullscreen | 3440×1440 | 5.414 | 4.608 | 4.904 | 0.631 | 1.763 | 0.307 | 1.898 |
| mars/window | 1600×900 | 2.897 | 2.218 | 2.345 | 0.609 | 0.615 | 0.414 | 0.572 |
| mars/fullscreen | 3440×1440 | 6.409 | 5.666 | 5.874 | 0.671 | 1.908 | 1.268 | 1.822 |
| dawn/window | 1600×900 | 3.505 | 2.798 | 3.031 | 0.500 | 0.754 | 0.886 | 0.651 |
| dawn/fullscreen | 3440×1440 | 7.932 | 7.015 | 7.361 | 0.509 | 2.024 | 2.427 | 1.934 |
| belt/window | 1600×900 | 4.752 | 3.812 | 3.954 | 0.579 | 1.032 | 1.525 | 0.673 |
| belt/fullscreen | 3440×1440 | 11.464 | 10.586 | 10.927 | 0.707 | 3.061 | 4.714 | 2.104 |
| belt-sun/window | 1600×900 | 3.269 | 2.566 | 2.703 | 0.577 | 0.526 | 0.839 | 0.618 |
| belt-sun/fullscreen | 3440×1440 | 7.710 | 6.894 | 7.166 | 0.604 | 1.713 | 2.567 | 1.912 |

## Children

The scopes inside the groups: indicative, since a scope reads where its commands were issued rather than an exact cost.

| Scene / mode | Scope | Median | p95 | Run range |
| --- | --- | ---: | ---: | ---: |
| earth/window | gpu_cull_ms | 0.164 | 0.165 | 0.163 to 0.164 |
| earth/window | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.018 |
| earth/window | gpu_belt_light_ms | 0.367 | 0.492 | 0.366 to 0.367 |
| earth/window | gpu_belt_disc_ms | 0.000 | 0.114 | 0.000 to 0.000 |
| earth/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| earth/window | gpu_surface_sky_ms | 0.212 | 0.214 | 0.210 to 0.212 |
| earth/window | gpu_surface_bodies_ms | 0.335 | 0.339 | 0.334 to 0.336 |
| earth/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/window | gpu_surface_splats_ms | 0.004 | 0.006 | 0.004 to 0.004 |
| earth/window | gpu_surface_clouds_ms | 0.173 | 0.175 | 0.173 to 0.173 |
| earth/window | gpu_atmospheres_ms | 0.751 | 0.758 | 0.748 to 0.752 |
| earth/window | gpu_belt_dust_ms | 0.205 | 0.216 | 0.204 to 0.206 |
| earth/window | gpu_splat_mask_ms | 0.008 | 0.009 | 0.008 to 0.008 |
| earth/window | gpu_temporal_ms | 0.198 | 0.200 | 0.198 to 0.198 |
| earth/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| earth/window | gpu_bloom_ms | 0.086 | 0.089 | 0.086 to 0.086 |
| earth/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| earth/window | gpu_composite_ms | 0.156 | 0.156 | 0.155 to 0.156 |
| earth/window | gpu_spatial_aa_ms | 0.133 | 0.138 | 0.133 to 0.133 |
| earth/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| earth/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| earth/fullscreen | gpu_cull_ms | 0.168 | 0.170 | 0.166 to 0.168 |
| earth/fullscreen | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.018 |
| earth/fullscreen | gpu_belt_light_ms | 0.498 | 0.510 | 0.367 to 0.505 |
| earth/fullscreen | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| earth/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.128 | 0.001 to 0.126 |
| earth/fullscreen | gpu_surface_sky_ms | 0.912 | 0.935 | 0.901 to 0.913 |
| earth/fullscreen | gpu_surface_bodies_ms | 0.682 | 0.696 | 0.674 to 0.682 |
| earth/fullscreen | gpu_surface_rocks_ms | 0.024 | 0.025 | 0.024 to 0.025 |
| earth/fullscreen | gpu_surface_splats_ms | 0.003 | 0.005 | 0.003 to 0.004 |
| earth/fullscreen | gpu_surface_clouds_ms | 0.374 | 0.383 | 0.369 to 0.374 |
| earth/fullscreen | gpu_atmospheres_ms | 2.044 | 2.101 | 2.014 to 2.050 |
| earth/fullscreen | gpu_belt_dust_ms | 0.582 | 0.618 | 0.573 to 0.583 |
| earth/fullscreen | gpu_splat_mask_ms | 0.008 | 0.009 | 0.008 to 0.008 |
| earth/fullscreen | gpu_temporal_ms | 0.648 | 0.654 | 0.641 to 0.648 |
| earth/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| earth/fullscreen | gpu_bloom_ms | 0.243 | 0.246 | 0.240 to 0.243 |
| earth/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| earth/fullscreen | gpu_composite_ms | 0.519 | 0.529 | 0.513 to 0.519 |
| earth/fullscreen | gpu_spatial_aa_ms | 0.390 | 0.397 | 0.387 to 0.390 |
| earth/fullscreen | gpu_meter_ms | 0.014 | 0.017 | 0.014 to 0.015 |
| earth/fullscreen | gpu_present_ms | 0.076 | 0.076 | 0.075 to 0.076 |
| giant/window | gpu_cull_ms | 0.208 | 0.210 | 0.207 to 0.209 |
| giant/window | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| giant/window | gpu_belt_light_ms | 0.365 | 0.486 | 0.362 to 0.366 |
| giant/window | gpu_belt_disc_ms | 0.000 | 0.113 | 0.000 to 0.000 |
| giant/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| giant/window | gpu_surface_sky_ms | 0.245 | 0.248 | 0.242 to 0.246 |
| giant/window | gpu_surface_bodies_ms | 0.314 | 0.326 | 0.312 to 0.316 |
| giant/window | gpu_surface_rocks_ms | 0.040 | 0.043 | 0.040 to 0.040 |
| giant/window | gpu_surface_splats_ms | 0.103 | 0.108 | 0.103 to 0.104 |
| giant/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/window | gpu_atmospheres_ms | 0.523 | 0.529 | 0.519 to 0.526 |
| giant/window | gpu_belt_dust_ms | 1.212 | 1.223 | 1.205 to 1.221 |
| giant/window | gpu_splat_mask_ms | 0.119 | 0.119 | 0.118 to 0.119 |
| giant/window | gpu_temporal_ms | 0.202 | 0.204 | 0.201 to 0.204 |
| giant/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/window | gpu_bloom_ms | 0.083 | 0.085 | 0.083 to 0.084 |
| giant/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| giant/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| giant/window | gpu_composite_ms | 0.156 | 0.156 | 0.155 to 0.157 |
| giant/window | gpu_spatial_aa_ms | 0.124 | 0.127 | 0.124 to 0.125 |
| giant/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| giant/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| giant/fullscreen | gpu_cull_ms | 0.220 | 0.222 | 0.219 to 0.221 |
| giant/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| giant/fullscreen | gpu_belt_light_ms | 0.482 | 0.498 | 0.367 to 0.486 |
| giant/fullscreen | gpu_belt_disc_ms | 0.000 | 0.115 | 0.000 to 0.000 |
| giant/fullscreen | gpu_surface_prepass_ms | 0.002 | 0.117 | 0.001 to 0.002 |
| giant/fullscreen | gpu_surface_sky_ms | 0.958 | 1.195 | 0.952 to 0.966 |
| giant/fullscreen | gpu_surface_bodies_ms | 0.625 | 2.347 | 0.621 to 0.629 |
| giant/fullscreen | gpu_surface_rocks_ms | 0.115 | 0.145 | 0.114 to 0.115 |
| giant/fullscreen | gpu_surface_splats_ms | 0.185 | 0.201 | 0.185 to 0.185 |
| giant/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/fullscreen | gpu_atmospheres_ms | 1.428 | 3.318 | 1.426 to 1.446 |
| giant/fullscreen | gpu_belt_dust_ms | 4.170 | 4.663 | 4.131 to 4.197 |
| giant/fullscreen | gpu_splat_mask_ms | 0.191 | 0.204 | 0.190 to 0.191 |
| giant/fullscreen | gpu_temporal_ms | 0.670 | 0.732 | 0.666 to 0.674 |
| giant/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/fullscreen | gpu_bloom_ms | 0.252 | 0.254 | 0.250 to 0.254 |
| giant/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| giant/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| giant/fullscreen | gpu_composite_ms | 0.519 | 0.641 | 0.516 to 0.523 |
| giant/fullscreen | gpu_spatial_aa_ms | 0.373 | 0.401 | 0.372 to 0.375 |
| giant/fullscreen | gpu_meter_ms | 0.016 | 0.018 | 0.016 to 0.016 |
| giant/fullscreen | gpu_present_ms | 0.077 | 0.078 | 0.077 to 0.077 |
| moon/window | gpu_cull_ms | 0.112 | 0.113 | 0.111 to 0.112 |
| moon/window | gpu_body_shadow_ms | 0.018 | 0.018 | 0.018 to 0.018 |
| moon/window | gpu_belt_light_ms | 0.367 | 0.494 | 0.365 to 0.492 |
| moon/window | gpu_belt_disc_ms | 0.000 | 0.114 | 0.000 to 0.000 |
| moon/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/window | gpu_surface_sky_ms | 0.220 | 0.223 | 0.219 to 0.221 |
| moon/window | gpu_surface_bodies_ms | 0.139 | 0.141 | 0.139 to 0.141 |
| moon/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_surface_splats_ms | 0.001 | 0.001 | 0.000 to 0.001 |
| moon/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/window | gpu_atmospheres_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/window | gpu_belt_dust_ms | 0.053 | 0.054 | 0.053 to 0.053 |
| moon/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| moon/window | gpu_temporal_ms | 0.193 | 0.194 | 0.191 to 0.194 |
| moon/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/window | gpu_bloom_ms | 0.078 | 0.079 | 0.077 to 0.078 |
| moon/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| moon/window | gpu_composite_ms | 0.155 | 0.155 | 0.154 to 0.156 |
| moon/window | gpu_spatial_aa_ms | 0.134 | 0.135 | 0.133 to 0.134 |
| moon/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| moon/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| moon/fullscreen | gpu_cull_ms | 0.117 | 0.120 | 0.117 to 0.118 |
| moon/fullscreen | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.018 |
| moon/fullscreen | gpu_belt_light_ms | 0.493 | 0.505 | 0.366 to 0.495 |
| moon/fullscreen | gpu_belt_disc_ms | 0.000 | 0.115 | 0.000 to 0.000 |
| moon/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.117 | 0.001 to 0.002 |
| moon/fullscreen | gpu_surface_sky_ms | 0.909 | 0.916 | 0.899 to 0.914 |
| moon/fullscreen | gpu_surface_bodies_ms | 0.270 | 0.273 | 0.268 to 0.272 |
| moon/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_surface_splats_ms | 0.001 | 0.001 | 0.000 to 0.001 |
| moon/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/fullscreen | gpu_atmospheres_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/fullscreen | gpu_belt_dust_ms | 0.297 | 0.300 | 0.294 to 0.299 |
| moon/fullscreen | gpu_splat_mask_ms | 0.005 | 0.005 | 0.005 to 0.005 |
| moon/fullscreen | gpu_temporal_ms | 0.636 | 0.641 | 0.630 to 0.637 |
| moon/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/fullscreen | gpu_bloom_ms | 0.244 | 0.246 | 0.241 to 0.245 |
| moon/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.005 to 0.006 |
| moon/fullscreen | gpu_composite_ms | 0.517 | 0.521 | 0.511 to 0.520 |
| moon/fullscreen | gpu_spatial_aa_ms | 0.403 | 0.409 | 0.400 to 0.404 |
| moon/fullscreen | gpu_meter_ms | 0.014 | 0.017 | 0.014 to 0.014 |
| moon/fullscreen | gpu_present_ms | 0.075 | 0.075 | 0.074 to 0.075 |
| mars/window | gpu_cull_ms | 0.112 | 0.112 | 0.112 to 0.112 |
| mars/window | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.019 |
| mars/window | gpu_belt_light_ms | 0.366 | 0.490 | 0.364 to 0.367 |
| mars/window | gpu_belt_disc_ms | 0.000 | 0.114 | 0.000 to 0.000 |
| mars/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| mars/window | gpu_surface_sky_ms | 0.249 | 0.250 | 0.247 to 0.250 |
| mars/window | gpu_surface_bodies_ms | 0.123 | 0.125 | 0.123 to 0.124 |
| mars/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_surface_splats_ms | 0.001 | 0.001 | 0.000 to 0.001 |
| mars/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/window | gpu_atmospheres_ms | 0.355 | 0.360 | 0.353 to 0.357 |
| mars/window | gpu_belt_dust_ms | 0.053 | 0.054 | 0.053 to 0.054 |
| mars/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| mars/window | gpu_temporal_ms | 0.187 | 0.189 | 0.186 to 0.188 |
| mars/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/window | gpu_bloom_ms | 0.076 | 0.077 | 0.076 to 0.077 |
| mars/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| mars/window | gpu_composite_ms | 0.155 | 0.155 | 0.154 to 0.156 |
| mars/window | gpu_spatial_aa_ms | 0.113 | 0.115 | 0.113 to 0.114 |
| mars/window | gpu_meter_ms | 0.007 | 0.013 | 0.007 to 0.007 |
| mars/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| mars/fullscreen | gpu_cull_ms | 0.151 | 0.152 | 0.150 to 0.151 |
| mars/fullscreen | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.019 |
| mars/fullscreen | gpu_belt_light_ms | 0.501 | 0.506 | 0.366 to 0.503 |
| mars/fullscreen | gpu_belt_disc_ms | 0.000 | 0.115 | 0.000 to 0.000 |
| mars/fullscreen | gpu_surface_prepass_ms | 0.115 | 0.117 | 0.001 to 0.116 |
| mars/fullscreen | gpu_surface_sky_ms | 0.997 | 1.008 | 0.992 to 1.005 |
| mars/fullscreen | gpu_surface_bodies_ms | 0.244 | 0.247 | 0.242 to 0.245 |
| mars/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/fullscreen | gpu_surface_splats_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| mars/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/fullscreen | gpu_atmospheres_ms | 0.925 | 0.948 | 0.915 to 0.927 |
| mars/fullscreen | gpu_belt_dust_ms | 0.335 | 0.346 | 0.330 to 0.335 |
| mars/fullscreen | gpu_splat_mask_ms | 0.006 | 0.007 | 0.006 to 0.006 |
| mars/fullscreen | gpu_temporal_ms | 0.625 | 0.630 | 0.620 to 0.627 |
| mars/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/fullscreen | gpu_bloom_ms | 0.242 | 0.245 | 0.240 to 0.244 |
| mars/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.005 to 0.006 |
| mars/fullscreen | gpu_composite_ms | 0.517 | 0.521 | 0.514 to 0.520 |
| mars/fullscreen | gpu_spatial_aa_ms | 0.340 | 0.344 | 0.338 to 0.341 |
| mars/fullscreen | gpu_meter_ms | 0.014 | 0.016 | 0.014 to 0.014 |
| mars/fullscreen | gpu_present_ms | 0.075 | 0.076 | 0.074 to 0.075 |
| dawn/window | gpu_cull_ms | 0.111 | 0.112 | 0.111 to 0.111 |
| dawn/window | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.018 |
| dawn/window | gpu_belt_light_ms | 0.369 | 0.495 | 0.368 to 0.371 |
| dawn/window | gpu_belt_disc_ms | 0.000 | 0.114 | 0.000 to 0.000 |
| dawn/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| dawn/window | gpu_surface_sky_ms | 0.131 | 0.133 | 0.130 to 0.131 |
| dawn/window | gpu_surface_bodies_ms | 0.349 | 0.352 | 0.348 to 0.352 |
| dawn/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.000 to 0.001 |
| dawn/window | gpu_surface_splats_ms | 0.001 | 0.001 | 0.000 to 0.001 |
| dawn/window | gpu_surface_clouds_ms | 0.194 | 0.196 | 0.193 to 0.195 |
| dawn/window | gpu_atmospheres_ms | 0.829 | 0.836 | 0.824 to 0.836 |
| dawn/window | gpu_belt_dust_ms | 0.052 | 0.052 | 0.052 to 0.052 |
| dawn/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| dawn/window | gpu_temporal_ms | 0.201 | 0.202 | 0.201 to 0.202 |
| dawn/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| dawn/window | gpu_bloom_ms | 0.096 | 0.098 | 0.096 to 0.096 |
| dawn/window | gpu_sun_visibility_ms | 0.028 | 0.028 | 0.028 to 0.028 |
| dawn/window | gpu_flare_ms | 0.009 | 0.010 | 0.009 to 0.009 |
| dawn/window | gpu_composite_ms | 0.161 | 0.161 | 0.160 to 0.162 |
| dawn/window | gpu_spatial_aa_ms | 0.119 | 0.121 | 0.119 to 0.120 |
| dawn/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| dawn/window | gpu_present_ms | 0.025 | 0.026 | 0.025 to 0.026 |
| dawn/fullscreen | gpu_cull_ms | 0.112 | 0.113 | 0.112 to 0.112 |
| dawn/fullscreen | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.018 |
| dawn/fullscreen | gpu_belt_light_ms | 0.374 | 0.512 | 0.373 to 0.504 |
| dawn/fullscreen | gpu_belt_disc_ms | 0.000 | 0.115 | 0.000 to 0.000 |
| dawn/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_sky_ms | 0.627 | 0.638 | 0.621 to 0.629 |
| dawn/fullscreen | gpu_surface_bodies_ms | 0.732 | 0.890 | 0.729 to 0.869 |
| dawn/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_clouds_ms | 0.451 | 0.462 | 0.448 to 0.453 |
| dawn/fullscreen | gpu_atmospheres_ms | 2.245 | 2.300 | 2.232 to 2.254 |
| dawn/fullscreen | gpu_belt_dust_ms | 0.174 | 0.177 | 0.173 to 0.175 |
| dawn/fullscreen | gpu_splat_mask_ms | 0.005 | 0.005 | 0.005 to 0.005 |
| dawn/fullscreen | gpu_temporal_ms | 0.659 | 0.670 | 0.656 to 0.663 |
| dawn/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| dawn/fullscreen | gpu_bloom_ms | 0.224 | 0.227 | 0.223 to 0.225 |
| dawn/fullscreen | gpu_sun_visibility_ms | 0.030 | 0.031 | 0.030 to 0.030 |
| dawn/fullscreen | gpu_flare_ms | 0.024 | 0.024 | 0.023 to 0.024 |
| dawn/fullscreen | gpu_composite_ms | 0.540 | 0.547 | 0.535 to 0.541 |
| dawn/fullscreen | gpu_spatial_aa_ms | 0.361 | 0.368 | 0.359 to 0.362 |
| dawn/fullscreen | gpu_meter_ms | 0.015 | 0.017 | 0.015 to 0.015 |
| dawn/fullscreen | gpu_present_ms | 0.074 | 0.075 | 0.074 to 0.075 |
| belt/window | gpu_cull_ms | 0.193 | 0.195 | 0.193 to 0.194 |
| belt/window | gpu_body_shadow_ms | 0.019 | 0.019 | 0.018 to 0.019 |
| belt/window | gpu_belt_light_ms | 0.365 | 0.492 | 0.362 to 0.366 |
| belt/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt/window | gpu_surface_sky_ms | 0.105 | 0.109 | 0.104 to 0.106 |
| belt/window | gpu_surface_bodies_ms | 0.410 | 0.415 | 0.408 to 0.412 |
| belt/window | gpu_surface_rocks_ms | 0.350 | 0.355 | 0.349 to 0.353 |
| belt/window | gpu_surface_splats_ms | 0.048 | 0.050 | 0.048 to 0.048 |
| belt/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_atmospheres_ms | 0.807 | 0.814 | 0.803 to 0.813 |
| belt/window | gpu_belt_dust_ms | 0.653 | 0.657 | 0.650 to 0.658 |
| belt/window | gpu_splat_mask_ms | 0.060 | 0.061 | 0.060 to 0.060 |
| belt/window | gpu_temporal_ms | 0.210 | 0.212 | 0.210 to 0.212 |
| belt/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_bloom_ms | 0.085 | 0.087 | 0.085 to 0.086 |
| belt/window | gpu_sun_visibility_ms | 0.028 | 0.028 | 0.028 to 0.028 |
| belt/window | gpu_flare_ms | 0.009 | 0.010 | 0.009 to 0.009 |
| belt/window | gpu_composite_ms | 0.161 | 0.161 | 0.160 to 0.162 |
| belt/window | gpu_spatial_aa_ms | 0.141 | 0.142 | 0.140 to 0.141 |
| belt/window | gpu_meter_ms | 0.008 | 0.012 | 0.007 to 0.009 |
| belt/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| belt/fullscreen | gpu_cull_ms | 0.206 | 0.208 | 0.205 to 0.206 |
| belt/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| belt/fullscreen | gpu_belt_light_ms | 0.481 | 0.522 | 0.480 to 0.483 |
| belt/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.105 | 0.001 to 0.001 |
| belt/fullscreen | gpu_surface_sky_ms | 0.500 | 0.515 | 0.497 to 0.502 |
| belt/fullscreen | gpu_surface_bodies_ms | 0.914 | 0.940 | 0.908 to 0.918 |
| belt/fullscreen | gpu_surface_rocks_ms | 1.304 | 1.348 | 1.297 to 1.309 |
| belt/fullscreen | gpu_surface_splats_ms | 0.067 | 0.070 | 0.067 to 0.068 |
| belt/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_atmospheres_ms | 2.467 | 2.554 | 2.449 to 2.485 |
| belt/fullscreen | gpu_belt_dust_ms | 2.160 | 2.183 | 2.148 to 2.174 |
| belt/fullscreen | gpu_splat_mask_ms | 0.086 | 0.087 | 0.085 to 0.086 |
| belt/fullscreen | gpu_temporal_ms | 0.699 | 0.706 | 0.695 to 0.701 |
| belt/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_bloom_ms | 0.274 | 0.278 | 0.273 to 0.276 |
| belt/fullscreen | gpu_sun_visibility_ms | 0.030 | 0.031 | 0.030 to 0.030 |
| belt/fullscreen | gpu_flare_ms | 0.023 | 0.024 | 0.023 to 0.024 |
| belt/fullscreen | gpu_composite_ms | 0.539 | 0.545 | 0.536 to 0.542 |
| belt/fullscreen | gpu_spatial_aa_ms | 0.440 | 0.450 | 0.439 to 0.442 |
| belt/fullscreen | gpu_meter_ms | 0.017 | 0.020 | 0.017 to 0.017 |
| belt/fullscreen | gpu_present_ms | 0.077 | 0.078 | 0.077 to 0.077 |
| belt-sun/window | gpu_cull_ms | 0.195 | 0.198 | 0.195 to 0.196 |
| belt-sun/window | gpu_body_shadow_ms | 0.019 | 0.023 | 0.019 to 0.019 |
| belt-sun/window | gpu_belt_light_ms | 0.361 | 0.490 | 0.359 to 0.362 |
| belt-sun/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt-sun/window | gpu_surface_sky_ms | 0.227 | 0.230 | 0.226 to 0.229 |
| belt-sun/window | gpu_surface_bodies_ms | 0.002 | 0.006 | 0.001 to 0.002 |
| belt-sun/window | gpu_surface_rocks_ms | 0.144 | 0.147 | 0.143 to 0.145 |
| belt-sun/window | gpu_surface_splats_ms | 0.072 | 0.074 | 0.072 to 0.073 |
| belt-sun/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_atmospheres_ms | 0.044 | 0.044 | 0.044 to 0.044 |
| belt-sun/window | gpu_belt_dust_ms | 0.713 | 0.722 | 0.710 to 0.719 |
| belt-sun/window | gpu_splat_mask_ms | 0.078 | 0.079 | 0.078 to 0.079 |
| belt-sun/window | gpu_temporal_ms | 0.195 | 0.196 | 0.194 to 0.196 |
| belt-sun/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_bloom_ms | 0.077 | 0.079 | 0.077 to 0.077 |
| belt-sun/window | gpu_sun_visibility_ms | 0.028 | 0.029 | 0.028 to 0.028 |
| belt-sun/window | gpu_flare_ms | 0.005 | 0.006 | 0.005 to 0.006 |
| belt-sun/window | gpu_composite_ms | 0.157 | 0.157 | 0.156 to 0.157 |
| belt-sun/window | gpu_spatial_aa_ms | 0.119 | 0.121 | 0.119 to 0.120 |
| belt-sun/window | gpu_meter_ms | 0.007 | 0.012 | 0.007 to 0.007 |
| belt-sun/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| belt-sun/fullscreen | gpu_cull_ms | 0.204 | 0.207 | 0.203 to 0.204 |
| belt-sun/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| belt-sun/fullscreen | gpu_belt_light_ms | 0.376 | 0.516 | 0.369 to 0.496 |
| belt-sun/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt-sun/fullscreen | gpu_surface_sky_ms | 0.874 | 0.886 | 0.868 to 0.879 |
| belt-sun/fullscreen | gpu_surface_bodies_ms | 0.014 | 0.020 | 0.014 to 0.014 |
| belt-sun/fullscreen | gpu_surface_rocks_ms | 0.484 | 0.612 | 0.479 to 0.599 |
| belt-sun/fullscreen | gpu_surface_splats_ms | 0.114 | 0.116 | 0.113 to 0.114 |
| belt-sun/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_atmospheres_ms | 0.276 | 0.282 | 0.274 to 0.278 |
| belt-sun/fullscreen | gpu_belt_dust_ms | 2.182 | 2.214 | 2.167 to 2.192 |
| belt-sun/fullscreen | gpu_splat_mask_ms | 0.105 | 0.108 | 0.105 to 0.106 |
| belt-sun/fullscreen | gpu_temporal_ms | 0.638 | 0.643 | 0.634 to 0.640 |
| belt-sun/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_bloom_ms | 0.218 | 0.222 | 0.216 to 0.219 |
| belt-sun/fullscreen | gpu_sun_visibility_ms | 0.032 | 0.032 | 0.031 to 0.032 |
| belt-sun/fullscreen | gpu_flare_ms | 0.014 | 0.015 | 0.014 to 0.014 |
| belt-sun/fullscreen | gpu_composite_ms | 0.522 | 0.530 | 0.519 to 0.525 |
| belt-sun/fullscreen | gpu_spatial_aa_ms | 0.388 | 0.393 | 0.385 to 0.388 |
| belt-sun/fullscreen | gpu_meter_ms | 0.016 | 0.023 | 0.016 to 0.016 |
| belt-sun/fullscreen | gpu_present_ms | 0.076 | 0.078 | 0.076 to 0.077 |

## Compared with headless-baseline

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| earth/window | cpu_submit_and_wait_ms | -0.057 | -1.5 | within threshold |
| earth/window | gpu_ms | -0.018 | -0.6 | within threshold |
| earth/window | cpu_prepare_ms | +0.001 | +0.4 | within threshold |
| earth/window | gpu_cull_shadow_ms | +0.010 | +1.9 | within threshold |
| earth/window | gpu_surface_ms | +0.008 | +0.8 | within threshold |
| earth/window | gpu_atmosphere_ms | -0.050 | -4.9 | within threshold |
| earth/window | gpu_post_ms | +0.011 | +1.9 | within threshold |
| earth/window | gpu_cull_ms | +0.005 | +3.1 | within threshold |
| earth/window | gpu_body_shadow_ms | +0.000 | +2.2 | within threshold |
| earth/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| earth/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_sky_ms | +0.002 | +1.0 | within threshold |
| earth/window | gpu_surface_bodies_ms | +0.003 | +0.9 | within threshold |
| earth/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_clouds_ms | +0.001 | +0.6 | within threshold |
| earth/window | gpu_atmospheres_ms | -0.050 | -6.3 | within threshold |
| earth/window | gpu_belt_dust_ms | +0.001 | +0.5 | within threshold |
| earth/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_temporal_ms | +0.003 | +1.6 | within threshold |
| earth/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_bloom_ms | +0.003 | +3.7 | within threshold |
| earth/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_composite_ms | +0.003 | +2.0 | within threshold |
| earth/window | gpu_spatial_aa_ms | +0.003 | +2.4 | within threshold |
| earth/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | cpu_submit_and_wait_ms | -0.225 | -2.5 | within threshold |
| earth/fullscreen | gpu_ms | -0.207 | -2.6 | within threshold |
| earth/fullscreen | cpu_prepare_ms | +0.030 | +10.0 | within threshold |
| earth/fullscreen | gpu_cull_shadow_ms | +0.015 | +2.2 | within threshold |
| earth/fullscreen | gpu_surface_ms | +0.032 | +1.3 | within threshold |
| earth/fullscreen | gpu_atmosphere_ms | -0.304 | -10.3 | within threshold |
| earth/fullscreen | gpu_post_ms | +0.036 | +1.9 | within threshold |
| earth/fullscreen | gpu_cull_ms | +0.005 | +3.4 | within threshold |
| earth/fullscreen | gpu_body_shadow_ms | +0.000 | +2.7 | within threshold |
| earth/fullscreen | gpu_belt_light_ms | +0.011 | +2.3 | within threshold |
| earth/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_surface_prepass_ms | -0.001 | -33.3 | within threshold |
| earth/fullscreen | gpu_surface_sky_ms | +0.007 | +0.8 | within threshold |
| earth/fullscreen | gpu_surface_bodies_ms | +0.007 | +1.1 | within threshold |
| earth/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_clouds_ms | +0.004 | +1.1 | within threshold |
| earth/fullscreen | gpu_atmospheres_ms | -0.314 | -13.3 | within threshold |
| earth/fullscreen | gpu_belt_dust_ms | +0.010 | +1.8 | within threshold |
| earth/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_temporal_ms | +0.014 | +2.3 | within threshold |
| earth/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_bloom_ms | +0.003 | +1.3 | within threshold |
| earth/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_flare_ms | +0.001 | +20.0 | within threshold |
| earth/fullscreen | gpu_composite_ms | +0.009 | +1.8 | within threshold |
| earth/fullscreen | gpu_spatial_aa_ms | +0.009 | +2.4 | within threshold |
| earth/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| giant/window | cpu_submit_and_wait_ms | +0.094 | +2.0 | within threshold |
| giant/window | gpu_ms | +0.021 | +0.5 | within threshold |
| giant/window | cpu_prepare_ms | +0.107 | +42.0 | within threshold |
| giant/window | gpu_cull_shadow_ms | +0.010 | +1.6 | within threshold |
| giant/window | gpu_surface_ms | +0.009 | +1.0 | within threshold |
| giant/window | gpu_atmosphere_ms | -0.006 | -0.3 | within threshold |
| giant/window | gpu_post_ms | +0.011 | +1.9 | within threshold |
| giant/window | gpu_cull_ms | +0.004 | +2.2 | within threshold |
| giant/window | gpu_body_shadow_ms | +0.000 | +2.1 | within threshold |
| giant/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| giant/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_surface_sky_ms | +0.002 | +0.8 | within threshold |
| giant/window | gpu_surface_bodies_ms | +0.004 | +1.2 | within threshold |
| giant/window | gpu_surface_rocks_ms | +0.001 | +2.6 | within threshold |
| giant/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_atmospheres_ms | -0.016 | -3.0 | within threshold |
| giant/window | gpu_belt_dust_ms | +0.009 | +0.8 | within threshold |
| giant/window | gpu_splat_mask_ms | +0.001 | +0.9 | within threshold |
| giant/window | gpu_temporal_ms | +0.003 | +1.5 | within threshold |
| giant/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_bloom_ms | +0.002 | +2.5 | within threshold |
| giant/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_composite_ms | +0.003 | +2.0 | within threshold |
| giant/window | gpu_spatial_aa_ms | +0.002 | +1.7 | within threshold |
| giant/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | cpu_submit_and_wait_ms | -0.128 | -1.1 | within threshold |
| giant/fullscreen | gpu_ms | -0.137 | -1.2 | within threshold |
| giant/fullscreen | cpu_prepare_ms | -0.073 | -19.5 | within threshold |
| giant/fullscreen | gpu_cull_shadow_ms | +0.023 | +3.3 | within threshold |
| giant/fullscreen | gpu_surface_ms | +0.084 | +3.5 | within threshold |
| giant/fullscreen | gpu_atmosphere_ms | -0.172 | -2.9 | within threshold |
| giant/fullscreen | gpu_post_ms | +0.031 | +1.6 | within threshold |
| giant/fullscreen | gpu_cull_ms | +0.004 | +2.0 | within threshold |
| giant/fullscreen | gpu_body_shadow_ms | +0.000 | +2.3 | within threshold |
| giant/fullscreen | gpu_belt_light_ms | +0.111 | +29.8 | within threshold |
| giant/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_surface_prepass_ms | +0.001 | +100.0 | within threshold |
| giant/fullscreen | gpu_surface_sky_ms | +0.004 | +0.4 | within threshold |
| giant/fullscreen | gpu_surface_bodies_ms | +0.008 | +1.3 | within threshold |
| giant/fullscreen | gpu_surface_rocks_ms | +0.001 | +0.9 | within threshold |
| giant/fullscreen | gpu_surface_splats_ms | +0.003 | +1.7 | within threshold |
| giant/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_atmospheres_ms | -0.215 | -13.1 | within threshold |
| giant/fullscreen | gpu_belt_dust_ms | +0.040 | +1.0 | within threshold |
| giant/fullscreen | gpu_splat_mask_ms | +0.004 | +2.2 | within threshold |
| giant/fullscreen | gpu_temporal_ms | +0.013 | +2.0 | within threshold |
| giant/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_bloom_ms | +0.003 | +1.2 | within threshold |
| giant/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_flare_ms | +0.001 | +9.1 | within threshold |
| giant/fullscreen | gpu_composite_ms | +0.008 | +1.6 | within threshold |
| giant/fullscreen | gpu_spatial_aa_ms | +0.007 | +2.0 | within threshold |
| giant/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_present_ms | +0.002 | +2.7 | within threshold |
| moon/window | cpu_submit_and_wait_ms | +0.032 | +1.3 | within threshold |
| moon/window | gpu_ms | +0.024 | +1.3 | within threshold |
| moon/window | cpu_prepare_ms | +0.001 | +0.2 | within threshold |
| moon/window | gpu_cull_shadow_ms | +0.011 | +1.8 | within threshold |
| moon/window | gpu_surface_ms | +0.004 | +0.7 | within threshold |
| moon/window | gpu_atmosphere_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_post_ms | +0.009 | +1.6 | within threshold |
| moon/window | gpu_cull_ms | +0.005 | +4.8 | within threshold |
| moon/window | gpu_body_shadow_ms | +0.000 | +1.7 | within threshold |
| moon/window | gpu_belt_light_ms | -0.058 | -13.6 | within threshold |
| moon/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_sky_ms | +0.001 | +0.5 | within threshold |
| moon/window | gpu_surface_bodies_ms | +0.001 | +0.7 | within threshold |
| moon/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_splats_ms | -0.001 | -50.0 | within threshold |
| moon/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_belt_dust_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_temporal_ms | +0.003 | +1.6 | within threshold |
| moon/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_bloom_ms | +0.002 | +2.7 | within threshold |
| moon/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_composite_ms | +0.002 | +1.3 | within threshold |
| moon/window | gpu_spatial_aa_ms | +0.003 | +2.3 | within threshold |
| moon/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| moon/fullscreen | cpu_submit_and_wait_ms | +0.061 | +1.1 | within threshold |
| moon/fullscreen | gpu_ms | +0.037 | +0.8 | within threshold |
| moon/fullscreen | cpu_prepare_ms | -0.038 | -10.8 | within threshold |
| moon/fullscreen | gpu_cull_shadow_ms | +0.011 | +1.7 | within threshold |
| moon/fullscreen | gpu_surface_ms | +0.004 | +0.2 | within threshold |
| moon/fullscreen | gpu_atmosphere_ms | +0.002 | +0.7 | within threshold |
| moon/fullscreen | gpu_post_ms | +0.033 | +1.8 | within threshold |
| moon/fullscreen | gpu_cull_ms | +0.006 | +5.0 | within threshold |
| moon/fullscreen | gpu_body_shadow_ms | +0.000 | +2.4 | within threshold |
| moon/fullscreen | gpu_belt_light_ms | +0.004 | +0.8 | within threshold |
| moon/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_sky_ms | +0.005 | +0.6 | within threshold |
| moon/fullscreen | gpu_surface_bodies_ms | +0.003 | +1.1 | within threshold |
| moon/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_belt_dust_ms | +0.001 | +0.3 | within threshold |
| moon/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_temporal_ms | +0.012 | +2.0 | within threshold |
| moon/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_bloom_ms | +0.003 | +1.3 | within threshold |
| moon/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_composite_ms | +0.008 | +1.6 | within threshold |
| moon/fullscreen | gpu_spatial_aa_ms | +0.009 | +2.3 | within threshold |
| moon/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| mars/window | cpu_submit_and_wait_ms | +0.069 | +2.4 | within threshold |
| mars/window | gpu_ms | +0.102 | +4.8 | within threshold |
| mars/window | cpu_prepare_ms | +0.002 | +0.8 | within threshold |
| mars/window | gpu_cull_shadow_ms | +0.122 | +25.0 | within threshold |
| mars/window | gpu_surface_ms | +0.005 | +0.8 | within threshold |
| mars/window | gpu_atmosphere_ms | -0.017 | -4.0 | within threshold |
| mars/window | gpu_post_ms | +0.009 | +1.6 | within threshold |
| mars/window | gpu_cull_ms | +0.005 | +4.7 | within threshold |
| mars/window | gpu_body_shadow_ms | +0.000 | +1.2 | within threshold |
| mars/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| mars/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_sky_ms | +0.002 | +0.8 | within threshold |
| mars/window | gpu_surface_bodies_ms | +0.002 | +1.7 | within threshold |
| mars/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_atmospheres_ms | -0.017 | -4.7 | within threshold |
| mars/window | gpu_belt_dust_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_temporal_ms | +0.003 | +1.7 | within threshold |
| mars/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_bloom_ms | +0.001 | +1.4 | within threshold |
| mars/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_composite_ms | +0.003 | +2.0 | within threshold |
| mars/window | gpu_spatial_aa_ms | +0.002 | +1.9 | within threshold |
| mars/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| mars/fullscreen | cpu_submit_and_wait_ms | -0.164 | -2.5 | within threshold |
| mars/fullscreen | gpu_ms | -0.016 | -0.3 | within threshold |
| mars/fullscreen | cpu_prepare_ms | -0.106 | -28.1 | within threshold |
| mars/fullscreen | gpu_cull_shadow_ms | +0.024 | +3.7 | within threshold |
| mars/fullscreen | gpu_surface_ms | +0.114 | +6.3 | within threshold |
| mars/fullscreen | gpu_atmosphere_ms | -0.205 | -13.9 | within threshold |
| mars/fullscreen | gpu_post_ms | +0.034 | +1.9 | within threshold |
| mars/fullscreen | gpu_cull_ms | +0.006 | +4.0 | within threshold |
| mars/fullscreen | gpu_body_shadow_ms | +0.000 | +1.5 | within threshold |
| mars/fullscreen | gpu_belt_light_ms | +0.134 | +36.6 | within threshold |
| mars/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_surface_prepass_ms | +0.114 | +11100.0 | within threshold |
| mars/fullscreen | gpu_surface_sky_ms | +0.005 | +0.5 | within threshold |
| mars/fullscreen | gpu_surface_bodies_ms | +0.004 | +1.7 | within threshold |
| mars/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_atmospheres_ms | -0.208 | -18.4 | within threshold |
| mars/fullscreen | gpu_belt_dust_ms | +0.004 | +1.2 | within threshold |
| mars/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_temporal_ms | +0.013 | +2.2 | within threshold |
| mars/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_bloom_ms | +0.002 | +0.9 | within threshold |
| mars/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_flare_ms | +0.001 | +9.1 | within threshold |
| mars/fullscreen | gpu_composite_ms | +0.009 | +1.8 | within threshold |
| mars/fullscreen | gpu_spatial_aa_ms | +0.008 | +2.5 | within threshold |
| mars/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| dawn/window | cpu_submit_and_wait_ms | -0.018 | -0.5 | within threshold |
| dawn/window | gpu_ms | -0.013 | -0.4 | within threshold |
| dawn/window | cpu_prepare_ms | -0.002 | -0.6 | within threshold |
| dawn/window | gpu_cull_shadow_ms | +0.009 | +1.9 | within threshold |
| dawn/window | gpu_surface_ms | +0.002 | +0.3 | within threshold |
| dawn/window | gpu_atmosphere_ms | -0.037 | -4.0 | within threshold |
| dawn/window | gpu_post_ms | +0.013 | +2.1 | within threshold |
| dawn/window | gpu_cull_ms | +0.005 | +4.7 | within threshold |
| dawn/window | gpu_body_shadow_ms | +0.000 | +1.8 | within threshold |
| dawn/window | gpu_belt_light_ms | +0.004 | +1.1 | within threshold |
| dawn/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_sky_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_bodies_ms | +0.002 | +0.6 | within threshold |
| dawn/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_clouds_ms | +0.001 | +0.5 | within threshold |
| dawn/window | gpu_atmospheres_ms | -0.036 | -4.1 | within threshold |
| dawn/window | gpu_belt_dust_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_temporal_ms | +0.003 | +1.6 | within threshold |
| dawn/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_bloom_ms | +0.005 | +5.6 | within threshold |
| dawn/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_composite_ms | +0.002 | +1.3 | within threshold |
| dawn/window | gpu_spatial_aa_ms | +0.002 | +1.8 | within threshold |
| dawn/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_present_ms | +0.001 | +3.2 | within threshold |
| dawn/fullscreen | cpu_submit_and_wait_ms | -0.294 | -3.6 | within threshold |
| dawn/fullscreen | gpu_ms | -0.298 | -4.1 | within threshold |
| dawn/fullscreen | cpu_prepare_ms | +0.030 | +7.9 | within threshold |
| dawn/fullscreen | gpu_cull_shadow_ms | -0.107 | -17.4 | within threshold |
| dawn/fullscreen | gpu_surface_ms | +0.020 | +1.0 | within threshold |
| dawn/fullscreen | gpu_atmosphere_ms | -0.364 | -13.0 | within threshold |
| dawn/fullscreen | gpu_post_ms | +0.040 | +2.1 | within threshold |
| dawn/fullscreen | gpu_cull_ms | +0.005 | +5.0 | within threshold |
| dawn/fullscreen | gpu_body_shadow_ms | +0.000 | +2.3 | within threshold |
| dawn/fullscreen | gpu_belt_light_ms | +0.001 | +0.3 | within threshold |
| dawn/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_sky_ms | +0.005 | +0.8 | within threshold |
| dawn/fullscreen | gpu_surface_bodies_ms | +0.008 | +1.1 | within threshold |
| dawn/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_clouds_ms | +0.005 | +1.0 | within threshold |
| dawn/fullscreen | gpu_atmospheres_ms | -0.364 | -14.0 | within threshold |
| dawn/fullscreen | gpu_belt_dust_ms | +0.001 | +0.6 | within threshold |
| dawn/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_temporal_ms | +0.012 | +1.9 | within threshold |
| dawn/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_bloom_ms | +0.006 | +2.8 | within threshold |
| dawn/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_flare_ms | +0.001 | +4.5 | within threshold |
| dawn/fullscreen | gpu_composite_ms | +0.010 | +1.9 | within threshold |
| dawn/fullscreen | gpu_spatial_aa_ms | +0.008 | +2.3 | within threshold |
| dawn/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_present_ms | +0.001 | +1.4 | within threshold |
| belt/window | cpu_submit_and_wait_ms | +0.173 | +3.8 | within threshold |
| belt/window | gpu_ms | +0.023 | +0.6 | within threshold |
| belt/window | cpu_prepare_ms | +0.177 | +68.1 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.009 | +1.6 | within threshold |
| belt/window | gpu_surface_ms | +0.003 | +0.3 | within threshold |
| belt/window | gpu_atmosphere_ms | +0.007 | +0.4 | within threshold |
| belt/window | gpu_post_ms | +0.005 | +0.8 | within threshold |
| belt/window | gpu_cull_ms | +0.005 | +2.5 | within threshold |
| belt/window | gpu_body_shadow_ms | +0.000 | +2.7 | within threshold |
| belt/window | gpu_belt_light_ms | +0.004 | +1.1 | within threshold |
| belt/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_sky_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_bodies_ms | +0.002 | +0.5 | within threshold |
| belt/window | gpu_surface_rocks_ms | +0.002 | +0.6 | within threshold |
| belt/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_atmospheres_ms | +0.002 | +0.3 | within threshold |
| belt/window | gpu_belt_dust_ms | +0.003 | +0.5 | within threshold |
| belt/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_temporal_ms | +0.003 | +1.5 | within threshold |
| belt/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_bloom_ms | +0.001 | +1.2 | within threshold |
| belt/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_composite_ms | +0.002 | +1.3 | within threshold |
| belt/window | gpu_spatial_aa_ms | +0.003 | +1.8 | within threshold |
| belt/window | gpu_meter_ms | +0.001 | +8.0 | within threshold |
| belt/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.038 | +0.3 | within threshold |
| belt/fullscreen | gpu_ms | +0.151 | +1.5 | within threshold |
| belt/fullscreen | cpu_prepare_ms | -0.056 | -13.8 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.122 | +20.9 | within threshold |
| belt/fullscreen | gpu_surface_ms | +0.022 | +0.7 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.013 | +0.3 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.029 | +1.4 | within threshold |
| belt/fullscreen | gpu_cull_ms | +0.005 | +2.3 | within threshold |
| belt/fullscreen | gpu_body_shadow_ms | +0.000 | +2.2 | within threshold |
| belt/fullscreen | gpu_belt_light_ms | +0.118 | +32.6 | within threshold |
| belt/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_sky_ms | +0.001 | +0.2 | within threshold |
| belt/fullscreen | gpu_surface_bodies_ms | +0.004 | +0.4 | within threshold |
| belt/fullscreen | gpu_surface_rocks_ms | +0.011 | +0.9 | within threshold |
| belt/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_atmospheres_ms | +0.008 | +0.3 | within threshold |
| belt/fullscreen | gpu_belt_dust_ms | +0.008 | +0.4 | within threshold |
| belt/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_temporal_ms | +0.012 | +1.8 | within threshold |
| belt/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_bloom_ms | +0.002 | +0.6 | within threshold |
| belt/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_composite_ms | +0.007 | +1.3 | within threshold |
| belt/fullscreen | gpu_spatial_aa_ms | +0.007 | +1.7 | within threshold |
| belt/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_present_ms | +0.001 | +1.4 | within threshold |
| belt-sun/window | cpu_submit_and_wait_ms | -0.058 | -1.8 | within threshold |
| belt-sun/window | gpu_ms | -0.062 | -2.4 | within threshold |
| belt-sun/window | cpu_prepare_ms | +0.000 | +0.1 | within threshold |
| belt-sun/window | gpu_cull_shadow_ms | +0.009 | +1.6 | within threshold |
| belt-sun/window | gpu_surface_ms | +0.001 | +0.2 | within threshold |
| belt-sun/window | gpu_atmosphere_ms | -0.082 | -8.9 | within threshold |
| belt-sun/window | gpu_post_ms | +0.010 | +1.7 | within threshold |
| belt-sun/window | gpu_cull_ms | +0.005 | +2.8 | within threshold |
| belt-sun/window | gpu_body_shadow_ms | +0.000 | +1.7 | within threshold |
| belt-sun/window | gpu_belt_light_ms | +0.004 | +1.1 | within threshold |
| belt-sun/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_sky_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_rocks_ms | +0.001 | +0.7 | within threshold |
| belt-sun/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_atmospheres_ms | -0.086 | -66.1 | within threshold |
| belt-sun/window | gpu_belt_dust_ms | +0.002 | +0.3 | within threshold |
| belt-sun/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_temporal_ms | +0.003 | +1.6 | within threshold |
| belt-sun/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_bloom_ms | +0.002 | +2.7 | within threshold |
| belt-sun/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_composite_ms | +0.003 | +2.0 | within threshold |
| belt-sun/window | gpu_spatial_aa_ms | +0.002 | +1.8 | within threshold |
| belt-sun/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | cpu_submit_and_wait_ms | -0.268 | -3.4 | within threshold |
| belt-sun/fullscreen | gpu_ms | -0.278 | -3.9 | within threshold |
| belt-sun/fullscreen | cpu_prepare_ms | +0.067 | +22.0 | within threshold |
| belt-sun/fullscreen | gpu_cull_shadow_ms | -0.108 | -15.1 | within threshold |
| belt-sun/fullscreen | gpu_surface_ms | +0.004 | +0.2 | within threshold |
| belt-sun/fullscreen | gpu_atmosphere_ms | -0.279 | -9.8 | within threshold |
| belt-sun/fullscreen | gpu_post_ms | +0.034 | +1.8 | within threshold |
| belt-sun/fullscreen | gpu_cull_ms | +0.006 | +3.0 | within threshold |
| belt-sun/fullscreen | gpu_body_shadow_ms | +0.000 | +2.3 | within threshold |
| belt-sun/fullscreen | gpu_belt_light_ms | -0.116 | -23.6 | within threshold |
| belt-sun/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_sky_ms | +0.003 | +0.4 | within threshold |
| belt-sun/fullscreen | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_splats_ms | +0.002 | +1.8 | within threshold |
| belt-sun/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_atmospheres_ms | -0.288 | -51.0 | within threshold |
| belt-sun/fullscreen | gpu_belt_dust_ms | +0.007 | +0.3 | within threshold |
| belt-sun/fullscreen | gpu_splat_mask_ms | +0.002 | +2.0 | within threshold |
| belt-sun/fullscreen | gpu_temporal_ms | +0.013 | +2.0 | within threshold |
| belt-sun/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_bloom_ms | +0.003 | +1.4 | within threshold |
| belt-sun/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_composite_ms | +0.007 | +1.4 | within threshold |
| belt-sun/fullscreen | gpu_spatial_aa_ms | +0.008 | +2.2 | within threshold |
| belt-sun/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_present_ms | +0.002 | +2.1 | within threshold |
