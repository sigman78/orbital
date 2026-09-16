# Render performance: static-heap

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `fce61cd5529277ac0d1e0da7deb7612222a3387f`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| earth/window | 1600×900 | 3.868 | 3.159 | 3.430 | 0.556 | 0.961 | 0.990 | 0.641 |
| earth/fullscreen | 3440×1440 | 8.826 | 7.978 | 8.770 | 0.689 | 2.563 | 2.695 | 1.987 |
| giant/window | 1600×900 | 4.728 | 3.937 | 4.308 | 0.598 | 0.922 | 1.781 | 0.632 |
| giant/fullscreen | 3440×1440 | 11.701 | 10.855 | 16.172 | 0.734 | 2.566 | 5.536 | 2.008 |
| moon/window | 1600×900 | 2.578 | 1.923 | 2.101 | 0.507 | 0.609 | 0.076 | 0.627 |
| moon/fullscreen | 3440×1440 | 5.559 | 4.814 | 5.047 | 0.638 | 1.848 | 0.353 | 1.979 |
| mars/window | 1600×900 | 2.907 | 2.277 | 2.465 | 0.616 | 0.620 | 0.440 | 0.600 |
| mars/fullscreen | 3440×1440 | 6.609 | 5.807 | 6.107 | 0.669 | 1.847 | 1.334 | 1.913 |
| dawn/window | 1600×900 | 3.605 | 2.868 | 3.297 | 0.505 | 0.753 | 0.927 | 0.679 |
| dawn/fullscreen | 3440×1440 | 8.110 | 7.367 | 7.779 | 0.643 | 2.161 | 2.555 | 2.017 |
| belt/window | 1600×900 | 4.642 | 3.850 | 4.128 | 0.584 | 1.062 | 1.499 | 0.698 |
| belt/fullscreen | 3440×1440 | 11.730 | 10.810 | 17.142 | 0.696 | 3.206 | 4.671 | 2.195 |
| belt-sun/window | 1600×900 | 3.254 | 2.548 | 2.686 | 0.583 | 0.531 | 0.782 | 0.645 |
| belt-sun/fullscreen | 3440×1440 | 7.719 | 6.802 | 7.128 | 0.711 | 1.717 | 2.413 | 1.999 |

## Children

The scopes inside the groups: indicative, since a scope reads where its commands were issued rather than an exact cost.

