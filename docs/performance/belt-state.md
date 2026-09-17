# Render performance: belt-state

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `c13022dab4ec55919b066b5ce293c29eaa50ee7b`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| earth/window | 1600×900 | 4.546 | 3.159 | 3.358 | 0.580 | 0.953 | 0.983 | 0.637 |
| earth/fullscreen | 3440×1440 | 9.246 | 7.753 | 8.090 | 0.590 | 2.524 | 2.666 | 1.965 |
| giant/window | 1600×900 | 5.316 | 3.927 | 4.157 | 0.615 | 0.913 | 1.767 | 0.628 |
| giant/fullscreen | 3440×1440 | 12.037 | 10.569 | 10.995 | 0.634 | 2.464 | 5.478 | 1.989 |
| moon/window | 1600×900 | 3.167 | 1.787 | 1.908 | 0.474 | 0.609 | 0.076 | 0.626 |
| moon/fullscreen | 3440×1440 | 5.961 | 4.558 | 4.950 | 0.479 | 1.750 | 0.352 | 1.973 |
| mars/window | 1600×900 | 3.530 | 2.139 | 2.275 | 0.475 | 0.620 | 0.440 | 0.600 |
| mars/fullscreen | 3440×1440 | 6.986 | 5.589 | 5.793 | 0.545 | 1.814 | 1.326 | 1.898 |
| dawn/window | 1600×900 | 4.245 | 2.840 | 3.031 | 0.478 | 0.753 | 0.928 | 0.677 |
| dawn/fullscreen | 3440×1440 | 8.447 | 7.034 | 7.283 | 0.480 | 2.003 | 2.531 | 2.009 |
| belt/window | 1600×900 | 5.249 | 3.861 | 3.874 | 0.599 | 1.061 | 1.500 | 0.697 |
| belt/fullscreen | 3440×1440 | 11.958 | 10.539 | 10.753 | 0.618 | 3.138 | 4.603 | 2.179 |
| belt-sun/window | 1600×900 | 3.921 | 2.541 | 2.550 | 0.583 | 0.530 | 0.780 | 0.645 |
| belt-sun/fullscreen | 3440×1440 | 8.104 | 6.686 | 6.746 | 0.605 | 1.699 | 2.397 | 1.986 |

## Children

The scopes inside the groups: indicative, since a scope reads where its commands were issued rather than an exact cost.