| Scene / mode | Scope | Median | p95 | Run range |
| --- | --- | ---: | ---: | ---: |
| earth/window | gpu_cull_ms | 0.167 | 0.168 | 0.166 to 0.167 |
| earth/window | gpu_body_shadow_ms | 0.019 | 0.019 | 0.018 to 0.019 |
| earth/window | gpu_belt_light_ms | 0.368 | 0.494 | 0.365 to 0.368 |
| earth/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| earth/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| earth/window | gpu_surface_sky_ms | 0.219 | 0.221 | 0.216 to 0.219 |
| earth/window | gpu_surface_bodies_ms | 0.325 | 0.328 | 0.321 to 0.325 |
| earth/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/window | gpu_surface_splats_ms | 0.005 | 0.008 | 0.005 to 0.005 |
| earth/window | gpu_surface_clouds_ms | 0.176 | 0.178 | 0.174 to 0.176 |
| earth/window | gpu_atmospheres_ms | 0.773 | 0.780 | 0.763 to 0.773 |
| earth/window | gpu_belt_dust_ms | 0.205 | 0.211 | 0.203 to 0.205 |
| earth/window | gpu_splat_mask_ms | 0.008 | 0.014 | 0.008 to 0.008 |
| earth/window | gpu_temporal_ms | 0.205 | 0.207 | 0.203 to 0.205 |
| earth/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| earth/window | gpu_bloom_ms | 0.088 | 0.091 | 0.088 to 0.088 |
| earth/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| earth/window | gpu_composite_ms | 0.172 | 0.173 | 0.171 to 0.172 |
| earth/window | gpu_spatial_aa_ms | 0.134 | 0.136 | 0.133 to 0.134 |
| earth/window | gpu_meter_ms | 0.007 | 0.010 | 0.007 to 0.007 |
| earth/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| earth/fullscreen | gpu_cull_ms | 0.170 | 0.172 | 0.169 to 0.171 |
| earth/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| earth/fullscreen | gpu_belt_light_ms | 0.498 | 0.512 | 0.494 to 0.501 |
| earth/fullscreen | gpu_belt_disc_ms | 0.000 | 0.118 | 0.000 to 0.000 |
| earth/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.128 | 0.001 to 0.001 |
| earth/fullscreen | gpu_surface_sky_ms | 0.931 | 0.950 | 0.923 to 0.934 |
| earth/fullscreen | gpu_surface_bodies_ms | 0.659 | 0.796 | 0.653 to 0.663 |
| earth/fullscreen | gpu_surface_rocks_ms | 0.025 | 0.025 | 0.024 to 0.025 |
| earth/fullscreen | gpu_surface_splats_ms | 0.004 | 0.007 | 0.004 to 0.004 |
| earth/fullscreen | gpu_surface_clouds_ms | 0.379 | 0.390 | 0.375 to 0.381 |
| earth/fullscreen | gpu_atmospheres_ms | 2.094 | 2.332 | 2.080 to 2.106 |
| earth/fullscreen | gpu_belt_dust_ms | 0.587 | 0.718 | 0.580 to 0.588 |
| earth/fullscreen | gpu_splat_mask_ms | 0.008 | 0.009 | 0.008 to 0.008 |
| earth/fullscreen | gpu_temporal_ms | 0.670 | 0.681 | 0.664 to 0.672 |
| earth/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| earth/fullscreen | gpu_bloom_ms | 0.244 | 0.247 | 0.242 to 0.245 |
| earth/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| earth/fullscreen | gpu_composite_ms | 0.573 | 0.711 | 0.569 to 0.575 |
| earth/fullscreen | gpu_spatial_aa_ms | 0.394 | 0.404 | 0.391 to 0.395 |
| earth/fullscreen | gpu_meter_ms | 0.015 | 0.017 | 0.015 to 0.015 |
| earth/fullscreen | gpu_present_ms | 0.076 | 0.077 | 0.076 to 0.077 |
| giant/window | gpu_cull_ms | 0.211 | 0.213 | 0.210 to 0.212 |
| giant/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| giant/window | gpu_belt_light_ms | 0.366 | 0.482 | 0.362 to 0.367 |
| giant/window | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| giant/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| giant/window | gpu_surface_sky_ms | 0.252 | 0.255 | 0.249 to 0.253 |
| giant/window | gpu_surface_bodies_ms | 0.294 | 0.303 | 0.291 to 0.295 |
| giant/window | gpu_surface_rocks_ms | 0.038 | 0.045 | 0.037 to 0.038 |
| giant/window | gpu_surface_splats_ms | 0.104 | 0.109 | 0.103 to 0.104 |
| giant/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/window | gpu_atmospheres_ms | 0.541 | 0.547 | 0.535 to 0.541 |
| giant/window | gpu_belt_dust_ms | 1.117 | 1.132 | 1.104 to 1.118 |
| giant/window | gpu_splat_mask_ms | 0.119 | 0.120 | 0.118 to 0.120 |
| giant/window | gpu_temporal_ms | 0.207 | 0.209 | 0.206 to 0.208 |
| giant/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/window | gpu_bloom_ms | 0.085 | 0.086 | 0.084 to 0.085 |
| giant/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| giant/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| giant/window | gpu_composite_ms | 0.172 | 0.173 | 0.170 to 0.172 |
| giant/window | gpu_spatial_aa_ms | 0.126 | 0.128 | 0.126 to 0.126 |
| giant/window | gpu_meter_ms | 0.008 | 0.010 | 0.007 to 0.008 |
| giant/window | gpu_present_ms | 0.027 | 0.027 | 0.026 to 0.027 |
| giant/fullscreen | gpu_cull_ms | 0.224 | 0.228 | 0.223 to 0.225 |
| giant/fullscreen | gpu_body_shadow_ms | 0.020 | 0.020 | 0.019 to 0.020 |
| giant/fullscreen | gpu_belt_light_ms | 0.486 | 0.501 | 0.483 to 0.487 |
| giant/fullscreen | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| giant/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.117 | 0.001 to 0.001 |
| giant/fullscreen | gpu_surface_sky_ms | 0.982 | 1.376 | 0.977 to 0.986 |
| giant/fullscreen | gpu_surface_bodies_ms | 0.658 | 2.263 | 0.654 to 0.662 |
| giant/fullscreen | gpu_surface_rocks_ms | 0.113 | 0.141 | 0.112 to 0.114 |
| giant/fullscreen | gpu_surface_splats_ms | 0.186 | 0.203 | 0.185 to 0.186 |
| giant/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/fullscreen | gpu_atmospheres_ms | 1.496 | 3.358 | 1.488 to 1.505 |
| giant/fullscreen | gpu_belt_dust_ms | 3.846 | 5.594 | 3.828 to 3.866 |
| giant/fullscreen | gpu_splat_mask_ms | 0.189 | 0.205 | 0.189 to 0.190 |
| giant/fullscreen | gpu_temporal_ms | 0.688 | 0.830 | 0.685 to 0.691 |
| giant/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/fullscreen | gpu_bloom_ms | 0.255 | 0.258 | 0.253 to 0.256 |
| giant/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| giant/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| giant/fullscreen | gpu_composite_ms | 0.577 | 0.730 | 0.574 to 0.579 |
| giant/fullscreen | gpu_spatial_aa_ms | 0.380 | 0.411 | 0.378 to 0.381 |
| giant/fullscreen | gpu_meter_ms | 0.016 | 0.019 | 0.016 to 0.016 |
| giant/fullscreen | gpu_present_ms | 0.078 | 0.079 | 0.077 to 0.078 |
| moon/window | gpu_cull_ms | 0.114 | 0.115 | 0.114 to 0.114 |
| moon/window | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.019 |
| moon/window | gpu_belt_light_ms | 0.367 | 0.492 | 0.366 to 0.492 |
| moon/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| moon/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/window | gpu_surface_sky_ms | 0.228 | 0.230 | 0.227 to 0.229 |
| moon/window | gpu_surface_bodies_ms | 0.133 | 0.134 | 0.133 to 0.134 |
| moon/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_surface_splats_ms | 0.001 | 0.001 | 0.000 to 0.001 |
| moon/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/window | gpu_atmospheres_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/window | gpu_belt_dust_ms | 0.070 | 0.071 | 0.070 to 0.070 |
| moon/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| moon/window | gpu_temporal_ms | 0.200 | 0.201 | 0.200 to 0.200 |
| moon/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/window | gpu_bloom_ms | 0.079 | 0.080 | 0.079 to 0.079 |
| moon/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| moon/window | gpu_composite_ms | 0.171 | 0.172 | 0.171 to 0.172 |
| moon/window | gpu_spatial_aa_ms | 0.135 | 0.141 | 0.134 to 0.135 |
| moon/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| moon/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| moon/fullscreen | gpu_cull_ms | 0.120 | 0.123 | 0.119 to 0.120 |
| moon/fullscreen | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| moon/fullscreen | gpu_belt_light_ms | 0.499 | 0.502 | 0.368 to 0.502 |
| moon/fullscreen | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| moon/fullscreen | gpu_surface_prepass_ms | 0.113 | 0.116 | 0.001 to 0.115 |
| moon/fullscreen | gpu_surface_sky_ms | 0.923 | 0.929 | 0.916 to 0.927 |
| moon/fullscreen | gpu_surface_bodies_ms | 0.266 | 0.269 | 0.265 to 0.267 |
| moon/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/fullscreen | gpu_atmospheres_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/fullscreen | gpu_belt_dust_ms | 0.344 | 0.347 | 0.342 to 0.346 |
| moon/fullscreen | gpu_splat_mask_ms | 0.005 | 0.005 | 0.005 to 0.005 |
| moon/fullscreen | gpu_temporal_ms | 0.657 | 0.663 | 0.654 to 0.659 |
| moon/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/fullscreen | gpu_bloom_ms | 0.245 | 0.247 | 0.243 to 0.246 |
| moon/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| moon/fullscreen | gpu_composite_ms | 0.569 | 0.573 | 0.567 to 0.572 |
| moon/fullscreen | gpu_spatial_aa_ms | 0.408 | 0.413 | 0.406 to 0.409 |
| moon/fullscreen | gpu_meter_ms | 0.015 | 0.017 | 0.014 to 0.015 |
| moon/fullscreen | gpu_present_ms | 0.075 | 0.076 | 0.075 to 0.075 |
| mars/window | gpu_cull_ms | 0.114 | 0.115 | 0.114 to 0.114 |
| mars/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| mars/window | gpu_belt_light_ms | 0.367 | 0.493 | 0.366 to 0.492 |
| mars/window | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| mars/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| mars/window | gpu_surface_sky_ms | 0.257 | 0.258 | 0.255 to 0.257 |
| mars/window | gpu_surface_bodies_ms | 0.120 | 0.122 | 0.120 to 0.121 |
| mars/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/window | gpu_atmospheres_ms | 0.367 | 0.372 | 0.366 to 0.368 |
| mars/window | gpu_belt_dust_ms | 0.070 | 0.070 | 0.069 to 0.070 |
| mars/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| mars/window | gpu_temporal_ms | 0.197 | 0.198 | 0.196 to 0.197 |
| mars/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/window | gpu_bloom_ms | 0.078 | 0.079 | 0.078 to 0.078 |
| mars/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| mars/window | gpu_composite_ms | 0.171 | 0.172 | 0.171 to 0.171 |
| mars/window | gpu_spatial_aa_ms | 0.114 | 0.116 | 0.114 to 0.115 |
| mars/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| mars/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| mars/fullscreen | gpu_cull_ms | 0.153 | 0.155 | 0.153 to 0.154 |
| mars/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| mars/fullscreen | gpu_belt_light_ms | 0.493 | 0.506 | 0.368 to 0.499 |
| mars/fullscreen | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| mars/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.118 | 0.001 to 0.114 |
| mars/fullscreen | gpu_surface_sky_ms | 1.019 | 1.032 | 1.010 to 1.023 |
| mars/fullscreen | gpu_surface_bodies_ms | 0.247 | 0.250 | 0.245 to 0.248 |
| mars/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/fullscreen | gpu_surface_splats_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| mars/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/fullscreen | gpu_atmospheres_ms | 0.949 | 0.972 | 0.943 to 0.957 |
| mars/fullscreen | gpu_belt_dust_ms | 0.374 | 0.386 | 0.371 to 0.379 |
| mars/fullscreen | gpu_splat_mask_ms | 0.006 | 0.007 | 0.006 to 0.006 |
| mars/fullscreen | gpu_temporal_ms | 0.650 | 0.677 | 0.646 to 0.652 |
| mars/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/fullscreen | gpu_bloom_ms | 0.244 | 0.247 | 0.242 to 0.245 |
| mars/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| mars/fullscreen | gpu_composite_ms | 0.572 | 0.594 | 0.568 to 0.574 |
| mars/fullscreen | gpu_spatial_aa_ms | 0.344 | 0.355 | 0.342 to 0.345 |
| mars/fullscreen | gpu_meter_ms | 0.014 | 0.017 | 0.014 to 0.015 |
| mars/fullscreen | gpu_present_ms | 0.075 | 0.076 | 0.075 to 0.076 |
| dawn/window | gpu_cull_ms | 0.114 | 0.115 | 0.114 to 0.114 |
| dawn/window | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| dawn/window | gpu_belt_light_ms | 0.370 | 0.496 | 0.370 to 0.371 |
| dawn/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| dawn/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| dawn/window | gpu_surface_sky_ms | 0.133 | 0.136 | 0.133 to 0.134 |
| dawn/window | gpu_surface_bodies_ms | 0.342 | 0.344 | 0.340 to 0.342 |
| dawn/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/window | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/window | gpu_surface_clouds_ms | 0.198 | 0.199 | 0.197 to 0.198 |
| dawn/window | gpu_atmospheres_ms | 0.853 | 0.860 | 0.849 to 0.855 |
| dawn/window | gpu_belt_dust_ms | 0.069 | 0.069 | 0.068 to 0.069 |
| dawn/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| dawn/window | gpu_temporal_ms | 0.207 | 0.208 | 0.207 to 0.207 |
| dawn/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| dawn/window | gpu_bloom_ms | 0.098 | 0.101 | 0.098 to 0.098 |
| dawn/window | gpu_sun_visibility_ms | 0.028 | 0.028 | 0.028 to 0.028 |
| dawn/window | gpu_flare_ms | 0.009 | 0.010 | 0.009 to 0.009 |
| dawn/window | gpu_composite_ms | 0.178 | 0.179 | 0.178 to 0.178 |
| dawn/window | gpu_spatial_aa_ms | 0.121 | 0.122 | 0.120 to 0.121 |
| dawn/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| dawn/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| dawn/fullscreen | gpu_cull_ms | 0.115 | 0.116 | 0.114 to 0.115 |
| dawn/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| dawn/fullscreen | gpu_belt_light_ms | 0.507 | 0.516 | 0.496 to 0.508 |
| dawn/fullscreen | gpu_belt_disc_ms | 0.000 | 0.118 | 0.000 to 0.000 |
| dawn/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_sky_ms | 0.635 | 0.658 | 0.634 to 0.639 |
| dawn/fullscreen | gpu_surface_bodies_ms | 0.797 | 0.880 | 0.712 to 0.861 |
| dawn/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_clouds_ms | 0.461 | 0.470 | 0.453 to 0.462 |
| dawn/fullscreen | gpu_atmospheres_ms | 2.316 | 2.476 | 2.294 to 2.318 |
| dawn/fullscreen | gpu_belt_dust_ms | 0.230 | 0.239 | 0.230 to 0.232 |
| dawn/fullscreen | gpu_splat_mask_ms | 0.005 | 0.005 | 0.005 to 0.005 |
| dawn/fullscreen | gpu_temporal_ms | 0.680 | 0.696 | 0.678 to 0.682 |
| dawn/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| dawn/fullscreen | gpu_bloom_ms | 0.227 | 0.232 | 0.226 to 0.227 |
| dawn/fullscreen | gpu_sun_visibility_ms | 0.030 | 0.031 | 0.030 to 0.030 |
| dawn/fullscreen | gpu_flare_ms | 0.024 | 0.025 | 0.024 to 0.024 |
| dawn/fullscreen | gpu_composite_ms | 0.595 | 0.610 | 0.593 to 0.596 |
| dawn/fullscreen | gpu_spatial_aa_ms | 0.367 | 0.375 | 0.365 to 0.367 |
| dawn/fullscreen | gpu_meter_ms | 0.016 | 0.018 | 0.015 to 0.016 |
| dawn/fullscreen | gpu_present_ms | 0.075 | 0.077 | 0.075 to 0.075 |
| belt/window | gpu_cull_ms | 0.196 | 0.198 | 0.196 to 0.197 |
| belt/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| belt/window | gpu_belt_light_ms | 0.366 | 0.503 | 0.365 to 0.367 |
| belt/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt/window | gpu_surface_sky_ms | 0.110 | 0.112 | 0.109 to 0.110 |
| belt/window | gpu_surface_bodies_ms | 0.438 | 0.444 | 0.436 to 0.439 |
| belt/window | gpu_surface_rocks_ms | 0.345 | 0.464 | 0.344 to 0.346 |
| belt/window | gpu_surface_splats_ms | 0.049 | 0.051 | 0.048 to 0.049 |
| belt/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_atmospheres_ms | 0.836 | 0.842 | 0.831 to 0.837 |
| belt/window | gpu_belt_dust_ms | 0.600 | 0.605 | 0.597 to 0.600 |
| belt/window | gpu_splat_mask_ms | 0.060 | 0.061 | 0.060 to 0.060 |
| belt/window | gpu_temporal_ms | 0.214 | 0.215 | 0.214 to 0.215 |
| belt/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_bloom_ms | 0.088 | 0.090 | 0.088 to 0.088 |
| belt/window | gpu_sun_visibility_ms | 0.028 | 0.029 | 0.028 to 0.028 |
| belt/window | gpu_flare_ms | 0.009 | 0.010 | 0.009 to 0.009 |
| belt/window | gpu_composite_ms | 0.178 | 0.179 | 0.177 to 0.178 |
| belt/window | gpu_spatial_aa_ms | 0.142 | 0.144 | 0.142 to 0.142 |
| belt/window | gpu_meter_ms | 0.008 | 0.010 | 0.008 to 0.008 |
| belt/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.027 |
| belt/fullscreen | gpu_cull_ms | 0.209 | 0.212 | 0.208 to 0.209 |
| belt/fullscreen | gpu_body_shadow_ms | 0.020 | 0.021 | 0.020 to 0.020 |
| belt/fullscreen | gpu_belt_light_ms | 0.466 | 0.504 | 0.371 to 0.490 |
| belt/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.104 | 0.001 to 0.002 |
| belt/fullscreen | gpu_surface_sky_ms | 0.509 | 0.688 | 0.507 to 0.510 |
| belt/fullscreen | gpu_surface_bodies_ms | 1.020 | 3.267 | 1.012 to 1.021 |
| belt/fullscreen | gpu_surface_rocks_ms | 1.291 | 1.791 | 1.286 to 1.293 |
| belt/fullscreen | gpu_surface_splats_ms | 0.069 | 0.079 | 0.069 to 0.070 |
| belt/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_atmospheres_ms | 2.583 | 5.592 | 2.557 to 2.589 |
| belt/fullscreen | gpu_belt_dust_ms | 1.983 | 2.978 | 1.963 to 1.987 |
| belt/fullscreen | gpu_splat_mask_ms | 0.086 | 0.088 | 0.086 to 0.086 |
| belt/fullscreen | gpu_temporal_ms | 0.712 | 0.872 | 0.710 to 0.715 |
| belt/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_bloom_ms | 0.276 | 0.283 | 0.274 to 0.278 |
| belt/fullscreen | gpu_sun_visibility_ms | 0.030 | 0.031 | 0.030 to 0.030 |
| belt/fullscreen | gpu_flare_ms | 0.024 | 0.024 | 0.024 to 0.024 |
| belt/fullscreen | gpu_composite_ms | 0.597 | 0.843 | 0.597 to 0.598 |
| belt/fullscreen | gpu_spatial_aa_ms | 0.448 | 0.495 | 0.446 to 0.449 |
| belt/fullscreen | gpu_meter_ms | 0.018 | 0.020 | 0.018 to 0.018 |
| belt/fullscreen | gpu_present_ms | 0.078 | 0.079 | 0.078 to 0.078 |
| belt-sun/window | gpu_cull_ms | 0.199 | 0.201 | 0.198 to 0.199 |
| belt-sun/window | gpu_body_shadow_ms | 0.019 | 0.021 | 0.019 to 0.020 |
| belt-sun/window | gpu_belt_light_ms | 0.362 | 0.495 | 0.361 to 0.364 |
| belt-sun/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt-sun/window | gpu_surface_sky_ms | 0.231 | 0.234 | 0.230 to 0.231 |
| belt-sun/window | gpu_surface_bodies_ms | 0.002 | 0.006 | 0.001 to 0.002 |
| belt-sun/window | gpu_surface_rocks_ms | 0.143 | 0.147 | 0.143 to 0.143 |
| belt-sun/window | gpu_surface_splats_ms | 0.073 | 0.075 | 0.073 to 0.073 |
| belt-sun/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_atmospheres_ms | 0.045 | 0.045 | 0.045 to 0.045 |
| belt-sun/window | gpu_belt_dust_ms | 0.656 | 0.664 | 0.653 to 0.657 |
| belt-sun/window | gpu_splat_mask_ms | 0.079 | 0.079 | 0.078 to 0.079 |
| belt-sun/window | gpu_temporal_ms | 0.201 | 0.202 | 0.201 to 0.202 |
| belt-sun/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_bloom_ms | 0.079 | 0.081 | 0.079 to 0.079 |
| belt-sun/window | gpu_sun_visibility_ms | 0.028 | 0.029 | 0.028 to 0.028 |
| belt-sun/window | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| belt-sun/window | gpu_composite_ms | 0.173 | 0.174 | 0.172 to 0.173 |
| belt-sun/window | gpu_spatial_aa_ms | 0.120 | 0.122 | 0.120 to 0.120 |
| belt-sun/window | gpu_meter_ms | 0.008 | 0.012 | 0.008 to 0.008 |
| belt-sun/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| belt-sun/fullscreen | gpu_cull_ms | 0.207 | 0.210 | 0.207 to 0.207 |
| belt-sun/fullscreen | gpu_body_shadow_ms | 0.020 | 0.021 | 0.020 to 0.020 |
| belt-sun/fullscreen | gpu_belt_light_ms | 0.483 | 0.501 | 0.373 to 0.497 |
| belt-sun/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt-sun/fullscreen | gpu_surface_sky_ms | 0.886 | 0.896 | 0.882 to 0.888 |
| belt-sun/fullscreen | gpu_surface_bodies_ms | 0.013 | 0.020 | 0.013 to 0.013 |
| belt-sun/fullscreen | gpu_surface_rocks_ms | 0.482 | 0.607 | 0.479 to 0.598 |
| belt-sun/fullscreen | gpu_surface_splats_ms | 0.113 | 0.117 | 0.113 to 0.114 |
| belt-sun/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_atmospheres_ms | 0.282 | 0.286 | 0.281 to 0.283 |
| belt-sun/fullscreen | gpu_belt_dust_ms | 2.021 | 2.073 | 2.015 to 2.032 |
| belt-sun/fullscreen | gpu_splat_mask_ms | 0.105 | 0.111 | 0.105 to 0.105 |
| belt-sun/fullscreen | gpu_temporal_ms | 0.662 | 0.674 | 0.659 to 0.664 |
| belt-sun/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_bloom_ms | 0.221 | 0.223 | 0.219 to 0.221 |
| belt-sun/fullscreen | gpu_sun_visibility_ms | 0.032 | 0.033 | 0.032 to 0.032 |
| belt-sun/fullscreen | gpu_flare_ms | 0.015 | 0.015 | 0.014 to 0.015 |
| belt-sun/fullscreen | gpu_composite_ms | 0.578 | 0.585 | 0.577 to 0.581 |
| belt-sun/fullscreen | gpu_spatial_aa_ms | 0.393 | 0.400 | 0.391 to 0.394 |
| belt-sun/fullscreen | gpu_meter_ms | 0.016 | 0.019 | 0.016 to 0.017 |
| belt-sun/fullscreen | gpu_present_ms | 0.077 | 0.078 | 0.077 to 0.077 |