| Scene / mode | Scope | Median | p95 | Run range |
| --- | --- | ---: | ---: | ---: |
| earth/window | gpu_cull_ms | 0.193 | 0.195 | 0.193 to 0.194 |
| earth/window | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| earth/window | gpu_belt_light_ms | 0.367 | 0.367 | 0.365 to 0.368 |
| earth/window | gpu_belt_disc_ms | 0.000 | 0.115 | 0.000 to 0.000 |
| earth/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| earth/window | gpu_surface_sky_ms | 0.217 | 0.220 | 0.216 to 0.219 |
| earth/window | gpu_surface_bodies_ms | 0.322 | 0.326 | 0.319 to 0.324 |
| earth/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/window | gpu_surface_splats_ms | 0.005 | 0.006 | 0.005 to 0.005 |
| earth/window | gpu_surface_clouds_ms | 0.175 | 0.178 | 0.174 to 0.176 |
| earth/window | gpu_atmospheres_ms | 0.768 | 0.775 | 0.764 to 0.773 |
| earth/window | gpu_belt_dust_ms | 0.204 | 0.211 | 0.203 to 0.205 |
| earth/window | gpu_splat_mask_ms | 0.008 | 0.009 | 0.008 to 0.008 |
| earth/window | gpu_temporal_ms | 0.204 | 0.205 | 0.203 to 0.205 |
| earth/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| earth/window | gpu_bloom_ms | 0.087 | 0.090 | 0.087 to 0.088 |
| earth/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| earth/window | gpu_composite_ms | 0.171 | 0.172 | 0.170 to 0.172 |
| earth/window | gpu_spatial_aa_ms | 0.133 | 0.135 | 0.133 to 0.134 |
| earth/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| earth/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| earth/fullscreen | gpu_cull_ms | 0.202 | 0.203 | 0.201 to 0.203 |
| earth/fullscreen | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| earth/fullscreen | gpu_belt_light_ms | 0.368 | 0.370 | 0.367 to 0.370 |
| earth/fullscreen | gpu_belt_disc_ms | 0.000 | 0.118 | 0.000 to 0.000 |
| earth/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| earth/fullscreen | gpu_surface_sky_ms | 0.922 | 0.928 | 0.915 to 0.928 |
| earth/fullscreen | gpu_surface_bodies_ms | 0.652 | 0.659 | 0.648 to 0.656 |
| earth/fullscreen | gpu_surface_rocks_ms | 0.024 | 0.025 | 0.024 to 0.024 |
| earth/fullscreen | gpu_surface_splats_ms | 0.004 | 0.006 | 0.004 to 0.004 |
| earth/fullscreen | gpu_surface_clouds_ms | 0.375 | 0.379 | 0.373 to 0.378 |
| earth/fullscreen | gpu_atmospheres_ms | 2.075 | 2.137 | 2.058 to 2.090 |
| earth/fullscreen | gpu_belt_dust_ms | 0.579 | 0.604 | 0.575 to 0.583 |
| earth/fullscreen | gpu_splat_mask_ms | 0.008 | 0.009 | 0.008 to 0.008 |
| earth/fullscreen | gpu_temporal_ms | 0.665 | 0.670 | 0.662 to 0.669 |
| earth/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| earth/fullscreen | gpu_bloom_ms | 0.242 | 0.244 | 0.241 to 0.244 |
| earth/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| earth/fullscreen | gpu_composite_ms | 0.567 | 0.570 | 0.564 to 0.570 |
| earth/fullscreen | gpu_spatial_aa_ms | 0.390 | 0.394 | 0.389 to 0.392 |
| earth/fullscreen | gpu_meter_ms | 0.014 | 0.017 | 0.014 to 0.015 |
| earth/fullscreen | gpu_present_ms | 0.076 | 0.077 | 0.076 to 0.076 |
| giant/window | gpu_cull_ms | 0.229 | 0.232 | 0.229 to 0.230 |
| giant/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.020 |
| giant/window | gpu_belt_light_ms | 0.365 | 0.366 | 0.362 to 0.366 |
| giant/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| giant/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| giant/window | gpu_surface_sky_ms | 0.251 | 0.253 | 0.249 to 0.252 |
| giant/window | gpu_surface_bodies_ms | 0.292 | 0.297 | 0.290 to 0.294 |
| giant/window | gpu_surface_rocks_ms | 0.037 | 0.039 | 0.037 to 0.038 |
| giant/window | gpu_surface_splats_ms | 0.104 | 0.104 | 0.103 to 0.104 |
| giant/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/window | gpu_atmospheres_ms | 0.538 | 0.543 | 0.535 to 0.541 |
| giant/window | gpu_belt_dust_ms | 1.108 | 1.127 | 1.103 to 1.116 |
| giant/window | gpu_splat_mask_ms | 0.119 | 0.120 | 0.118 to 0.119 |
| giant/window | gpu_temporal_ms | 0.206 | 0.208 | 0.206 to 0.207 |
| giant/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/window | gpu_bloom_ms | 0.084 | 0.086 | 0.084 to 0.084 |
| giant/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| giant/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| giant/window | gpu_composite_ms | 0.171 | 0.173 | 0.170 to 0.172 |
| giant/window | gpu_spatial_aa_ms | 0.125 | 0.127 | 0.125 to 0.126 |
| giant/window | gpu_meter_ms | 0.007 | 0.010 | 0.007 to 0.007 |
| giant/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.027 |
| giant/fullscreen | gpu_cull_ms | 0.246 | 0.249 | 0.244 to 0.246 |
| giant/fullscreen | gpu_body_shadow_ms | 0.020 | 0.021 | 0.020 to 0.020 |
| giant/fullscreen | gpu_belt_light_ms | 0.367 | 0.369 | 0.365 to 0.367 |
| giant/fullscreen | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| giant/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| giant/fullscreen | gpu_surface_sky_ms | 0.975 | 0.985 | 0.969 to 0.977 |
| giant/fullscreen | gpu_surface_bodies_ms | 0.654 | 0.663 | 0.648 to 0.655 |
| giant/fullscreen | gpu_surface_rocks_ms | 0.112 | 0.117 | 0.112 to 0.113 |
| giant/fullscreen | gpu_surface_splats_ms | 0.184 | 0.189 | 0.184 to 0.185 |
| giant/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/fullscreen | gpu_atmospheres_ms | 1.473 | 1.508 | 1.461 to 1.476 |
| giant/fullscreen | gpu_belt_dust_ms | 3.809 | 3.848 | 3.785 to 3.826 |
| giant/fullscreen | gpu_splat_mask_ms | 0.190 | 0.193 | 0.189 to 0.190 |
| giant/fullscreen | gpu_temporal_ms | 0.684 | 0.690 | 0.680 to 0.686 |
| giant/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/fullscreen | gpu_bloom_ms | 0.253 | 0.256 | 0.251 to 0.254 |
| giant/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| giant/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| giant/fullscreen | gpu_composite_ms | 0.570 | 0.574 | 0.567 to 0.572 |
| giant/fullscreen | gpu_spatial_aa_ms | 0.376 | 0.380 | 0.375 to 0.377 |
| giant/fullscreen | gpu_meter_ms | 0.016 | 0.018 | 0.016 to 0.016 |
| giant/fullscreen | gpu_present_ms | 0.078 | 0.078 | 0.077 to 0.078 |
| moon/window | gpu_cull_ms | 0.088 | 0.088 | 0.087 to 0.088 |
| moon/window | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.018 |
| moon/window | gpu_belt_light_ms | 0.366 | 0.367 | 0.364 to 0.366 |
| moon/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| moon/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/window | gpu_surface_sky_ms | 0.228 | 0.229 | 0.226 to 0.228 |
| moon/window | gpu_surface_bodies_ms | 0.133 | 0.135 | 0.132 to 0.133 |
| moon/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_surface_splats_ms | 0.001 | 0.001 | 0.000 to 0.001 |
| moon/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/window | gpu_atmospheres_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/window | gpu_belt_dust_ms | 0.070 | 0.071 | 0.069 to 0.070 |
| moon/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| moon/window | gpu_temporal_ms | 0.200 | 0.201 | 0.198 to 0.200 |
| moon/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/window | gpu_bloom_ms | 0.079 | 0.080 | 0.078 to 0.079 |
| moon/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| moon/window | gpu_composite_ms | 0.171 | 0.172 | 0.170 to 0.171 |
| moon/window | gpu_spatial_aa_ms | 0.135 | 0.137 | 0.134 to 0.135 |
| moon/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| moon/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| moon/fullscreen | gpu_cull_ms | 0.092 | 0.095 | 0.092 to 0.093 |
| moon/fullscreen | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.018 |
| moon/fullscreen | gpu_belt_light_ms | 0.366 | 0.367 | 0.364 to 0.366 |
| moon/fullscreen | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| moon/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/fullscreen | gpu_surface_sky_ms | 0.922 | 0.924 | 0.910 to 0.922 |
| moon/fullscreen | gpu_surface_bodies_ms | 0.266 | 0.267 | 0.263 to 0.266 |
| moon/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/fullscreen | gpu_atmospheres_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/fullscreen | gpu_belt_dust_ms | 0.343 | 0.344 | 0.339 to 0.343 |
| moon/fullscreen | gpu_splat_mask_ms | 0.005 | 0.005 | 0.005 to 0.005 |
| moon/fullscreen | gpu_temporal_ms | 0.654 | 0.657 | 0.649 to 0.655 |
| moon/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/fullscreen | gpu_bloom_ms | 0.244 | 0.245 | 0.241 to 0.244 |
| moon/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| moon/fullscreen | gpu_composite_ms | 0.568 | 0.569 | 0.563 to 0.568 |
| moon/fullscreen | gpu_spatial_aa_ms | 0.406 | 0.408 | 0.402 to 0.406 |
| moon/fullscreen | gpu_meter_ms | 0.014 | 0.016 | 0.014 to 0.014 |
| moon/fullscreen | gpu_present_ms | 0.075 | 0.076 | 0.075 to 0.075 |
| mars/window | gpu_cull_ms | 0.088 | 0.089 | 0.087 to 0.088 |
| mars/window | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| mars/window | gpu_belt_light_ms | 0.367 | 0.368 | 0.364 to 0.367 |
| mars/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| mars/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| mars/window | gpu_surface_sky_ms | 0.257 | 0.258 | 0.254 to 0.257 |
| mars/window | gpu_surface_bodies_ms | 0.120 | 0.121 | 0.119 to 0.121 |
| mars/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_surface_splats_ms | 0.001 | 0.001 | 0.000 to 0.001 |
| mars/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/window | gpu_atmospheres_ms | 0.367 | 0.372 | 0.362 to 0.367 |
| mars/window | gpu_belt_dust_ms | 0.070 | 0.070 | 0.069 to 0.070 |
| mars/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| mars/window | gpu_temporal_ms | 0.197 | 0.198 | 0.195 to 0.197 |
| mars/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/window | gpu_bloom_ms | 0.077 | 0.079 | 0.077 to 0.078 |
| mars/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| mars/window | gpu_composite_ms | 0.171 | 0.172 | 0.169 to 0.171 |
| mars/window | gpu_spatial_aa_ms | 0.114 | 0.116 | 0.114 to 0.115 |
| mars/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| mars/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| mars/fullscreen | gpu_cull_ms | 0.158 | 0.159 | 0.157 to 0.159 |
| mars/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| mars/fullscreen | gpu_belt_light_ms | 0.366 | 0.367 | 0.364 to 0.367 |
| mars/fullscreen | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| mars/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| mars/fullscreen | gpu_surface_sky_ms | 1.012 | 1.014 | 0.999 to 1.013 |
| mars/fullscreen | gpu_surface_bodies_ms | 0.245 | 0.246 | 0.243 to 0.245 |
| mars/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/fullscreen | gpu_surface_splats_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| mars/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/fullscreen | gpu_atmospheres_ms | 0.943 | 0.964 | 0.932 to 0.944 |
| mars/fullscreen | gpu_belt_dust_ms | 0.370 | 0.383 | 0.366 to 0.370 |
| mars/fullscreen | gpu_splat_mask_ms | 0.006 | 0.007 | 0.006 to 0.006 |
| mars/fullscreen | gpu_temporal_ms | 0.645 | 0.648 | 0.641 to 0.646 |
| mars/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/fullscreen | gpu_bloom_ms | 0.242 | 0.244 | 0.239 to 0.242 |
| mars/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.005 to 0.006 |
| mars/fullscreen | gpu_composite_ms | 0.568 | 0.569 | 0.563 to 0.568 |
| mars/fullscreen | gpu_spatial_aa_ms | 0.341 | 0.344 | 0.340 to 0.342 |
| mars/fullscreen | gpu_meter_ms | 0.014 | 0.016 | 0.014 to 0.014 |
| mars/fullscreen | gpu_present_ms | 0.075 | 0.076 | 0.075 to 0.075 |
| dawn/window | gpu_cull_ms | 0.087 | 0.088 | 0.087 to 0.088 |
| dawn/window | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| dawn/window | gpu_belt_light_ms | 0.370 | 0.371 | 0.369 to 0.371 |
| dawn/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| dawn/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| dawn/window | gpu_surface_sky_ms | 0.133 | 0.135 | 0.132 to 0.133 |
| dawn/window | gpu_surface_bodies_ms | 0.341 | 0.344 | 0.339 to 0.342 |
| dawn/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/window | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/window | gpu_surface_clouds_ms | 0.197 | 0.199 | 0.196 to 0.198 |
| dawn/window | gpu_atmospheres_ms | 0.855 | 0.862 | 0.848 to 0.855 |
| dawn/window | gpu_belt_dust_ms | 0.069 | 0.069 | 0.068 to 0.069 |
| dawn/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| dawn/window | gpu_temporal_ms | 0.207 | 0.208 | 0.206 to 0.207 |
| dawn/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| dawn/window | gpu_bloom_ms | 0.097 | 0.099 | 0.097 to 0.097 |
| dawn/window | gpu_sun_visibility_ms | 0.028 | 0.028 | 0.028 to 0.028 |
| dawn/window | gpu_flare_ms | 0.009 | 0.010 | 0.009 to 0.009 |
| dawn/window | gpu_composite_ms | 0.178 | 0.179 | 0.177 to 0.178 |
| dawn/window | gpu_spatial_aa_ms | 0.120 | 0.122 | 0.120 to 0.121 |
| dawn/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| dawn/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| dawn/fullscreen | gpu_cull_ms | 0.088 | 0.089 | 0.088 to 0.088 |
| dawn/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| dawn/fullscreen | gpu_belt_light_ms | 0.371 | 0.373 | 0.370 to 0.372 |
| dawn/fullscreen | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| dawn/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_sky_ms | 0.633 | 0.639 | 0.631 to 0.636 |
| dawn/fullscreen | gpu_surface_bodies_ms | 0.708 | 0.715 | 0.706 to 0.711 |
| dawn/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_splats_ms | 0.001 | 0.001 | 0.000 to 0.001 |
| dawn/fullscreen | gpu_surface_clouds_ms | 0.451 | 0.455 | 0.449 to 0.452 |
| dawn/fullscreen | gpu_atmospheres_ms | 2.293 | 2.335 | 2.275 to 2.299 |
| dawn/fullscreen | gpu_belt_dust_ms | 0.228 | 0.231 | 0.228 to 0.230 |
| dawn/fullscreen | gpu_splat_mask_ms | 0.005 | 0.005 | 0.005 to 0.005 |
| dawn/fullscreen | gpu_temporal_ms | 0.678 | 0.681 | 0.674 to 0.679 |
| dawn/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| dawn/fullscreen | gpu_bloom_ms | 0.225 | 0.228 | 0.224 to 0.226 |
| dawn/fullscreen | gpu_sun_visibility_ms | 0.030 | 0.031 | 0.030 to 0.030 |
| dawn/fullscreen | gpu_flare_ms | 0.024 | 0.024 | 0.023 to 0.024 |
| dawn/fullscreen | gpu_composite_ms | 0.592 | 0.594 | 0.591 to 0.593 |
| dawn/fullscreen | gpu_spatial_aa_ms | 0.364 | 0.368 | 0.362 to 0.365 |
| dawn/fullscreen | gpu_meter_ms | 0.015 | 0.018 | 0.015 to 0.015 |
| dawn/fullscreen | gpu_present_ms | 0.075 | 0.076 | 0.075 to 0.075 |
| belt/window | gpu_cull_ms | 0.212 | 0.214 | 0.211 to 0.213 |
| belt/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| belt/window | gpu_belt_light_ms | 0.366 | 0.367 | 0.365 to 0.367 |
| belt/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt/window | gpu_surface_sky_ms | 0.110 | 0.112 | 0.109 to 0.110 |
| belt/window | gpu_surface_bodies_ms | 0.438 | 0.443 | 0.435 to 0.439 |
| belt/window | gpu_surface_rocks_ms | 0.345 | 0.351 | 0.343 to 0.346 |
| belt/window | gpu_surface_splats_ms | 0.049 | 0.050 | 0.048 to 0.049 |
| belt/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_atmospheres_ms | 0.837 | 0.843 | 0.830 to 0.837 |
| belt/window | gpu_belt_dust_ms | 0.599 | 0.604 | 0.595 to 0.601 |
| belt/window | gpu_splat_mask_ms | 0.060 | 0.061 | 0.060 to 0.061 |
| belt/window | gpu_temporal_ms | 0.214 | 0.215 | 0.213 to 0.215 |
| belt/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_bloom_ms | 0.087 | 0.089 | 0.087 to 0.087 |
| belt/window | gpu_sun_visibility_ms | 0.028 | 0.029 | 0.028 to 0.028 |
| belt/window | gpu_flare_ms | 0.009 | 0.010 | 0.009 to 0.009 |
| belt/window | gpu_composite_ms | 0.178 | 0.179 | 0.177 to 0.178 |
| belt/window | gpu_spatial_aa_ms | 0.142 | 0.144 | 0.141 to 0.142 |
| belt/window | gpu_meter_ms | 0.008 | 0.010 | 0.008 to 0.008 |
| belt/window | gpu_present_ms | 0.027 | 0.027 | 0.026 to 0.027 |
| belt/fullscreen | gpu_cull_ms | 0.230 | 0.232 | 0.229 to 0.230 |
| belt/fullscreen | gpu_body_shadow_ms | 0.020 | 0.021 | 0.020 to 0.020 |
| belt/fullscreen | gpu_belt_light_ms | 0.367 | 0.369 | 0.366 to 0.368 |
| belt/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt/fullscreen | gpu_surface_sky_ms | 0.505 | 0.509 | 0.503 to 0.508 |
| belt/fullscreen | gpu_surface_bodies_ms | 1.012 | 1.021 | 1.008 to 1.016 |
| belt/fullscreen | gpu_surface_rocks_ms | 1.280 | 1.289 | 1.273 to 1.284 |
| belt/fullscreen | gpu_surface_splats_ms | 0.069 | 0.075 | 0.069 to 0.070 |
| belt/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_atmospheres_ms | 2.543 | 2.609 | 2.528 to 2.550 |
| belt/fullscreen | gpu_belt_dust_ms | 1.972 | 1.980 | 1.961 to 1.977 |
| belt/fullscreen | gpu_splat_mask_ms | 0.086 | 0.087 | 0.086 to 0.086 |
| belt/fullscreen | gpu_temporal_ms | 0.711 | 0.716 | 0.708 to 0.713 |
| belt/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_bloom_ms | 0.275 | 0.279 | 0.274 to 0.276 |
| belt/fullscreen | gpu_sun_visibility_ms | 0.030 | 0.031 | 0.030 to 0.030 |
| belt/fullscreen | gpu_flare_ms | 0.024 | 0.024 | 0.023 to 0.024 |
| belt/fullscreen | gpu_composite_ms | 0.593 | 0.596 | 0.592 to 0.595 |
| belt/fullscreen | gpu_spatial_aa_ms | 0.444 | 0.449 | 0.443 to 0.446 |
| belt/fullscreen | gpu_meter_ms | 0.017 | 0.020 | 0.017 to 0.018 |
| belt/fullscreen | gpu_present_ms | 0.078 | 0.079 | 0.078 to 0.078 |
| belt-sun/window | gpu_cull_ms | 0.199 | 0.201 | 0.198 to 0.200 |
| belt-sun/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.020 |
| belt-sun/window | gpu_belt_light_ms | 0.362 | 0.364 | 0.360 to 0.362 |
| belt-sun/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt-sun/window | gpu_surface_sky_ms | 0.231 | 0.234 | 0.229 to 0.232 |
| belt-sun/window | gpu_surface_bodies_ms | 0.002 | 0.006 | 0.001 to 0.002 |
| belt-sun/window | gpu_surface_rocks_ms | 0.143 | 0.147 | 0.142 to 0.144 |
| belt-sun/window | gpu_surface_splats_ms | 0.073 | 0.074 | 0.073 to 0.073 |
| belt-sun/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_atmospheres_ms | 0.045 | 0.045 | 0.045 to 0.045 |
| belt-sun/window | gpu_belt_dust_ms | 0.654 | 0.660 | 0.650 to 0.657 |
| belt-sun/window | gpu_splat_mask_ms | 0.079 | 0.079 | 0.078 to 0.079 |
| belt-sun/window | gpu_temporal_ms | 0.201 | 0.202 | 0.200 to 0.202 |
| belt-sun/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_bloom_ms | 0.079 | 0.081 | 0.078 to 0.079 |
| belt-sun/window | gpu_sun_visibility_ms | 0.028 | 0.029 | 0.028 to 0.028 |
| belt-sun/window | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| belt-sun/window | gpu_composite_ms | 0.173 | 0.173 | 0.172 to 0.173 |
| belt-sun/window | gpu_spatial_aa_ms | 0.120 | 0.122 | 0.120 to 0.120 |
| belt-sun/window | gpu_meter_ms | 0.007 | 0.010 | 0.007 to 0.008 |
| belt-sun/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| belt-sun/fullscreen | gpu_cull_ms | 0.214 | 0.216 | 0.213 to 0.216 |
| belt-sun/fullscreen | gpu_body_shadow_ms | 0.019 | 0.021 | 0.019 to 0.019 |
| belt-sun/fullscreen | gpu_belt_light_ms | 0.370 | 0.372 | 0.369 to 0.370 |
| belt-sun/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt-sun/fullscreen | gpu_surface_sky_ms | 0.882 | 0.888 | 0.876 to 0.883 |
| belt-sun/fullscreen | gpu_surface_bodies_ms | 0.013 | 0.020 | 0.013 to 0.013 |
| belt-sun/fullscreen | gpu_surface_rocks_ms | 0.476 | 0.483 | 0.474 to 0.479 |
| belt-sun/fullscreen | gpu_surface_splats_ms | 0.112 | 0.115 | 0.112 to 0.113 |
| belt-sun/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_atmospheres_ms | 0.280 | 0.283 | 0.279 to 0.281 |
| belt-sun/fullscreen | gpu_belt_dust_ms | 2.009 | 2.028 | 1.999 to 2.017 |
| belt-sun/fullscreen | gpu_splat_mask_ms | 0.105 | 0.108 | 0.105 to 0.105 |
| belt-sun/fullscreen | gpu_temporal_ms | 0.658 | 0.664 | 0.657 to 0.660 |
| belt-sun/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_bloom_ms | 0.219 | 0.222 | 0.218 to 0.220 |
| belt-sun/fullscreen | gpu_sun_visibility_ms | 0.031 | 0.032 | 0.031 to 0.032 |
| belt-sun/fullscreen | gpu_flare_ms | 0.014 | 0.015 | 0.014 to 0.015 |
| belt-sun/fullscreen | gpu_composite_ms | 0.573 | 0.578 | 0.571 to 0.574 |
| belt-sun/fullscreen | gpu_spatial_aa_ms | 0.390 | 0.395 | 0.388 to 0.391 |
| belt-sun/fullscreen | gpu_meter_ms | 0.016 | 0.019 | 0.016 to 0.016 |
| belt-sun/fullscreen | gpu_present_ms | 0.077 | 0.081 | 0.077 to 0.077 |

## Compared with static-heap

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| earth/window | cpu_submit_and_wait_ms | +0.678 | +17.5 | regression |
| earth/window | gpu_ms | -0.001 | -0.0 | within threshold |
| earth/window | cpu_prepare_ms | +0.689 | +270.7 | regression |
| earth/window | gpu_cull_shadow_ms | +0.025 | +4.5 | within threshold |
| earth/window | gpu_surface_ms | -0.007 | -0.7 | within threshold |
| earth/window | gpu_atmosphere_ms | -0.007 | -0.7 | within threshold |
| earth/window | gpu_post_ms | -0.004 | -0.6 | within threshold |
| earth/window | gpu_cull_ms | +0.027 | +16.1 | within threshold |
| earth/window | gpu_body_shadow_ms | -0.000 | -0.5 | within threshold |
| earth/window | gpu_belt_light_ms | -0.001 | -0.3 | within threshold |
| earth/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_sky_ms | -0.002 | -0.7 | within threshold |
| earth/window | gpu_surface_bodies_ms | -0.003 | -0.9 | within threshold |
| earth/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_clouds_ms | -0.001 | -0.6 | within threshold |
| earth/window | gpu_atmospheres_ms | -0.005 | -0.7 | within threshold |
| earth/window | gpu_belt_dust_ms | -0.001 | -0.5 | within threshold |
| earth/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_temporal_ms | -0.001 | -0.5 | within threshold |
| earth/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_bloom_ms | -0.001 | -1.2 | within threshold |
| earth/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_composite_ms | -0.001 | -0.6 | within threshold |
| earth/window | gpu_spatial_aa_ms | -0.001 | -0.8 | within threshold |
| earth/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | cpu_submit_and_wait_ms | +0.419 | +4.8 | within threshold |
| earth/fullscreen | gpu_ms | -0.225 | -2.8 | within threshold |
| earth/fullscreen | cpu_prepare_ms | +0.675 | +210.2 | regression |
| earth/fullscreen | gpu_cull_shadow_ms | -0.099 | -14.4 | within threshold |
| earth/fullscreen | gpu_surface_ms | -0.038 | -1.5 | within threshold |
| earth/fullscreen | gpu_atmosphere_ms | -0.029 | -1.1 | within threshold |
| earth/fullscreen | gpu_post_ms | -0.022 | -1.1 | within threshold |
| earth/fullscreen | gpu_cull_ms | +0.031 | +18.5 | within threshold |
| earth/fullscreen | gpu_body_shadow_ms | -0.000 | -0.7 | within threshold |
| earth/fullscreen | gpu_belt_light_ms | -0.130 | -26.1 | within threshold |
| earth/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_sky_ms | -0.009 | -1.0 | within threshold |
| earth/fullscreen | gpu_surface_bodies_ms | -0.007 | -1.1 | within threshold |
| earth/fullscreen | gpu_surface_rocks_ms | -0.001 | -4.2 | within threshold |
| earth/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_clouds_ms | -0.004 | -1.1 | within threshold |
| earth/fullscreen | gpu_atmospheres_ms | -0.019 | -0.9 | within threshold |
| earth/fullscreen | gpu_belt_dust_ms | -0.008 | -1.4 | within threshold |
| earth/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_temporal_ms | -0.005 | -0.8 | within threshold |
| earth/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_bloom_ms | -0.002 | -0.8 | within threshold |
| earth/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_composite_ms | -0.006 | -1.1 | within threshold |
| earth/fullscreen | gpu_spatial_aa_ms | -0.004 | -1.0 | within threshold |
| earth/fullscreen | gpu_meter_ms | -0.001 | -5.8 | within threshold |
| earth/fullscreen | gpu_present_ms | +0.000 | +0.0 | within threshold |
| giant/window | cpu_submit_and_wait_ms | +0.588 | +12.4 | regression |
| giant/window | gpu_ms | -0.010 | -0.2 | within threshold |
| giant/window | cpu_prepare_ms | +0.640 | +193.7 | regression |
| giant/window | gpu_cull_shadow_ms | +0.017 | +2.8 | within threshold |
| giant/window | gpu_surface_ms | -0.009 | -0.9 | within threshold |
| giant/window | gpu_atmosphere_ms | -0.013 | -0.7 | within threshold |
| giant/window | gpu_post_ms | -0.004 | -0.6 | within threshold |
| giant/window | gpu_cull_ms | +0.018 | +8.5 | within threshold |
| giant/window | gpu_body_shadow_ms | +0.000 | +1.4 | within threshold |
| giant/window | gpu_belt_light_ms | -0.001 | -0.3 | within threshold |
| giant/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_surface_sky_ms | -0.001 | -0.4 | within threshold |
| giant/window | gpu_surface_bodies_ms | -0.002 | -0.7 | within threshold |
| giant/window | gpu_surface_rocks_ms | -0.001 | -2.7 | within threshold |
| giant/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_atmospheres_ms | -0.003 | -0.6 | within threshold |
| giant/window | gpu_belt_dust_ms | -0.009 | -0.8 | within threshold |
| giant/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_temporal_ms | -0.001 | -0.5 | within threshold |
| giant/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_bloom_ms | -0.001 | -1.2 | within threshold |
| giant/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_composite_ms | -0.001 | -0.6 | within threshold |
| giant/window | gpu_spatial_aa_ms | -0.001 | -0.8 | within threshold |
| giant/window | gpu_meter_ms | -0.001 | -12.5 | within threshold |
| giant/window | gpu_present_ms | -0.001 | -3.7 | within threshold |
| giant/fullscreen | cpu_submit_and_wait_ms | +0.336 | +2.9 | within threshold |
| giant/fullscreen | gpu_ms | -0.286 | -2.6 | within threshold |
| giant/fullscreen | cpu_prepare_ms | +0.693 | +225.8 | regression |
| giant/fullscreen | gpu_cull_shadow_ms | -0.099 | -13.5 | within threshold |
| giant/fullscreen | gpu_surface_ms | -0.102 | -4.0 | within threshold |
| giant/fullscreen | gpu_atmosphere_ms | -0.057 | -1.0 | within threshold |
| giant/fullscreen | gpu_post_ms | -0.019 | -1.0 | within threshold |
| giant/fullscreen | gpu_cull_ms | +0.022 | +9.7 | within threshold |
| giant/fullscreen | gpu_body_shadow_ms | +0.000 | +1.1 | within threshold |
| giant/fullscreen | gpu_belt_light_ms | -0.120 | -24.6 | within threshold |
| giant/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_surface_sky_ms | -0.007 | -0.7 | within threshold |
| giant/fullscreen | gpu_surface_bodies_ms | -0.004 | -0.6 | within threshold |
| giant/fullscreen | gpu_surface_rocks_ms | -0.001 | -0.9 | within threshold |
| giant/fullscreen | gpu_surface_splats_ms | -0.002 | -1.1 | within threshold |
| giant/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_atmospheres_ms | -0.024 | -1.6 | within threshold |
| giant/fullscreen | gpu_belt_dust_ms | -0.037 | -1.0 | within threshold |
| giant/fullscreen | gpu_splat_mask_ms | +0.001 | +0.5 | within threshold |
| giant/fullscreen | gpu_temporal_ms | -0.004 | -0.6 | within threshold |
| giant/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_bloom_ms | -0.002 | -0.8 | within threshold |
| giant/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_composite_ms | -0.006 | -1.1 | within threshold |
| giant/fullscreen | gpu_spatial_aa_ms | -0.004 | -1.1 | within threshold |
| giant/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_present_ms | +0.000 | +0.0 | within threshold |
| moon/window | cpu_submit_and_wait_ms | +0.589 | +22.8 | regression |
| moon/window | gpu_ms | -0.136 | -7.1 | within threshold |
| moon/window | cpu_prepare_ms | +0.699 | +282.0 | regression |
| moon/window | gpu_cull_shadow_ms | -0.033 | -6.5 | within threshold |
| moon/window | gpu_surface_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_atmosphere_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_post_ms | -0.001 | -0.2 | within threshold |
| moon/window | gpu_cull_ms | -0.026 | -23.2 | within threshold |
| moon/window | gpu_body_shadow_ms | -0.000 | -0.3 | within threshold |
| moon/window | gpu_belt_light_ms | -0.001 | -0.3 | within threshold |
| moon/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_sky_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_splats_ms | -0.001 | -50.0 | within threshold |
| moon/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_belt_dust_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_temporal_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_bloom_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_composite_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_spatial_aa_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | cpu_submit_and_wait_ms | +0.402 | +7.2 | regression |
| moon/fullscreen | gpu_ms | -0.256 | -5.3 | within threshold |
| moon/fullscreen | cpu_prepare_ms | +0.702 | +256.8 | regression |
| moon/fullscreen | gpu_cull_shadow_ms | -0.159 | -24.9 | within threshold |
| moon/fullscreen | gpu_surface_ms | -0.098 | -5.3 | within threshold |
| moon/fullscreen | gpu_atmosphere_ms | -0.001 | -0.3 | within threshold |
| moon/fullscreen | gpu_post_ms | -0.006 | -0.3 | within threshold |
| moon/fullscreen | gpu_cull_ms | -0.027 | -22.8 | within threshold |
| moon/fullscreen | gpu_body_shadow_ms | -0.000 | -0.9 | within threshold |
| moon/fullscreen | gpu_belt_light_ms | -0.133 | -26.7 | within threshold |
| moon/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_surface_prepass_ms | -0.112 | -99.1 | within threshold |
| moon/fullscreen | gpu_surface_sky_ms | -0.001 | -0.1 | within threshold |
| moon/fullscreen | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_belt_dust_ms | -0.001 | -0.3 | within threshold |
| moon/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_temporal_ms | -0.003 | -0.5 | within threshold |
| moon/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_bloom_ms | -0.001 | -0.4 | within threshold |
| moon/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_composite_ms | -0.001 | -0.2 | within threshold |
| moon/fullscreen | gpu_spatial_aa_ms | -0.002 | -0.5 | within threshold |
| moon/fullscreen | gpu_meter_ms | -0.000 | -2.1 | within threshold |
| moon/fullscreen | gpu_present_ms | +0.000 | +0.0 | within threshold |
| mars/window | cpu_submit_and_wait_ms | +0.622 | +21.4 | regression |
| mars/window | gpu_ms | -0.137 | -6.0 | within threshold |
| mars/window | cpu_prepare_ms | +0.699 | +282.0 | regression |
| mars/window | gpu_cull_shadow_ms | -0.142 | -23.0 | within threshold |
| mars/window | gpu_surface_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_atmosphere_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_post_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_cull_ms | -0.026 | -23.0 | within threshold |
| mars/window | gpu_body_shadow_ms | -0.000 | -1.7 | within threshold |
| mars/window | gpu_belt_light_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_sky_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_belt_dust_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_temporal_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_bloom_ms | -0.001 | -0.7 | within threshold |
| mars/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_composite_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_spatial_aa_ms | -0.001 | -0.4 | within threshold |
| mars/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | cpu_submit_and_wait_ms | +0.377 | +5.7 | regression |
| mars/fullscreen | gpu_ms | -0.218 | -3.8 | within threshold |
| mars/fullscreen | cpu_prepare_ms | +0.613 | +166.6 | regression |
| mars/fullscreen | gpu_cull_shadow_ms | -0.124 | -18.5 | within threshold |
| mars/fullscreen | gpu_surface_ms | -0.034 | -1.8 | within threshold |
| mars/fullscreen | gpu_atmosphere_ms | -0.009 | -0.7 | within threshold |
| mars/fullscreen | gpu_post_ms | -0.014 | -0.7 | within threshold |
| mars/fullscreen | gpu_cull_ms | +0.005 | +3.1 | within threshold |
| mars/fullscreen | gpu_body_shadow_ms | -0.000 | -0.3 | within threshold |
| mars/fullscreen | gpu_belt_light_ms | -0.127 | -25.8 | within threshold |
| mars/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_sky_ms | -0.007 | -0.7 | within threshold |
| mars/fullscreen | gpu_surface_bodies_ms | -0.002 | -0.8 | within threshold |
| mars/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_atmospheres_ms | -0.006 | -0.6 | within threshold |
| mars/fullscreen | gpu_belt_dust_ms | -0.004 | -1.1 | within threshold |
| mars/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_temporal_ms | -0.005 | -0.8 | within threshold |
| mars/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_bloom_ms | -0.002 | -0.8 | within threshold |
| mars/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_composite_ms | -0.004 | -0.7 | within threshold |
| mars/fullscreen | gpu_spatial_aa_ms | -0.003 | -0.9 | within threshold |
| mars/fullscreen | gpu_meter_ms | -0.000 | -0.1 | within threshold |
| mars/fullscreen | gpu_present_ms | +0.000 | +0.0 | within threshold |
| dawn/window | cpu_submit_and_wait_ms | +0.640 | +17.8 | regression |
| dawn/window | gpu_ms | -0.027 | -1.0 | within threshold |
| dawn/window | cpu_prepare_ms | +0.689 | +268.2 | regression |
| dawn/window | gpu_cull_shadow_ms | -0.027 | -5.3 | within threshold |
| dawn/window | gpu_surface_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_atmosphere_ms | +0.002 | +0.2 | within threshold |
| dawn/window | gpu_post_ms | -0.002 | -0.3 | within threshold |
| dawn/window | gpu_cull_ms | -0.026 | -23.3 | within threshold |
| dawn/window | gpu_body_shadow_ms | -0.000 | -0.2 | within threshold |
| dawn/window | gpu_belt_light_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_sky_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_bodies_ms | -0.001 | -0.3 | within threshold |
| dawn/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_clouds_ms | -0.001 | -0.5 | within threshold |
| dawn/window | gpu_atmospheres_ms | +0.002 | +0.2 | within threshold |
| dawn/window | gpu_belt_dust_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_temporal_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_bloom_ms | -0.001 | -1.0 | within threshold |
| dawn/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_composite_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_spatial_aa_ms | -0.001 | -0.8 | within threshold |
| dawn/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | cpu_submit_and_wait_ms | +0.338 | +4.2 | within threshold |
| dawn/fullscreen | gpu_ms | -0.333 | -4.5 | within threshold |
| dawn/fullscreen | cpu_prepare_ms | +0.707 | +246.6 | regression |
| dawn/fullscreen | gpu_cull_shadow_ms | -0.163 | -25.3 | within threshold |
| dawn/fullscreen | gpu_surface_ms | -0.158 | -7.3 | within threshold |
| dawn/fullscreen | gpu_atmosphere_ms | -0.024 | -0.9 | within threshold |
| dawn/fullscreen | gpu_post_ms | -0.008 | -0.4 | within threshold |
| dawn/fullscreen | gpu_cull_ms | -0.027 | -23.2 | within threshold |
| dawn/fullscreen | gpu_body_shadow_ms | -0.000 | -0.3 | within threshold |
| dawn/fullscreen | gpu_belt_light_ms | -0.136 | -26.9 | within threshold |
| dawn/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_sky_ms | -0.002 | -0.3 | within threshold |
| dawn/fullscreen | gpu_surface_bodies_ms | -0.089 | -11.2 | within threshold |
| dawn/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_clouds_ms | -0.010 | -2.1 | within threshold |
| dawn/fullscreen | gpu_atmospheres_ms | -0.024 | -1.0 | within threshold |
| dawn/fullscreen | gpu_belt_dust_ms | -0.002 | -0.9 | within threshold |
| dawn/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_temporal_ms | -0.002 | -0.3 | within threshold |
| dawn/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_bloom_ms | -0.002 | -0.9 | within threshold |
| dawn/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_composite_ms | -0.003 | -0.5 | within threshold |
| dawn/fullscreen | gpu_spatial_aa_ms | -0.003 | -0.8 | within threshold |
| dawn/fullscreen | gpu_meter_ms | -0.001 | -4.4 | within threshold |
| dawn/fullscreen | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt/window | cpu_submit_and_wait_ms | +0.607 | +13.1 | regression |
| belt/window | gpu_ms | +0.011 | +0.3 | within threshold |
| belt/window | cpu_prepare_ms | +0.614 | +184.2 | regression |
| belt/window | gpu_cull_shadow_ms | +0.015 | +2.6 | within threshold |
| belt/window | gpu_surface_ms | -0.001 | -0.1 | within threshold |
| belt/window | gpu_atmosphere_ms | +0.001 | +0.1 | within threshold |
| belt/window | gpu_post_ms | -0.001 | -0.1 | within threshold |
| belt/window | gpu_cull_ms | +0.016 | +8.1 | within threshold |
| belt/window | gpu_body_shadow_ms | +0.000 | +2.0 | within threshold |
| belt/window | gpu_belt_light_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_sky_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_atmospheres_ms | +0.001 | +0.1 | within threshold |
| belt/window | gpu_belt_dust_ms | -0.001 | -0.2 | within threshold |
| belt/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_temporal_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_bloom_ms | -0.001 | -1.2 | within threshold |
| belt/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_composite_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_spatial_aa_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_present_ms | +0.000 | +1.2 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.228 | +1.9 | within threshold |
| belt/fullscreen | gpu_ms | -0.271 | -2.5 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.653 | +190.2 | regression |
| belt/fullscreen | gpu_cull_shadow_ms | -0.079 | -11.3 | within threshold |
| belt/fullscreen | gpu_surface_ms | -0.069 | -2.1 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | -0.068 | -1.5 | within threshold |
| belt/fullscreen | gpu_post_ms | -0.017 | -0.8 | within threshold |
| belt/fullscreen | gpu_cull_ms | +0.020 | +9.8 | within threshold |
| belt/fullscreen | gpu_body_shadow_ms | +0.000 | +0.3 | within threshold |
| belt/fullscreen | gpu_belt_light_ms | -0.100 | -21.4 | within threshold |
| belt/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_sky_ms | -0.004 | -0.8 | within threshold |
| belt/fullscreen | gpu_surface_bodies_ms | -0.008 | -0.8 | within threshold |
| belt/fullscreen | gpu_surface_rocks_ms | -0.011 | -0.9 | within threshold |
| belt/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_atmospheres_ms | -0.040 | -1.5 | within threshold |
| belt/fullscreen | gpu_belt_dust_ms | -0.012 | -0.6 | within threshold |
| belt/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_temporal_ms | -0.001 | -0.1 | within threshold |
| belt/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_bloom_ms | -0.001 | -0.4 | within threshold |
| belt/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_composite_ms | -0.004 | -0.7 | within threshold |
| belt/fullscreen | gpu_spatial_aa_ms | -0.004 | -0.8 | within threshold |
| belt/fullscreen | gpu_meter_ms | -0.001 | -5.6 | within threshold |
| belt/fullscreen | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | cpu_submit_and_wait_ms | +0.667 | +20.5 | regression |
| belt-sun/window | gpu_ms | -0.006 | -0.2 | within threshold |
| belt-sun/window | cpu_prepare_ms | +0.707 | +276.6 | regression |
| belt-sun/window | gpu_cull_shadow_ms | -0.001 | -0.1 | within threshold |
| belt-sun/window | gpu_surface_ms | -0.001 | -0.2 | within threshold |
| belt-sun/window | gpu_atmosphere_ms | -0.002 | -0.3 | within threshold |
| belt-sun/window | gpu_post_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_cull_ms | +0.001 | +0.3 | within threshold |
| belt-sun/window | gpu_body_shadow_ms | -0.000 | -0.2 | within threshold |
| belt-sun/window | gpu_belt_light_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_sky_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_belt_dust_ms | -0.002 | -0.3 | within threshold |
| belt-sun/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_temporal_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_bloom_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_composite_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_spatial_aa_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_meter_ms | -0.001 | -10.2 | within threshold |
| belt-sun/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | cpu_submit_and_wait_ms | +0.386 | +5.0 | within threshold |
| belt-sun/fullscreen | gpu_ms | -0.116 | -1.7 | within threshold |
| belt-sun/fullscreen | cpu_prepare_ms | +0.660 | +197.0 | regression |
| belt-sun/fullscreen | gpu_cull_shadow_ms | -0.106 | -14.9 | within threshold |
| belt-sun/fullscreen | gpu_surface_ms | -0.018 | -1.1 | within threshold |
| belt-sun/fullscreen | gpu_atmosphere_ms | -0.015 | -0.6 | within threshold |
| belt-sun/fullscreen | gpu_post_ms | -0.013 | -0.7 | within threshold |
| belt-sun/fullscreen | gpu_cull_ms | +0.007 | +3.3 | within threshold |
| belt-sun/fullscreen | gpu_body_shadow_ms | -0.000 | -2.2 | within threshold |
| belt-sun/fullscreen | gpu_belt_light_ms | -0.113 | -23.4 | within threshold |
| belt-sun/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_sky_ms | -0.004 | -0.5 | within threshold |
| belt-sun/fullscreen | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_rocks_ms | -0.006 | -1.3 | within threshold |
| belt-sun/fullscreen | gpu_surface_splats_ms | -0.001 | -0.9 | within threshold |
| belt-sun/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_atmospheres_ms | -0.002 | -0.7 | within threshold |
| belt-sun/fullscreen | gpu_belt_dust_ms | -0.012 | -0.6 | within threshold |
| belt-sun/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_temporal_ms | -0.003 | -0.5 | within threshold |
| belt-sun/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_bloom_ms | -0.002 | -0.9 | within threshold |
| belt-sun/fullscreen | gpu_sun_visibility_ms | -0.001 | -3.2 | within threshold |
| belt-sun/fullscreen | gpu_flare_ms | -0.001 | -6.7 | within threshold |
| belt-sun/fullscreen | gpu_composite_ms | -0.004 | -0.7 | within threshold |
| belt-sun/fullscreen | gpu_spatial_aa_ms | -0.003 | -0.8 | within threshold |
| belt-sun/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_present_ms | +0.000 | +0.0 | within threshold |