## Compared with headless-baseline

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| earth/window | cpu_submit_and_wait_ms | -0.010 | -0.2 | within threshold |
| earth/window | gpu_ms | +0.041 | +1.3 | within threshold |
| earth/window | cpu_prepare_ms | +0.000 | +0.2 | within threshold |
| earth/window | gpu_cull_shadow_ms | +0.016 | +2.9 | within threshold |
| earth/window | gpu_surface_ms | +0.008 | +0.8 | within threshold |
| earth/window | gpu_atmosphere_ms | -0.030 | -2.9 | within threshold |
| earth/window | gpu_post_ms | +0.039 | +6.5 | within threshold |
| earth/window | gpu_cull_ms | +0.008 | +4.8 | within threshold |
| earth/window | gpu_body_shadow_ms | +0.001 | +6.0 | within threshold |
| earth/window | gpu_belt_light_ms | +0.006 | +1.7 | within threshold |
| earth/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_sky_ms | +0.009 | +4.1 | within threshold |
| earth/window | gpu_surface_bodies_ms | -0.007 | -2.2 | within threshold |
| earth/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_splats_ms | +0.001 | +25.0 | within threshold |
| earth/window | gpu_surface_clouds_ms | +0.004 | +2.4 | within threshold |
| earth/window | gpu_atmospheres_ms | -0.028 | -3.5 | within threshold |
| earth/window | gpu_belt_dust_ms | +0.001 | +0.5 | within threshold |
| earth/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_temporal_ms | +0.010 | +5.3 | within threshold |
| earth/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_bloom_ms | +0.005 | +6.2 | within threshold |
| earth/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_composite_ms | +0.019 | +12.8 | within threshold |
| earth/window | gpu_spatial_aa_ms | +0.004 | +3.1 | within threshold |
| earth/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | cpu_submit_and_wait_ms | -0.037 | -0.4 | within threshold |
| earth/fullscreen | gpu_ms | -0.029 | -0.4 | within threshold |
| earth/fullscreen | cpu_prepare_ms | +0.017 | +5.6 | within threshold |
| earth/fullscreen | gpu_cull_shadow_ms | +0.018 | +2.6 | within threshold |
| earth/fullscreen | gpu_surface_ms | +0.033 | +1.3 | within threshold |
| earth/fullscreen | gpu_atmosphere_ms | -0.249 | -8.5 | within threshold |
| earth/fullscreen | gpu_post_ms | +0.122 | +6.5 | within threshold |
| earth/fullscreen | gpu_cull_ms | +0.008 | +5.0 | within threshold |
| earth/fullscreen | gpu_body_shadow_ms | +0.001 | +6.5 | within threshold |
| earth/fullscreen | gpu_belt_light_ms | +0.011 | +2.3 | within threshold |
| earth/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_surface_prepass_ms | -0.001 | -33.3 | within threshold |
| earth/fullscreen | gpu_surface_sky_ms | +0.026 | +2.8 | within threshold |
| earth/fullscreen | gpu_surface_bodies_ms | -0.015 | -2.3 | within threshold |
| earth/fullscreen | gpu_surface_rocks_ms | +0.001 | +4.3 | within threshold |
| earth/fullscreen | gpu_surface_splats_ms | +0.001 | +33.3 | within threshold |
| earth/fullscreen | gpu_surface_clouds_ms | +0.009 | +2.5 | within threshold |
| earth/fullscreen | gpu_atmospheres_ms | -0.264 | -11.2 | within threshold |
| earth/fullscreen | gpu_belt_dust_ms | +0.015 | +2.7 | within threshold |
| earth/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_temporal_ms | +0.036 | +5.7 | within threshold |
| earth/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_bloom_ms | +0.004 | +1.7 | within threshold |
| earth/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_flare_ms | +0.001 | +20.0 | within threshold |
| earth/fullscreen | gpu_composite_ms | +0.063 | +12.4 | within threshold |
| earth/fullscreen | gpu_spatial_aa_ms | +0.013 | +3.5 | within threshold |
| earth/fullscreen | gpu_meter_ms | +0.001 | +7.1 | within threshold |
| earth/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| giant/window | cpu_submit_and_wait_ms | -0.031 | -0.6 | within threshold |
| giant/window | gpu_ms | -0.040 | -1.0 | within threshold |
| giant/window | cpu_prepare_ms | +0.074 | +29.1 | within threshold |
| giant/window | gpu_cull_shadow_ms | +0.015 | +2.6 | within threshold |
| giant/window | gpu_surface_ms | -0.005 | -0.6 | within threshold |
| giant/window | gpu_atmosphere_ms | -0.084 | -4.5 | within threshold |
| giant/window | gpu_post_ms | +0.036 | +6.0 | within threshold |
| giant/window | gpu_cull_ms | +0.008 | +3.9 | within threshold |
| giant/window | gpu_body_shadow_ms | +0.001 | +5.0 | within threshold |
| giant/window | gpu_belt_light_ms | +0.006 | +1.7 | within threshold |
| giant/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_surface_sky_ms | +0.009 | +3.8 | within threshold |
| giant/window | gpu_surface_bodies_ms | -0.016 | -5.3 | within threshold |
| giant/window | gpu_surface_rocks_ms | -0.001 | -2.6 | within threshold |
| giant/window | gpu_surface_splats_ms | +0.001 | +1.0 | within threshold |
| giant/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_atmospheres_ms | +0.001 | +0.2 | within threshold |
| giant/window | gpu_belt_dust_ms | -0.086 | -7.1 | within threshold |
| giant/window | gpu_splat_mask_ms | +0.001 | +0.9 | within threshold |
| giant/window | gpu_temporal_ms | +0.008 | +4.1 | within threshold |
| giant/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_bloom_ms | +0.004 | +5.1 | within threshold |
| giant/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_composite_ms | +0.019 | +12.8 | within threshold |
| giant/window | gpu_spatial_aa_ms | +0.004 | +3.4 | within threshold |
| giant/window | gpu_meter_ms | +0.001 | +14.3 | within threshold |
| giant/window | gpu_present_ms | +0.001 | +3.9 | within threshold |
| giant/fullscreen | cpu_submit_and_wait_ms | -0.258 | -2.2 | within threshold |
| giant/fullscreen | gpu_ms | -0.176 | -1.6 | within threshold |
| giant/fullscreen | cpu_prepare_ms | -0.067 | -17.8 | within threshold |
| giant/fullscreen | gpu_cull_shadow_ms | +0.033 | +4.7 | within threshold |
| giant/fullscreen | gpu_surface_ms | +0.137 | +5.6 | within threshold |
| giant/fullscreen | gpu_atmosphere_ms | -0.429 | -7.2 | within threshold |
| giant/fullscreen | gpu_post_ms | +0.120 | +6.3 | within threshold |
| giant/fullscreen | gpu_cull_ms | +0.009 | +4.1 | within threshold |
| giant/fullscreen | gpu_body_shadow_ms | +0.001 | +5.8 | within threshold |
| giant/fullscreen | gpu_belt_light_ms | +0.115 | +31.0 | within threshold |
| giant/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_surface_sky_ms | +0.028 | +2.9 | within threshold |
| giant/fullscreen | gpu_surface_bodies_ms | +0.042 | +6.8 | within threshold |
| giant/fullscreen | gpu_surface_rocks_ms | -0.001 | -0.9 | within threshold |
| giant/fullscreen | gpu_surface_splats_ms | +0.004 | +2.2 | within threshold |
| giant/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_atmospheres_ms | -0.147 | -8.9 | within threshold |
| giant/fullscreen | gpu_belt_dust_ms | -0.284 | -6.9 | within threshold |
| giant/fullscreen | gpu_splat_mask_ms | +0.002 | +1.1 | within threshold |
| giant/fullscreen | gpu_temporal_ms | +0.032 | +4.8 | within threshold |
| giant/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_bloom_ms | +0.006 | +2.5 | within threshold |
| giant/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_flare_ms | +0.001 | +9.1 | within threshold |
| giant/fullscreen | gpu_composite_ms | +0.066 | +12.8 | within threshold |
| giant/fullscreen | gpu_spatial_aa_ms | +0.014 | +3.9 | within threshold |
| giant/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_present_ms | +0.003 | +4.1 | within threshold |
| moon/window | cpu_submit_and_wait_ms | +0.081 | +3.3 | within threshold |
| moon/window | gpu_ms | +0.063 | +3.4 | within threshold |
| moon/window | cpu_prepare_ms | +0.004 | +1.5 | within threshold |
| moon/window | gpu_cull_shadow_ms | -0.101 | -16.7 | within threshold |
| moon/window | gpu_surface_ms | +0.007 | +1.2 | within threshold |
| moon/window | gpu_atmosphere_ms | +0.016 | +27.6 | within threshold |
| moon/window | gpu_post_ms | +0.038 | +6.4 | within threshold |
| moon/window | gpu_cull_ms | +0.007 | +7.0 | within threshold |
| moon/window | gpu_body_shadow_ms | +0.001 | +5.8 | within threshold |
| moon/window | gpu_belt_light_ms | -0.058 | -13.6 | within threshold |
| moon/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_sky_ms | +0.009 | +4.2 | within threshold |
| moon/window | gpu_surface_bodies_ms | -0.005 | -3.7 | within threshold |
| moon/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
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
| moon/fullscreen | cpu_submit_and_wait_ms | +0.206 | +3.9 | within threshold |
| moon/fullscreen | gpu_ms | +0.243 | +5.3 | noisy increase |
| moon/fullscreen | cpu_prepare_ms | -0.083 | -23.3 | within threshold |
| moon/fullscreen | gpu_cull_shadow_ms | +0.017 | +2.8 | within threshold |
| moon/fullscreen | gpu_surface_ms | +0.089 | +5.1 | within threshold |
| moon/fullscreen | gpu_atmosphere_ms | +0.048 | +15.8 | within threshold |
| moon/fullscreen | gpu_post_ms | +0.113 | +6.1 | within threshold |
| moon/fullscreen | gpu_cull_ms | +0.008 | +7.3 | within threshold |
| moon/fullscreen | gpu_body_shadow_ms | +0.001 | +6.0 | within threshold |
| moon/fullscreen | gpu_belt_light_ms | +0.010 | +2.1 | within threshold |
| moon/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_surface_prepass_ms | +0.112 | +10900.0 | within threshold |
| moon/fullscreen | gpu_surface_sky_ms | +0.018 | +2.0 | within threshold |
| moon/fullscreen | gpu_surface_bodies_ms | -0.001 | -0.4 | within threshold |
| moon/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_belt_dust_ms | +0.048 | +16.3 | within threshold |
| moon/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_temporal_ms | +0.034 | +5.4 | within threshold |
| moon/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_bloom_ms | +0.004 | +1.7 | within threshold |
| moon/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_composite_ms | +0.060 | +11.9 | within threshold |
| moon/fullscreen | gpu_spatial_aa_ms | +0.013 | +3.4 | within threshold |
| moon/fullscreen | gpu_meter_ms | +0.000 | +2.1 | within threshold |
| moon/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| mars/window | cpu_submit_and_wait_ms | +0.079 | +2.8 | within threshold |
| mars/window | gpu_ms | +0.160 | +7.6 | within threshold |
| mars/window | cpu_prepare_ms | +0.002 | +0.8 | within threshold |
| mars/window | gpu_cull_shadow_ms | +0.129 | +26.5 | within threshold |
| mars/window | gpu_surface_ms | +0.009 | +1.5 | within threshold |
| mars/window | gpu_atmosphere_ms | +0.009 | +2.1 | within threshold |
| mars/window | gpu_post_ms | +0.037 | +6.5 | within threshold |
| mars/window | gpu_cull_ms | +0.008 | +7.1 | within threshold |
| mars/window | gpu_body_shadow_ms | +0.001 | +4.9 | within threshold |
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
| mars/window | gpu_bloom_ms | +0.003 | +4.1 | within threshold |
| mars/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_composite_ms | +0.019 | +12.8 | within threshold |
| mars/window | gpu_spatial_aa_ms | +0.004 | +3.2 | within threshold |
| mars/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| mars/fullscreen | cpu_submit_and_wait_ms | +0.036 | +0.6 | within threshold |
| mars/fullscreen | gpu_ms | +0.125 | +2.2 | within threshold |
| mars/fullscreen | cpu_prepare_ms | -0.008 | -2.2 | within threshold |
| mars/fullscreen | gpu_cull_shadow_ms | +0.021 | +3.3 | within threshold |
| mars/fullscreen | gpu_surface_ms | +0.053 | +3.0 | within threshold |
| mars/fullscreen | gpu_atmosphere_ms | -0.138 | -9.4 | within threshold |
| mars/fullscreen | gpu_post_ms | +0.125 | +7.0 | within threshold |
| mars/fullscreen | gpu_cull_ms | +0.009 | +5.9 | within threshold |
| mars/fullscreen | gpu_body_shadow_ms | +0.001 | +4.7 | within threshold |
| mars/fullscreen | gpu_belt_light_ms | +0.126 | +34.4 | within threshold |
| mars/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_sky_ms | +0.027 | +2.7 | within threshold |
| mars/fullscreen | gpu_surface_bodies_ms | +0.007 | +3.0 | within threshold |
| mars/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_atmospheres_ms | -0.183 | -16.2 | within threshold |
| mars/fullscreen | gpu_belt_dust_ms | +0.043 | +13.0 | within threshold |
| mars/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_temporal_ms | +0.039 | +6.4 | within threshold |
| mars/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_bloom_ms | +0.004 | +1.7 | within threshold |
| mars/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_flare_ms | +0.001 | +9.1 | within threshold |
| mars/fullscreen | gpu_composite_ms | +0.065 | +12.7 | within threshold |
| mars/fullscreen | gpu_spatial_aa_ms | +0.012 | +3.7 | within threshold |
| mars/fullscreen | gpu_meter_ms | +0.000 | +0.1 | within threshold |
| mars/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| dawn/window | cpu_submit_and_wait_ms | +0.082 | +2.3 | within threshold |
| dawn/window | gpu_ms | +0.058 | +2.0 | within threshold |
| dawn/window | cpu_prepare_ms | +0.004 | +1.5 | within threshold |
| dawn/window | gpu_cull_shadow_ms | +0.014 | +2.8 | within threshold |
| dawn/window | gpu_surface_ms | +0.001 | +0.1 | within threshold |
| dawn/window | gpu_atmosphere_ms | +0.004 | +0.4 | within threshold |
| dawn/window | gpu_post_ms | +0.041 | +6.4 | within threshold |
| dawn/window | gpu_cull_ms | +0.007 | +7.0 | within threshold |
| dawn/window | gpu_body_shadow_ms | +0.001 | +5.7 | within threshold |
| dawn/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| dawn/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_sky_ms | +0.002 | +1.6 | within threshold |
| dawn/window | gpu_surface_bodies_ms | -0.005 | -1.5 | within threshold |
| dawn/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_clouds_ms | +0.005 | +2.7 | within threshold |
| dawn/window | gpu_atmospheres_ms | -0.012 | -1.4 | within threshold |
| dawn/window | gpu_belt_dust_ms | +0.016 | +31.4 | within threshold |
| dawn/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_temporal_ms | +0.009 | +4.7 | within threshold |
| dawn/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_bloom_ms | +0.007 | +7.9 | within threshold |
| dawn/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_composite_ms | +0.019 | +12.3 | within threshold |
| dawn/window | gpu_spatial_aa_ms | +0.004 | +3.5 | within threshold |
| dawn/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| dawn/fullscreen | cpu_submit_and_wait_ms | -0.116 | -1.4 | within threshold |
| dawn/fullscreen | gpu_ms | +0.054 | +0.7 | within threshold |
| dawn/fullscreen | cpu_prepare_ms | -0.091 | -24.2 | within threshold |
| dawn/fullscreen | gpu_cull_shadow_ms | +0.027 | +4.4 | within threshold |
| dawn/fullscreen | gpu_surface_ms | +0.157 | +7.8 | within threshold |
| dawn/fullscreen | gpu_atmosphere_ms | -0.236 | -8.4 | within threshold |
| dawn/fullscreen | gpu_post_ms | +0.123 | +6.5 | within threshold |
| dawn/fullscreen | gpu_cull_ms | +0.008 | +7.3 | within threshold |
| dawn/fullscreen | gpu_body_shadow_ms | +0.001 | +6.1 | within threshold |
| dawn/fullscreen | gpu_belt_light_ms | +0.134 | +36.0 | within threshold |
| dawn/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_sky_ms | +0.013 | +2.1 | within threshold |
| dawn/fullscreen | gpu_surface_bodies_ms | +0.073 | +10.0 | within threshold |
| dawn/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_clouds_ms | +0.015 | +3.3 | within threshold |
| dawn/fullscreen | gpu_atmospheres_ms | -0.292 | -11.2 | within threshold |
| dawn/fullscreen | gpu_belt_dust_ms | +0.057 | +33.1 | within threshold |
| dawn/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_temporal_ms | +0.033 | +5.1 | within threshold |
| dawn/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_bloom_ms | +0.009 | +4.2 | within threshold |
| dawn/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_flare_ms | +0.001 | +4.5 | within threshold |
| dawn/fullscreen | gpu_composite_ms | +0.066 | +12.4 | within threshold |
| dawn/fullscreen | gpu_spatial_aa_ms | +0.013 | +3.8 | within threshold |
| dawn/fullscreen | gpu_meter_ms | +0.001 | +4.6 | within threshold |
| dawn/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| belt/window | cpu_submit_and_wait_ms | +0.062 | +1.4 | within threshold |
| belt/window | gpu_ms | +0.060 | +1.6 | within threshold |
| belt/window | cpu_prepare_ms | +0.073 | +28.1 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.014 | +2.5 | within threshold |
| belt/window | gpu_surface_ms | +0.033 | +3.2 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.019 | -1.2 | within threshold |
| belt/window | gpu_post_ms | +0.031 | +4.6 | within threshold |
| belt/window | gpu_cull_ms | +0.008 | +4.1 | within threshold |
| belt/window | gpu_body_shadow_ms | +0.001 | +5.7 | within threshold |
| belt/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| belt/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_sky_ms | +0.004 | +3.9 | within threshold |
| belt/window | gpu_surface_bodies_ms | +0.031 | +7.5 | within threshold |
| belt/window | gpu_surface_rocks_ms | -0.003 | -0.9 | within threshold |
| belt/window | gpu_surface_splats_ms | +0.001 | +2.1 | within threshold |
| belt/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_atmospheres_ms | +0.031 | +3.8 | within threshold |
| belt/window | gpu_belt_dust_ms | -0.050 | -7.7 | within threshold |
| belt/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_temporal_ms | +0.007 | +3.5 | within threshold |
| belt/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_bloom_ms | +0.004 | +4.9 | within threshold |
| belt/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_composite_ms | +0.019 | +12.3 | within threshold |
| belt/window | gpu_spatial_aa_ms | +0.004 | +2.6 | within threshold |
| belt/window | gpu_meter_ms | +0.001 | +14.3 | within threshold |
| belt/window | gpu_present_ms | +0.001 | +2.8 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.305 | +2.7 | within threshold |
| belt/fullscreen | gpu_ms | +0.376 | +3.6 | within threshold |
| belt/fullscreen | cpu_prepare_ms | -0.062 | -15.2 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.112 | +19.1 | within threshold |
| belt/fullscreen | gpu_surface_ms | +0.167 | +5.5 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | -0.030 | -0.6 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.120 | +5.8 | within threshold |
| belt/fullscreen | gpu_cull_ms | +0.008 | +4.0 | within threshold |
| belt/fullscreen | gpu_body_shadow_ms | +0.001 | +5.9 | within threshold |
| belt/fullscreen | gpu_belt_light_ms | +0.104 | +28.7 | within threshold |
| belt/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_sky_ms | +0.010 | +2.1 | within threshold |
| belt/fullscreen | gpu_surface_bodies_ms | +0.110 | +12.0 | within threshold |
| belt/fullscreen | gpu_surface_rocks_ms | -0.001 | -0.1 | within threshold |
| belt/fullscreen | gpu_surface_splats_ms | +0.002 | +3.1 | within threshold |
| belt/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_atmospheres_ms | +0.123 | +5.0 | within threshold |
| belt/fullscreen | gpu_belt_dust_ms | -0.168 | -7.8 | within threshold |
| belt/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_temporal_ms | +0.025 | +3.6 | within threshold |
| belt/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_bloom_ms | +0.004 | +1.3 | within threshold |
| belt/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_flare_ms | +0.001 | +4.5 | within threshold |
| belt/fullscreen | gpu_composite_ms | +0.066 | +12.3 | within threshold |
| belt/fullscreen | gpu_spatial_aa_ms | +0.015 | +3.4 | within threshold |
| belt/fullscreen | gpu_meter_ms | +0.001 | +5.9 | within threshold |
| belt/fullscreen | gpu_present_ms | +0.002 | +2.7 | within threshold |
| belt-sun/window | cpu_submit_and_wait_ms | -0.074 | -2.2 | within threshold |
| belt-sun/window | gpu_ms | -0.081 | -3.1 | within threshold |
| belt-sun/window | cpu_prepare_ms | +0.002 | +0.8 | within threshold |
| belt-sun/window | gpu_cull_shadow_ms | +0.016 | +2.8 | within threshold |
| belt-sun/window | gpu_surface_ms | +0.006 | +1.2 | within threshold |
| belt-sun/window | gpu_atmosphere_ms | -0.138 | -15.0 | within threshold |
| belt-sun/window | gpu_post_ms | +0.037 | +6.1 | within threshold |
| belt-sun/window | gpu_cull_ms | +0.008 | +4.5 | within threshold |
| belt-sun/window | gpu_body_shadow_ms | +0.001 | +5.0 | within threshold |
| belt-sun/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| belt-sun/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_sky_ms | +0.004 | +1.8 | within threshold |
| belt-sun/window | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_splats_ms | +0.001 | +1.4 | within threshold |
| belt-sun/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_atmospheres_ms | -0.085 | -65.4 | within threshold |
| belt-sun/window | gpu_belt_dust_ms | -0.054 | -7.6 | within threshold |
| belt-sun/window | gpu_splat_mask_ms | +0.001 | +1.3 | within threshold |
| belt-sun/window | gpu_temporal_ms | +0.009 | +4.8 | within threshold |
| belt-sun/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_bloom_ms | +0.004 | +5.5 | within threshold |
| belt-sun/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_flare_ms | +0.001 | +20.0 | within threshold |
| belt-sun/window | gpu_composite_ms | +0.019 | +12.7 | within threshold |
| belt-sun/window | gpu_spatial_aa_ms | +0.003 | +2.6 | within threshold |
| belt-sun/window | gpu_meter_ms | +0.001 | +14.3 | within threshold |
| belt-sun/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | cpu_submit_and_wait_ms | -0.259 | -3.2 | within threshold |
| belt-sun/fullscreen | gpu_ms | -0.370 | -5.2 | within threshold |
| belt-sun/fullscreen | cpu_prepare_ms | +0.029 | +9.7 | within threshold |
| belt-sun/fullscreen | gpu_cull_shadow_ms | -0.001 | -0.1 | within threshold |
| belt-sun/fullscreen | gpu_surface_ms | +0.008 | +0.5 | within threshold |
| belt-sun/fullscreen | gpu_atmosphere_ms | -0.433 | -15.2 | within threshold |
| belt-sun/fullscreen | gpu_post_ms | +0.121 | +6.4 | within threshold |
| belt-sun/fullscreen | gpu_cull_ms | +0.009 | +4.5 | within threshold |
| belt-sun/fullscreen | gpu_body_shadow_ms | +0.001 | +5.7 | within threshold |
| belt-sun/fullscreen | gpu_belt_light_ms | -0.009 | -1.9 | within threshold |
| belt-sun/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_sky_ms | +0.014 | +1.6 | within threshold |
| belt-sun/fullscreen | gpu_surface_bodies_ms | -0.001 | -7.1 | within threshold |
| belt-sun/fullscreen | gpu_surface_rocks_ms | -0.002 | -0.4 | within threshold |
| belt-sun/fullscreen | gpu_surface_splats_ms | +0.001 | +0.9 | within threshold |
| belt-sun/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_atmospheres_ms | -0.283 | -50.1 | within threshold |
| belt-sun/fullscreen | gpu_belt_dust_ms | -0.154 | -7.1 | within threshold |
| belt-sun/fullscreen | gpu_splat_mask_ms | +0.002 | +2.0 | within threshold |
| belt-sun/fullscreen | gpu_temporal_ms | +0.036 | +5.7 | within threshold |
| belt-sun/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_bloom_ms | +0.006 | +2.9 | within threshold |
| belt-sun/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_flare_ms | +0.001 | +7.1 | within threshold |
| belt-sun/fullscreen | gpu_composite_ms | +0.062 | +12.1 | within threshold |
| belt-sun/fullscreen | gpu_spatial_aa_ms | +0.013 | +3.5 | within threshold |
| belt-sun/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_present_ms | +0.002 | +2.7 | within threshold |