## Compared with headless-baseline

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| earth/window | cpu_submit_and_wait_ms | +0.668 | +17.2 | regression |
| earth/window | gpu_ms | +0.040 | +1.3 | within threshold |
| earth/window | cpu_prepare_ms | +0.689 | +271.3 | regression |
| earth/window | gpu_cull_shadow_ms | +0.041 | +7.5 | within threshold |
| earth/window | gpu_surface_ms | +0.001 | +0.1 | within threshold |
| earth/window | gpu_atmosphere_ms | -0.037 | -3.6 | within threshold |
| earth/window | gpu_post_ms | +0.035 | +5.8 | within threshold |
| earth/window | gpu_cull_ms | +0.034 | +21.7 | within threshold |
| earth/window | gpu_body_shadow_ms | +0.001 | +5.4 | within threshold |
| earth/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| earth/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_sky_ms | +0.007 | +3.4 | within threshold |
| earth/window | gpu_surface_bodies_ms | -0.010 | -3.1 | within threshold |
| earth/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_splats_ms | +0.001 | +25.0 | within threshold |
| earth/window | gpu_surface_clouds_ms | +0.003 | +1.8 | within threshold |
| earth/window | gpu_atmospheres_ms | -0.033 | -4.1 | within threshold |
| earth/window | gpu_belt_dust_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_temporal_ms | +0.009 | +4.7 | within threshold |
| earth/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_bloom_ms | +0.004 | +4.9 | within threshold |
| earth/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_composite_ms | +0.018 | +12.1 | within threshold |
| earth/window | gpu_spatial_aa_ms | +0.003 | +2.4 | within threshold |
| earth/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | cpu_submit_and_wait_ms | +0.383 | +4.3 | within threshold |
| earth/fullscreen | gpu_ms | -0.254 | -3.2 | within threshold |
| earth/fullscreen | cpu_prepare_ms | +0.692 | +227.5 | regression |
| earth/fullscreen | gpu_cull_shadow_ms | -0.081 | -12.1 | within threshold |
| earth/fullscreen | gpu_surface_ms | -0.006 | -0.2 | within threshold |
| earth/fullscreen | gpu_atmosphere_ms | -0.278 | -9.4 | within threshold |
| earth/fullscreen | gpu_post_ms | +0.100 | +5.4 | within threshold |
| earth/fullscreen | gpu_cull_ms | +0.039 | +24.3 | within threshold |
| earth/fullscreen | gpu_body_shadow_ms | +0.001 | +5.8 | within threshold |
| earth/fullscreen | gpu_belt_light_ms | -0.119 | -24.4 | within threshold |
| earth/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_surface_prepass_ms | -0.001 | -33.3 | within threshold |
| earth/fullscreen | gpu_surface_sky_ms | +0.016 | +1.8 | within threshold |
| earth/fullscreen | gpu_surface_bodies_ms | -0.023 | -3.3 | within threshold |
| earth/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_splats_ms | +0.001 | +33.3 | within threshold |
| earth/fullscreen | gpu_surface_clouds_ms | +0.005 | +1.4 | within threshold |
| earth/fullscreen | gpu_atmospheres_ms | -0.283 | -12.0 | within threshold |
| earth/fullscreen | gpu_belt_dust_ms | +0.007 | +1.3 | within threshold |
| earth/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_temporal_ms | +0.031 | +4.8 | within threshold |
| earth/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_bloom_ms | +0.002 | +0.9 | within threshold |
| earth/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_flare_ms | +0.001 | +20.0 | within threshold |
| earth/fullscreen | gpu_composite_ms | +0.057 | +11.2 | within threshold |
| earth/fullscreen | gpu_spatial_aa_ms | +0.009 | +2.4 | within threshold |
| earth/fullscreen | gpu_meter_ms | +0.000 | +0.9 | within threshold |
| earth/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| giant/window | cpu_submit_and_wait_ms | +0.557 | +11.7 | regression |
| giant/window | gpu_ms | -0.049 | -1.2 | within threshold |
| giant/window | cpu_prepare_ms | +0.714 | +279.3 | regression |
| giant/window | gpu_cull_shadow_ms | +0.032 | +5.4 | within threshold |
| giant/window | gpu_surface_ms | -0.014 | -1.5 | within threshold |
| giant/window | gpu_atmosphere_ms | -0.097 | -5.2 | within threshold |
| giant/window | gpu_post_ms | +0.032 | +5.3 | within threshold |
| giant/window | gpu_cull_ms | +0.026 | +12.7 | within threshold |
| giant/window | gpu_body_shadow_ms | +0.001 | +6.5 | within threshold |
| giant/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| giant/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_surface_sky_ms | +0.008 | +3.4 | within threshold |
| giant/window | gpu_surface_bodies_ms | -0.018 | -5.9 | within threshold |
| giant/window | gpu_surface_rocks_ms | -0.002 | -5.3 | within threshold |
| giant/window | gpu_surface_splats_ms | +0.001 | +1.0 | within threshold |
| giant/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_atmospheres_ms | -0.002 | -0.4 | within threshold |
| giant/window | gpu_belt_dust_ms | -0.095 | -7.9 | within threshold |
| giant/window | gpu_splat_mask_ms | +0.001 | +0.9 | within threshold |
| giant/window | gpu_temporal_ms | +0.007 | +3.6 | within threshold |
| giant/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_bloom_ms | +0.003 | +3.8 | within threshold |
| giant/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_composite_ms | +0.018 | +12.1 | within threshold |
| giant/window | gpu_spatial_aa_ms | +0.003 | +2.5 | within threshold |
| giant/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | cpu_submit_and_wait_ms | +0.078 | +0.7 | within threshold |
| giant/fullscreen | gpu_ms | -0.462 | -4.2 | within threshold |
| giant/fullscreen | cpu_prepare_ms | +0.627 | +167.7 | regression |
| giant/fullscreen | gpu_cull_shadow_ms | -0.066 | -9.5 | within threshold |
| giant/fullscreen | gpu_surface_ms | +0.035 | +1.4 | within threshold |
| giant/fullscreen | gpu_atmosphere_ms | -0.486 | -8.1 | within threshold |
| giant/fullscreen | gpu_post_ms | +0.100 | +5.3 | within threshold |
| giant/fullscreen | gpu_cull_ms | +0.030 | +14.1 | within threshold |
| giant/fullscreen | gpu_body_shadow_ms | +0.001 | +7.0 | within threshold |
| giant/fullscreen | gpu_belt_light_ms | -0.005 | -1.2 | within threshold |
| giant/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_surface_sky_ms | +0.020 | +2.1 | within threshold |
| giant/fullscreen | gpu_surface_bodies_ms | +0.038 | +6.1 | within threshold |
| giant/fullscreen | gpu_surface_rocks_ms | -0.002 | -1.8 | within threshold |
| giant/fullscreen | gpu_surface_splats_ms | +0.002 | +1.1 | within threshold |
| giant/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_atmospheres_ms | -0.170 | -10.4 | within threshold |
| giant/fullscreen | gpu_belt_dust_ms | -0.321 | -7.8 | within threshold |
| giant/fullscreen | gpu_splat_mask_ms | +0.003 | +1.6 | within threshold |
| giant/fullscreen | gpu_temporal_ms | +0.028 | +4.2 | within threshold |
| giant/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_bloom_ms | +0.004 | +1.6 | within threshold |
| giant/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_flare_ms | +0.001 | +9.1 | within threshold |
| giant/fullscreen | gpu_composite_ms | +0.059 | +11.6 | within threshold |
| giant/fullscreen | gpu_spatial_aa_ms | +0.010 | +2.8 | within threshold |
| giant/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_present_ms | +0.003 | +4.1 | within threshold |
| moon/window | cpu_submit_and_wait_ms | +0.670 | +26.8 | regression |
| moon/window | gpu_ms | -0.073 | -3.9 | within threshold |
| moon/window | cpu_prepare_ms | +0.702 | +287.8 | regression |
| moon/window | gpu_cull_shadow_ms | -0.134 | -22.1 | within threshold |
| moon/window | gpu_surface_ms | +0.007 | +1.2 | within threshold |
| moon/window | gpu_atmosphere_ms | +0.016 | +27.6 | within threshold |
| moon/window | gpu_post_ms | +0.037 | +6.3 | within threshold |
| moon/window | gpu_cull_ms | -0.019 | -17.8 | within threshold |
| moon/window | gpu_body_shadow_ms | +0.001 | +5.4 | within threshold |
| moon/window | gpu_belt_light_ms | -0.059 | -13.9 | within threshold |
| moon/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_sky_ms | +0.009 | +4.2 | within threshold |
| moon/window | gpu_surface_bodies_ms | -0.005 | -3.7 | within threshold |
| moon/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_splats_ms | -0.001 | -50.0 | within threshold |
| moon/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_belt_dust_ms | +0.016 | +30.8 | within threshold |
| moon/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_temporal_ms | +0.010 | +5.4 | within threshold |
| moon/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_bloom_ms | +0.003 | +4.1 | within threshold |
| moon/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_composite_ms | +0.018 | +12.1 | within threshold |
| moon/window | gpu_spatial_aa_ms | +0.004 | +3.1 | within threshold |
| moon/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| moon/fullscreen | cpu_submit_and_wait_ms | +0.608 | +11.4 | regression |
| moon/fullscreen | gpu_ms | -0.012 | -0.3 | within threshold |
| moon/fullscreen | cpu_prepare_ms | +0.619 | +173.6 | regression |
| moon/fullscreen | gpu_cull_shadow_ms | -0.142 | -22.9 | within threshold |
| moon/fullscreen | gpu_surface_ms | -0.009 | -0.5 | within threshold |
| moon/fullscreen | gpu_atmosphere_ms | +0.047 | +15.4 | within threshold |
| moon/fullscreen | gpu_post_ms | +0.108 | +5.8 | within threshold |
| moon/fullscreen | gpu_cull_ms | -0.019 | -17.2 | within threshold |
| moon/fullscreen | gpu_body_shadow_ms | +0.001 | +5.1 | within threshold |
| moon/fullscreen | gpu_belt_light_ms | -0.123 | -25.2 | within threshold |
| moon/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_sky_ms | +0.017 | +1.9 | within threshold |
| moon/fullscreen | gpu_surface_bodies_ms | -0.001 | -0.4 | within threshold |
| moon/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_belt_dust_ms | +0.047 | +15.9 | within threshold |
| moon/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_temporal_ms | +0.031 | +4.9 | within threshold |
| moon/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_bloom_ms | +0.003 | +1.3 | within threshold |
| moon/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_composite_ms | +0.059 | +11.7 | within threshold |
| moon/fullscreen | gpu_spatial_aa_ms | +0.011 | +2.9 | within threshold |
| moon/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| mars/window | cpu_submit_and_wait_ms | +0.701 | +24.8 | regression |
| mars/window | gpu_ms | +0.023 | +1.1 | within threshold |
| mars/window | cpu_prepare_ms | +0.701 | +284.9 | regression |
| mars/window | gpu_cull_shadow_ms | -0.013 | -2.6 | within threshold |
| mars/window | gpu_surface_ms | +0.009 | +1.5 | within threshold |
| mars/window | gpu_atmosphere_ms | +0.009 | +2.1 | within threshold |
| mars/window | gpu_post_ms | +0.037 | +6.5 | within threshold |
| mars/window | gpu_cull_ms | -0.019 | -17.6 | within threshold |
| mars/window | gpu_body_shadow_ms | +0.001 | +3.2 | within threshold |
| mars/window | gpu_belt_light_ms | +0.006 | +1.7 | within threshold |
| mars/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_sky_ms | +0.010 | +4.1 | within threshold |
| mars/window | gpu_surface_bodies_ms | -0.001 | -0.8 | within threshold |
| mars/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_atmospheres_ms | -0.006 | -1.6 | within threshold |
| mars/window | gpu_belt_dust_ms | +0.016 | +30.8 | within threshold |
| mars/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_temporal_ms | +0.012 | +6.7 | within threshold |
| mars/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_bloom_ms | +0.003 | +3.4 | within threshold |
| mars/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_composite_ms | +0.019 | +12.8 | within threshold |
| mars/window | gpu_spatial_aa_ms | +0.003 | +2.8 | within threshold |
| mars/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| mars/fullscreen | cpu_submit_and_wait_ms | +0.413 | +6.3 | regression |
| mars/fullscreen | gpu_ms | -0.093 | -1.6 | within threshold |
| mars/fullscreen | cpu_prepare_ms | +0.604 | +160.7 | regression |
| mars/fullscreen | gpu_cull_shadow_ms | -0.102 | -15.8 | within threshold |
| mars/fullscreen | gpu_surface_ms | +0.019 | +1.1 | within threshold |
| mars/fullscreen | gpu_atmosphere_ms | -0.147 | -10.0 | within threshold |
| mars/fullscreen | gpu_post_ms | +0.111 | +6.2 | within threshold |
| mars/fullscreen | gpu_cull_ms | +0.013 | +9.1 | within threshold |
| mars/fullscreen | gpu_body_shadow_ms | +0.001 | +4.5 | within threshold |
| mars/fullscreen | gpu_belt_light_ms | -0.001 | -0.3 | within threshold |
| mars/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_sky_ms | +0.019 | +2.0 | within threshold |
| mars/fullscreen | gpu_surface_bodies_ms | +0.005 | +2.1 | within threshold |
| mars/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_atmospheres_ms | -0.189 | -16.7 | within threshold |
| mars/fullscreen | gpu_belt_dust_ms | +0.039 | +11.8 | within threshold |
| mars/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_temporal_ms | +0.034 | +5.5 | within threshold |
| mars/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_bloom_ms | +0.002 | +0.9 | within threshold |
| mars/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_flare_ms | +0.001 | +9.1 | within threshold |
| mars/fullscreen | gpu_composite_ms | +0.060 | +11.9 | within threshold |
| mars/fullscreen | gpu_spatial_aa_ms | +0.009 | +2.8 | within threshold |
| mars/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| dawn/window | cpu_submit_and_wait_ms | +0.722 | +20.5 | regression |
| dawn/window | gpu_ms | +0.030 | +1.1 | within threshold |
| dawn/window | cpu_prepare_ms | +0.693 | +273.7 | regression |
| dawn/window | gpu_cull_shadow_ms | -0.013 | -2.6 | within threshold |
| dawn/window | gpu_surface_ms | +0.001 | +0.1 | within threshold |
| dawn/window | gpu_atmosphere_ms | +0.006 | +0.6 | within threshold |
| dawn/window | gpu_post_ms | +0.039 | +6.1 | within threshold |
| dawn/window | gpu_cull_ms | -0.019 | -17.9 | within threshold |
| dawn/window | gpu_body_shadow_ms | +0.001 | +5.5 | within threshold |
| dawn/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| dawn/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_sky_ms | +0.002 | +1.6 | within threshold |
| dawn/window | gpu_surface_bodies_ms | -0.006 | -1.8 | within threshold |
| dawn/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_clouds_ms | +0.004 | +2.1 | within threshold |
| dawn/window | gpu_atmospheres_ms | -0.010 | -1.2 | within threshold |
| dawn/window | gpu_belt_dust_ms | +0.016 | +31.4 | within threshold |
| dawn/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_temporal_ms | +0.009 | +4.7 | within threshold |
| dawn/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_bloom_ms | +0.006 | +6.7 | within threshold |
| dawn/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_composite_ms | +0.019 | +12.3 | within threshold |
| dawn/window | gpu_spatial_aa_ms | +0.003 | +2.6 | within threshold |
| dawn/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| dawn/fullscreen | cpu_submit_and_wait_ms | +0.221 | +2.7 | within threshold |
| dawn/fullscreen | gpu_ms | -0.279 | -3.8 | within threshold |
| dawn/fullscreen | cpu_prepare_ms | +0.615 | +162.8 | regression |
| dawn/fullscreen | gpu_cull_shadow_ms | -0.136 | -22.0 | within threshold |
| dawn/fullscreen | gpu_surface_ms | -0.001 | -0.1 | within threshold |
| dawn/fullscreen | gpu_atmosphere_ms | -0.259 | -9.3 | within threshold |
| dawn/fullscreen | gpu_post_ms | +0.115 | +6.1 | within threshold |
| dawn/fullscreen | gpu_cull_ms | -0.019 | -17.7 | within threshold |
| dawn/fullscreen | gpu_body_shadow_ms | +0.001 | +5.8 | within threshold |
| dawn/fullscreen | gpu_belt_light_ms | -0.002 | -0.5 | within threshold |
| dawn/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_sky_ms | +0.011 | +1.7 | within threshold |
| dawn/fullscreen | gpu_surface_bodies_ms | -0.016 | -2.3 | within threshold |
| dawn/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_clouds_ms | +0.005 | +1.1 | within threshold |
| dawn/fullscreen | gpu_atmospheres_ms | -0.316 | -12.1 | within threshold |
| dawn/fullscreen | gpu_belt_dust_ms | +0.055 | +32.0 | within threshold |
| dawn/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_temporal_ms | +0.031 | +4.7 | within threshold |
| dawn/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_bloom_ms | +0.007 | +3.3 | within threshold |
| dawn/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_flare_ms | +0.001 | +4.5 | within threshold |
| dawn/fullscreen | gpu_composite_ms | +0.062 | +11.8 | within threshold |
| dawn/fullscreen | gpu_spatial_aa_ms | +0.010 | +2.9 | within threshold |
| dawn/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| belt/window | cpu_submit_and_wait_ms | +0.670 | +14.6 | regression |
| belt/window | gpu_ms | +0.072 | +1.9 | within threshold |
| belt/window | cpu_prepare_ms | +0.687 | +264.1 | regression |
| belt/window | gpu_cull_shadow_ms | +0.030 | +5.2 | within threshold |
| belt/window | gpu_surface_ms | +0.032 | +3.1 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.018 | -1.2 | within threshold |
| belt/window | gpu_post_ms | +0.030 | +4.4 | within threshold |
| belt/window | gpu_cull_ms | +0.024 | +12.6 | within threshold |
| belt/window | gpu_body_shadow_ms | +0.001 | +7.8 | within threshold |
| belt/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| belt/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_sky_ms | +0.004 | +3.9 | within threshold |
| belt/window | gpu_surface_bodies_ms | +0.031 | +7.5 | within threshold |
| belt/window | gpu_surface_rocks_ms | -0.003 | -0.9 | within threshold |
| belt/window | gpu_surface_splats_ms | +0.001 | +2.1 | within threshold |
| belt/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_atmospheres_ms | +0.032 | +3.9 | within threshold |
| belt/window | gpu_belt_dust_ms | -0.051 | -7.9 | within threshold |
| belt/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_temporal_ms | +0.007 | +3.5 | within threshold |
| belt/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_bloom_ms | +0.003 | +3.7 | within threshold |
| belt/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_composite_ms | +0.019 | +12.3 | within threshold |
| belt/window | gpu_spatial_aa_ms | +0.004 | +2.6 | within threshold |
| belt/window | gpu_meter_ms | +0.001 | +14.3 | within threshold |
| belt/window | gpu_present_ms | +0.001 | +4.0 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.532 | +4.7 | within threshold |
| belt/fullscreen | gpu_ms | +0.105 | +1.0 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.592 | +146.0 | regression |
| belt/fullscreen | gpu_cull_shadow_ms | +0.033 | +5.6 | within threshold |
| belt/fullscreen | gpu_surface_ms | +0.098 | +3.2 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | -0.098 | -2.1 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.103 | +5.0 | within threshold |
| belt/fullscreen | gpu_cull_ms | +0.029 | +14.2 | within threshold |
| belt/fullscreen | gpu_body_shadow_ms | +0.001 | +6.2 | within threshold |
| belt/fullscreen | gpu_belt_light_ms | +0.004 | +1.1 | within threshold |
| belt/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_sky_ms | +0.006 | +1.2 | within threshold |
| belt/fullscreen | gpu_surface_bodies_ms | +0.102 | +11.2 | within threshold |
| belt/fullscreen | gpu_surface_rocks_ms | -0.012 | -1.0 | within threshold |
| belt/fullscreen | gpu_surface_splats_ms | +0.002 | +3.1 | within threshold |
| belt/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_atmospheres_ms | +0.083 | +3.4 | within threshold |
| belt/fullscreen | gpu_belt_dust_ms | -0.180 | -8.4 | within threshold |
| belt/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_temporal_ms | +0.024 | +3.4 | within threshold |
| belt/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_bloom_ms | +0.003 | +0.9 | within threshold |
| belt/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_flare_ms | +0.001 | +4.5 | within threshold |
| belt/fullscreen | gpu_composite_ms | +0.061 | +11.6 | within threshold |
| belt/fullscreen | gpu_spatial_aa_ms | +0.011 | +2.6 | within threshold |
| belt/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_present_ms | +0.002 | +2.7 | within threshold |
| belt-sun/window | cpu_submit_and_wait_ms | +0.594 | +17.8 | regression |
| belt-sun/window | gpu_ms | -0.087 | -3.3 | within threshold |
| belt-sun/window | cpu_prepare_ms | +0.709 | +279.7 | regression |
| belt-sun/window | gpu_cull_shadow_ms | +0.015 | +2.7 | within threshold |
| belt-sun/window | gpu_surface_ms | +0.005 | +1.0 | within threshold |
| belt-sun/window | gpu_atmosphere_ms | -0.140 | -15.2 | within threshold |
| belt-sun/window | gpu_post_ms | +0.037 | +6.1 | within threshold |
| belt-sun/window | gpu_cull_ms | +0.009 | +4.8 | within threshold |
| belt-sun/window | gpu_body_shadow_ms | +0.001 | +4.8 | within threshold |
| belt-sun/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| belt-sun/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_sky_ms | +0.004 | +1.8 | within threshold |
| belt-sun/window | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_splats_ms | +0.001 | +1.4 | within threshold |
| belt-sun/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_atmospheres_ms | -0.085 | -65.4 | within threshold |
| belt-sun/window | gpu_belt_dust_ms | -0.056 | -7.9 | within threshold |
| belt-sun/window | gpu_splat_mask_ms | +0.001 | +1.3 | within threshold |
| belt-sun/window | gpu_temporal_ms | +0.009 | +4.8 | within threshold |
| belt-sun/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_bloom_ms | +0.004 | +5.5 | within threshold |
| belt-sun/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_flare_ms | +0.001 | +20.0 | within threshold |
| belt-sun/window | gpu_composite_ms | +0.019 | +12.7 | within threshold |
| belt-sun/window | gpu_spatial_aa_ms | +0.003 | +2.6 | within threshold |
| belt-sun/window | gpu_meter_ms | +0.000 | +2.7 | within threshold |
| belt-sun/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | cpu_submit_and_wait_ms | +0.126 | +1.6 | within threshold |
| belt-sun/fullscreen | gpu_ms | -0.486 | -6.8 | within threshold |
| belt-sun/fullscreen | cpu_prepare_ms | +0.689 | +225.7 | regression |
| belt-sun/fullscreen | gpu_cull_shadow_ms | -0.107 | -15.1 | within threshold |
| belt-sun/fullscreen | gpu_surface_ms | -0.010 | -0.6 | within threshold |
| belt-sun/fullscreen | gpu_atmosphere_ms | -0.449 | -15.8 | within threshold |
| belt-sun/fullscreen | gpu_post_ms | +0.108 | +5.7 | within threshold |
| belt-sun/fullscreen | gpu_cull_ms | +0.016 | +7.9 | within threshold |
| belt-sun/fullscreen | gpu_body_shadow_ms | +0.001 | +3.4 | within threshold |
| belt-sun/fullscreen | gpu_belt_light_ms | -0.122 | -24.9 | within threshold |
| belt-sun/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_sky_ms | +0.010 | +1.2 | within threshold |
| belt-sun/fullscreen | gpu_surface_bodies_ms | -0.001 | -7.1 | within threshold |
| belt-sun/fullscreen | gpu_surface_rocks_ms | -0.008 | -1.7 | within threshold |
| belt-sun/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_atmospheres_ms | -0.285 | -50.5 | within threshold |
| belt-sun/fullscreen | gpu_belt_dust_ms | -0.166 | -7.6 | within threshold |
| belt-sun/fullscreen | gpu_splat_mask_ms | +0.002 | +2.0 | within threshold |
| belt-sun/fullscreen | gpu_temporal_ms | +0.033 | +5.2 | within threshold |
| belt-sun/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_bloom_ms | +0.004 | +1.9 | within threshold |
| belt-sun/fullscreen | gpu_sun_visibility_ms | -0.001 | -3.2 | within threshold |
| belt-sun/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_composite_ms | +0.058 | +11.3 | within threshold |
| belt-sun/fullscreen | gpu_spatial_aa_ms | +0.010 | +2.7 | within threshold |
| belt-sun/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_present_ms | +0.002 | +2.7 | within threshold |
