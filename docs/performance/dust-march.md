# Render performance: dust-march

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `5b3b50dcee8622af3824af269bef779e47a3a5ee`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| earth/window | 1600×900 | 3.940 | 3.136 | 3.844 | 0.589 | 1.009 | 0.884 | 0.646 |
| earth/fullscreen | 3440×1440 | 8.800 | 7.917 | 8.833 | 0.723 | 2.712 | 2.433 | 2.014 |
| giant/window | 1600×900 | 4.197 | 3.375 | 3.911 | 0.642 | 0.975 | 1.114 | 0.640 |
| giant/fullscreen | 3440×1440 | 9.607 | 8.804 | 9.850 | 0.788 | 2.692 | 3.241 | 2.041 |
| moon/window | 1600×900 | 2.594 | 1.884 | 2.333 | 0.518 | 0.658 | 0.072 | 0.633 |
| moon/fullscreen | 3440×1440 | 5.631 | 4.939 | 5.519 | 0.656 | 1.949 | 0.256 | 2.015 |
| mars/window | 1600×900 | 2.957 | 2.247 | 2.690 | 0.518 | 0.673 | 0.437 | 0.607 |
| mars/fullscreen | 3440×1440 | 6.664 | 5.920 | 6.540 | 0.698 | 2.009 | 1.215 | 1.942 |
| dawn/window | 1600×900 | 3.646 | 2.939 | 3.251 | 0.521 | 0.804 | 0.924 | 0.685 |
| dawn/fullscreen | 3440×1440 | 8.251 | 7.493 | 8.226 | 0.651 | 2.183 | 2.536 | 2.042 |
| belt/window | 1600×900 | 4.438 | 3.737 | 4.221 | 0.620 | 1.142 | 1.267 | 0.702 |
| belt/fullscreen | 3440×1440 | 11.182 | 10.256 | 16.585 | 0.647 | 3.394 | 3.868 | 2.214 |
| belt-sun/window | 1600×900 | 3.074 | 2.370 | 2.646 | 0.618 | 0.613 | 0.484 | 0.649 |
| belt-sun/fullscreen | 3440×1440 | 7.130 | 6.355 | 7.082 | 0.772 | 2.001 | 1.512 | 2.021 |

## Children

The scopes inside the groups: indicative, since a scope reads where its commands were issued rather than an exact cost.

| Scene / mode | Scope | Median | p95 | Run range |
| --- | --- | ---: | ---: | ---: |
| earth/window | gpu_cull_ms | 0.202 | 0.203 | 0.201 to 0.202 |
| earth/window | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.018 |
| earth/window | gpu_belt_light_ms | 0.368 | 0.515 | 0.366 to 0.368 |
| earth/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| earth/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| earth/window | gpu_surface_sky_ms | 0.217 | 0.220 | 0.216 to 0.218 |
| earth/window | gpu_surface_bodies_ms | 0.324 | 0.329 | 0.321 to 0.325 |
| earth/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/window | gpu_surface_splats_ms | 0.005 | 0.008 | 0.005 to 0.005 |
| earth/window | gpu_surface_clouds_ms | 0.176 | 0.179 | 0.175 to 0.176 |
| earth/window | gpu_surface_motion_ms | 0.049 | 0.050 | 0.049 to 0.049 |
| earth/window | gpu_atmospheres_ms | 0.769 | 0.779 | 0.763 to 0.773 |
| earth/window | gpu_belt_dust_ms | 0.103 | 0.106 | 0.102 to 0.103 |
| earth/window | gpu_splat_mask_ms | 0.008 | 0.009 | 0.008 to 0.008 |
| earth/window | gpu_temporal_ms | 0.213 | 0.218 | 0.211 to 0.213 |
| earth/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| earth/window | gpu_bloom_ms | 0.086 | 0.089 | 0.086 to 0.086 |
| earth/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| earth/window | gpu_composite_ms | 0.171 | 0.172 | 0.170 to 0.172 |
| earth/window | gpu_spatial_aa_ms | 0.133 | 0.135 | 0.133 to 0.134 |
| earth/window | gpu_meter_ms | 0.007 | 0.010 | 0.007 to 0.007 |
| earth/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| earth/fullscreen | gpu_cull_ms | 0.209 | 0.212 | 0.208 to 0.209 |
| earth/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| earth/fullscreen | gpu_belt_light_ms | 0.495 | 0.569 | 0.375 to 0.495 |
| earth/fullscreen | gpu_belt_disc_ms | 0.000 | 0.118 | 0.000 to 0.000 |
| earth/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.127 | 0.001 to 0.001 |
| earth/fullscreen | gpu_surface_sky_ms | 0.933 | 0.964 | 0.927 to 0.933 |
| earth/fullscreen | gpu_surface_bodies_ms | 0.660 | 0.892 | 0.658 to 0.663 |
| earth/fullscreen | gpu_surface_rocks_ms | 0.025 | 0.027 | 0.024 to 0.025 |
| earth/fullscreen | gpu_surface_splats_ms | 0.004 | 0.007 | 0.004 to 0.004 |
| earth/fullscreen | gpu_surface_clouds_ms | 0.380 | 0.391 | 0.378 to 0.380 |
| earth/fullscreen | gpu_surface_motion_ms | 0.150 | 0.154 | 0.148 to 0.150 |
| earth/fullscreen | gpu_atmospheres_ms | 2.106 | 2.391 | 2.087 to 2.107 |
| earth/fullscreen | gpu_belt_dust_ms | 0.313 | 0.489 | 0.311 to 0.315 |
| earth/fullscreen | gpu_splat_mask_ms | 0.008 | 0.009 | 0.008 to 0.008 |
| earth/fullscreen | gpu_temporal_ms | 0.700 | 0.857 | 0.695 to 0.701 |
| earth/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| earth/fullscreen | gpu_bloom_ms | 0.245 | 0.250 | 0.243 to 0.245 |
| earth/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| earth/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| earth/fullscreen | gpu_composite_ms | 0.572 | 0.585 | 0.568 to 0.572 |
| earth/fullscreen | gpu_spatial_aa_ms | 0.394 | 0.404 | 0.392 to 0.395 |
| earth/fullscreen | gpu_meter_ms | 0.015 | 0.017 | 0.015 to 0.015 |
| earth/fullscreen | gpu_present_ms | 0.077 | 0.077 | 0.076 to 0.077 |
| giant/window | gpu_cull_ms | 0.255 | 0.258 | 0.254 to 0.255 |
| giant/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| giant/window | gpu_belt_light_ms | 0.366 | 0.488 | 0.364 to 0.367 |
| giant/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| giant/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| giant/window | gpu_surface_sky_ms | 0.252 | 0.255 | 0.249 to 0.253 |
| giant/window | gpu_surface_bodies_ms | 0.293 | 0.298 | 0.290 to 0.293 |
| giant/window | gpu_surface_rocks_ms | 0.038 | 0.040 | 0.038 to 0.038 |
| giant/window | gpu_surface_splats_ms | 0.109 | 0.112 | 0.109 to 0.109 |
| giant/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/window | gpu_surface_motion_ms | 0.051 | 0.051 | 0.050 to 0.051 |
| giant/window | gpu_atmospheres_ms | 0.541 | 0.547 | 0.535 to 0.542 |
| giant/window | gpu_belt_dust_ms | 0.450 | 0.456 | 0.444 to 0.451 |
| giant/window | gpu_splat_mask_ms | 0.119 | 0.122 | 0.118 to 0.119 |
| giant/window | gpu_temporal_ms | 0.216 | 0.217 | 0.214 to 0.216 |
| giant/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/window | gpu_bloom_ms | 0.084 | 0.086 | 0.084 to 0.084 |
| giant/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| giant/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| giant/window | gpu_composite_ms | 0.172 | 0.172 | 0.170 to 0.172 |
| giant/window | gpu_spatial_aa_ms | 0.126 | 0.128 | 0.125 to 0.126 |
| giant/window | gpu_meter_ms | 0.008 | 0.012 | 0.008 to 0.008 |
| giant/window | gpu_present_ms | 0.027 | 0.027 | 0.026 to 0.027 |
| giant/fullscreen | gpu_cull_ms | 0.277 | 0.281 | 0.275 to 0.278 |
| giant/fullscreen | gpu_body_shadow_ms | 0.020 | 0.021 | 0.019 to 0.020 |
| giant/fullscreen | gpu_belt_light_ms | 0.492 | 0.511 | 0.375 to 0.498 |
| giant/fullscreen | gpu_belt_disc_ms | 0.000 | 0.118 | 0.000 to 0.000 |
| giant/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.118 | 0.001 to 0.002 |
| giant/fullscreen | gpu_surface_sky_ms | 0.987 | 1.238 | 0.979 to 0.993 |
| giant/fullscreen | gpu_surface_bodies_ms | 0.662 | 0.685 | 0.656 to 0.665 |
| giant/fullscreen | gpu_surface_rocks_ms | 0.114 | 0.120 | 0.113 to 0.114 |
| giant/fullscreen | gpu_surface_splats_ms | 0.194 | 0.197 | 0.194 to 0.194 |
| giant/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/fullscreen | gpu_surface_motion_ms | 0.163 | 0.166 | 0.161 to 0.164 |
| giant/fullscreen | gpu_atmospheres_ms | 1.495 | 1.802 | 1.490 to 1.500 |
| giant/fullscreen | gpu_belt_dust_ms | 1.538 | 1.886 | 1.527 to 1.547 |
| giant/fullscreen | gpu_splat_mask_ms | 0.202 | 0.204 | 0.201 to 0.203 |
| giant/fullscreen | gpu_temporal_ms | 0.724 | 0.741 | 0.718 to 0.725 |
| giant/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| giant/fullscreen | gpu_bloom_ms | 0.256 | 0.260 | 0.254 to 0.257 |
| giant/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| giant/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| giant/fullscreen | gpu_composite_ms | 0.575 | 0.588 | 0.571 to 0.579 |
| giant/fullscreen | gpu_spatial_aa_ms | 0.381 | 0.386 | 0.378 to 0.382 |
| giant/fullscreen | gpu_meter_ms | 0.016 | 0.019 | 0.016 to 0.016 |
| giant/fullscreen | gpu_present_ms | 0.078 | 0.079 | 0.078 to 0.078 |
| moon/window | gpu_cull_ms | 0.131 | 0.132 | 0.131 to 0.131 |
| moon/window | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| moon/window | gpu_belt_light_ms | 0.367 | 0.490 | 0.366 to 0.367 |
| moon/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| moon/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/window | gpu_surface_sky_ms | 0.228 | 0.230 | 0.227 to 0.229 |
| moon/window | gpu_surface_bodies_ms | 0.133 | 0.135 | 0.133 to 0.133 |
| moon/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_surface_splats_ms | 0.000 | 0.001 | 0.000 to 0.001 |
| moon/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/window | gpu_surface_motion_ms | 0.049 | 0.050 | 0.049 to 0.049 |
| moon/window | gpu_atmospheres_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/window | gpu_belt_dust_ms | 0.067 | 0.067 | 0.066 to 0.067 |
| moon/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| moon/window | gpu_temporal_ms | 0.208 | 0.209 | 0.207 to 0.208 |
| moon/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/window | gpu_bloom_ms | 0.078 | 0.080 | 0.078 to 0.078 |
| moon/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| moon/window | gpu_composite_ms | 0.171 | 0.172 | 0.170 to 0.171 |
| moon/window | gpu_spatial_aa_ms | 0.135 | 0.136 | 0.135 to 0.135 |
| moon/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| moon/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| moon/fullscreen | gpu_cull_ms | 0.137 | 0.140 | 0.137 to 0.137 |
| moon/fullscreen | gpu_body_shadow_ms | 0.019 | 0.019 | 0.019 to 0.019 |
| moon/fullscreen | gpu_belt_light_ms | 0.496 | 0.508 | 0.492 to 0.500 |
| moon/fullscreen | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| moon/fullscreen | gpu_surface_prepass_ms | 0.002 | 0.116 | 0.002 to 0.114 |
| moon/fullscreen | gpu_surface_sky_ms | 0.928 | 0.929 | 0.920 to 0.928 |
| moon/fullscreen | gpu_surface_bodies_ms | 0.268 | 0.269 | 0.266 to 0.268 |
| moon/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/fullscreen | gpu_surface_motion_ms | 0.145 | 0.146 | 0.144 to 0.145 |
| moon/fullscreen | gpu_atmospheres_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| moon/fullscreen | gpu_belt_dust_ms | 0.247 | 0.248 | 0.245 to 0.247 |
| moon/fullscreen | gpu_splat_mask_ms | 0.005 | 0.006 | 0.005 to 0.005 |
| moon/fullscreen | gpu_temporal_ms | 0.689 | 0.710 | 0.685 to 0.690 |
| moon/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| moon/fullscreen | gpu_bloom_ms | 0.245 | 0.247 | 0.244 to 0.246 |
| moon/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| moon/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| moon/fullscreen | gpu_composite_ms | 0.570 | 0.572 | 0.567 to 0.570 |
| moon/fullscreen | gpu_spatial_aa_ms | 0.409 | 0.417 | 0.407 to 0.410 |
| moon/fullscreen | gpu_meter_ms | 0.014 | 0.017 | 0.014 to 0.014 |
| moon/fullscreen | gpu_present_ms | 0.075 | 0.076 | 0.075 to 0.076 |
| mars/window | gpu_cull_ms | 0.131 | 0.132 | 0.131 to 0.131 |
| mars/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| mars/window | gpu_belt_light_ms | 0.367 | 0.493 | 0.366 to 0.367 |
| mars/window | gpu_belt_disc_ms | 0.000 | 0.117 | 0.000 to 0.000 |
| mars/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| mars/window | gpu_surface_sky_ms | 0.257 | 0.406 | 0.256 to 0.257 |
| mars/window | gpu_surface_bodies_ms | 0.120 | 0.122 | 0.120 to 0.121 |
| mars/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_surface_splats_ms | 0.000 | 0.001 | 0.000 to 0.000 |
| mars/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/window | gpu_surface_motion_ms | 0.052 | 0.053 | 0.052 to 0.052 |
| mars/window | gpu_atmospheres_ms | 0.367 | 0.371 | 0.365 to 0.367 |
| mars/window | gpu_belt_dust_ms | 0.066 | 0.067 | 0.066 to 0.066 |
| mars/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| mars/window | gpu_temporal_ms | 0.205 | 0.206 | 0.204 to 0.205 |
| mars/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/window | gpu_bloom_ms | 0.077 | 0.078 | 0.076 to 0.077 |
| mars/window | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/window | gpu_flare_ms | 0.003 | 0.003 | 0.003 to 0.003 |
| mars/window | gpu_composite_ms | 0.171 | 0.171 | 0.170 to 0.171 |
| mars/window | gpu_spatial_aa_ms | 0.115 | 0.117 | 0.114 to 0.115 |
| mars/window | gpu_meter_ms | 0.007 | 0.009 | 0.007 to 0.007 |
| mars/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| mars/fullscreen | gpu_cull_ms | 0.178 | 0.180 | 0.178 to 0.179 |
| mars/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| mars/fullscreen | gpu_belt_light_ms | 0.495 | 0.506 | 0.488 to 0.501 |
| mars/fullscreen | gpu_belt_disc_ms | 0.000 | 0.118 | 0.000 to 0.000 |
| mars/fullscreen | gpu_surface_prepass_ms | 0.002 | 0.118 | 0.001 to 0.113 |
| mars/fullscreen | gpu_surface_sky_ms | 1.019 | 1.024 | 1.009 to 1.023 |
| mars/fullscreen | gpu_surface_bodies_ms | 0.247 | 0.249 | 0.245 to 0.248 |
| mars/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/fullscreen | gpu_surface_splats_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| mars/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/fullscreen | gpu_surface_motion_ms | 0.153 | 0.154 | 0.151 to 0.153 |
| mars/fullscreen | gpu_atmospheres_ms | 0.950 | 0.970 | 0.945 to 0.956 |
| mars/fullscreen | gpu_belt_dust_ms | 0.254 | 0.257 | 0.253 to 0.256 |
| mars/fullscreen | gpu_splat_mask_ms | 0.006 | 0.007 | 0.006 to 0.006 |
| mars/fullscreen | gpu_temporal_ms | 0.681 | 0.697 | 0.676 to 0.683 |
| mars/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| mars/fullscreen | gpu_bloom_ms | 0.244 | 0.245 | 0.241 to 0.245 |
| mars/fullscreen | gpu_sun_visibility_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| mars/fullscreen | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| mars/fullscreen | gpu_composite_ms | 0.570 | 0.584 | 0.566 to 0.572 |
| mars/fullscreen | gpu_spatial_aa_ms | 0.345 | 0.348 | 0.343 to 0.346 |
| mars/fullscreen | gpu_meter_ms | 0.014 | 0.016 | 0.014 to 0.014 |
| mars/fullscreen | gpu_present_ms | 0.076 | 0.076 | 0.075 to 0.076 |
| dawn/window | gpu_cull_ms | 0.131 | 0.132 | 0.130 to 0.131 |
| dawn/window | gpu_body_shadow_ms | 0.018 | 0.019 | 0.018 to 0.019 |
| dawn/window | gpu_belt_light_ms | 0.371 | 0.494 | 0.370 to 0.371 |
| dawn/window | gpu_belt_disc_ms | 0.000 | 0.116 | 0.000 to 0.000 |
| dawn/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| dawn/window | gpu_surface_sky_ms | 0.133 | 0.136 | 0.133 to 0.134 |
| dawn/window | gpu_surface_bodies_ms | 0.341 | 0.344 | 0.340 to 0.342 |
| dawn/window | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/window | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/window | gpu_surface_clouds_ms | 0.197 | 0.200 | 0.197 to 0.198 |
| dawn/window | gpu_surface_motion_ms | 0.049 | 0.053 | 0.049 to 0.049 |
| dawn/window | gpu_atmospheres_ms | 0.853 | 0.861 | 0.849 to 0.854 |
| dawn/window | gpu_belt_dust_ms | 0.065 | 0.066 | 0.065 to 0.066 |
| dawn/window | gpu_splat_mask_ms | 0.002 | 0.002 | 0.002 to 0.002 |
| dawn/window | gpu_temporal_ms | 0.215 | 0.216 | 0.215 to 0.215 |
| dawn/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| dawn/window | gpu_bloom_ms | 0.097 | 0.099 | 0.096 to 0.097 |
| dawn/window | gpu_sun_visibility_ms | 0.028 | 0.028 | 0.028 to 0.028 |
| dawn/window | gpu_flare_ms | 0.009 | 0.010 | 0.009 to 0.009 |
| dawn/window | gpu_composite_ms | 0.178 | 0.178 | 0.177 to 0.178 |
| dawn/window | gpu_spatial_aa_ms | 0.120 | 0.122 | 0.120 to 0.120 |
| dawn/window | gpu_meter_ms | 0.008 | 0.013 | 0.008 to 0.009 |
| dawn/window | gpu_present_ms | 0.026 | 0.026 | 0.026 to 0.026 |
| dawn/fullscreen | gpu_cull_ms | 0.132 | 0.134 | 0.131 to 0.132 |
| dawn/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.018 to 0.019 |
| dawn/fullscreen | gpu_belt_light_ms | 0.499 | 0.511 | 0.496 to 0.499 |
| dawn/fullscreen | gpu_belt_disc_ms | 0.000 | 0.118 | 0.000 to 0.000 |
| dawn/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_sky_ms | 0.637 | 0.654 | 0.634 to 0.638 |
| dawn/fullscreen | gpu_surface_bodies_ms | 0.722 | 0.930 | 0.713 to 0.723 |
| dawn/fullscreen | gpu_surface_rocks_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_splats_ms | 0.001 | 0.001 | 0.001 to 0.001 |
| dawn/fullscreen | gpu_surface_clouds_ms | 0.457 | 0.473 | 0.454 to 0.459 |
| dawn/fullscreen | gpu_surface_motion_ms | 0.140 | 0.142 | 0.139 to 0.140 |
| dawn/fullscreen | gpu_atmospheres_ms | 2.309 | 2.582 | 2.293 to 2.309 |
| dawn/fullscreen | gpu_belt_dust_ms | 0.218 | 0.223 | 0.217 to 0.219 |
| dawn/fullscreen | gpu_splat_mask_ms | 0.005 | 0.006 | 0.005 to 0.005 |
| dawn/fullscreen | gpu_temporal_ms | 0.712 | 0.725 | 0.709 to 0.712 |
| dawn/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| dawn/fullscreen | gpu_bloom_ms | 0.225 | 0.229 | 0.224 to 0.225 |
| dawn/fullscreen | gpu_sun_visibility_ms | 0.031 | 0.031 | 0.030 to 0.031 |
| dawn/fullscreen | gpu_flare_ms | 0.024 | 0.024 | 0.024 to 0.024 |
| dawn/fullscreen | gpu_composite_ms | 0.593 | 0.602 | 0.592 to 0.593 |
| dawn/fullscreen | gpu_spatial_aa_ms | 0.366 | 0.374 | 0.364 to 0.367 |
| dawn/fullscreen | gpu_meter_ms | 0.015 | 0.018 | 0.015 to 0.016 |
| dawn/fullscreen | gpu_present_ms | 0.075 | 0.076 | 0.075 to 0.075 |
| belt/window | gpu_cull_ms | 0.234 | 0.235 | 0.232 to 0.234 |
| belt/window | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| belt/window | gpu_belt_light_ms | 0.366 | 0.488 | 0.365 to 0.367 |
| belt/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt/window | gpu_surface_sky_ms | 0.110 | 0.112 | 0.109 to 0.110 |
| belt/window | gpu_surface_bodies_ms | 0.438 | 0.444 | 0.436 to 0.438 |
| belt/window | gpu_surface_rocks_ms | 0.347 | 0.354 | 0.345 to 0.347 |
| belt/window | gpu_surface_splats_ms | 0.049 | 0.051 | 0.048 to 0.049 |
| belt/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_surface_motion_ms | 0.079 | 0.079 | 0.078 to 0.079 |
| belt/window | gpu_atmospheres_ms | 0.836 | 0.841 | 0.830 to 0.836 |
| belt/window | gpu_belt_dust_ms | 0.367 | 0.371 | 0.366 to 0.368 |
| belt/window | gpu_splat_mask_ms | 0.061 | 0.062 | 0.061 to 0.061 |
| belt/window | gpu_temporal_ms | 0.222 | 0.223 | 0.222 to 0.223 |
| belt/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/window | gpu_bloom_ms | 0.085 | 0.087 | 0.085 to 0.085 |
| belt/window | gpu_sun_visibility_ms | 0.028 | 0.029 | 0.028 to 0.028 |
| belt/window | gpu_flare_ms | 0.009 | 0.010 | 0.009 to 0.009 |
| belt/window | gpu_composite_ms | 0.177 | 0.178 | 0.177 to 0.178 |
| belt/window | gpu_spatial_aa_ms | 0.142 | 0.144 | 0.141 to 0.142 |
| belt/window | gpu_meter_ms | 0.008 | 0.010 | 0.008 to 0.008 |
| belt/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.027 |
| belt/fullscreen | gpu_cull_ms | 0.254 | 0.258 | 0.253 to 0.255 |
| belt/fullscreen | gpu_body_shadow_ms | 0.019 | 0.020 | 0.019 to 0.019 |
| belt/fullscreen | gpu_belt_light_ms | 0.370 | 0.503 | 0.368 to 0.373 |
| belt/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.102 | 0.001 to 0.001 |
| belt/fullscreen | gpu_surface_sky_ms | 0.508 | 0.627 | 0.507 to 0.510 |
| belt/fullscreen | gpu_surface_bodies_ms | 1.018 | 3.195 | 1.016 to 1.021 |
| belt/fullscreen | gpu_surface_rocks_ms | 1.292 | 1.529 | 1.289 to 1.293 |
| belt/fullscreen | gpu_surface_splats_ms | 0.069 | 0.080 | 0.069 to 0.069 |
| belt/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_surface_motion_ms | 0.229 | 0.231 | 0.229 to 0.231 |
| belt/fullscreen | gpu_atmospheres_ms | 2.565 | 5.500 | 2.557 to 2.575 |
| belt/fullscreen | gpu_belt_dust_ms | 1.208 | 1.533 | 1.205 to 1.212 |
| belt/fullscreen | gpu_splat_mask_ms | 0.086 | 0.087 | 0.086 to 0.086 |
| belt/fullscreen | gpu_temporal_ms | 0.742 | 0.921 | 0.740 to 0.742 |
| belt/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt/fullscreen | gpu_bloom_ms | 0.273 | 0.276 | 0.272 to 0.273 |
| belt/fullscreen | gpu_sun_visibility_ms | 0.030 | 0.031 | 0.030 to 0.031 |
| belt/fullscreen | gpu_flare_ms | 0.024 | 0.024 | 0.024 to 0.024 |
| belt/fullscreen | gpu_composite_ms | 0.595 | 0.757 | 0.593 to 0.595 |
| belt/fullscreen | gpu_spatial_aa_ms | 0.446 | 0.490 | 0.444 to 0.447 |
| belt/fullscreen | gpu_meter_ms | 0.018 | 0.020 | 0.018 to 0.018 |
| belt/fullscreen | gpu_present_ms | 0.078 | 0.079 | 0.078 to 0.078 |
| belt-sun/window | gpu_cull_ms | 0.234 | 0.237 | 0.233 to 0.234 |
| belt-sun/window | gpu_body_shadow_ms | 0.020 | 0.020 | 0.019 to 0.020 |
| belt-sun/window | gpu_belt_light_ms | 0.362 | 0.484 | 0.361 to 0.364 |
| belt-sun/window | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_surface_prepass_ms | 0.001 | 0.002 | 0.001 to 0.001 |
| belt-sun/window | gpu_surface_sky_ms | 0.231 | 0.234 | 0.230 to 0.232 |
| belt-sun/window | gpu_surface_bodies_ms | 0.001 | 0.006 | 0.001 to 0.001 |
| belt-sun/window | gpu_surface_rocks_ms | 0.143 | 0.147 | 0.143 to 0.143 |
| belt-sun/window | gpu_surface_splats_ms | 0.077 | 0.078 | 0.076 to 0.077 |
| belt-sun/window | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_surface_motion_ms | 0.078 | 0.079 | 0.078 to 0.078 |
| belt-sun/window | gpu_atmospheres_ms | 0.045 | 0.049 | 0.045 to 0.045 |
| belt-sun/window | gpu_belt_dust_ms | 0.357 | 0.362 | 0.355 to 0.357 |
| belt-sun/window | gpu_splat_mask_ms | 0.079 | 0.080 | 0.079 to 0.079 |
| belt-sun/window | gpu_temporal_ms | 0.209 | 0.210 | 0.209 to 0.210 |
| belt-sun/window | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/window | gpu_bloom_ms | 0.076 | 0.078 | 0.076 to 0.076 |
| belt-sun/window | gpu_sun_visibility_ms | 0.028 | 0.029 | 0.028 to 0.028 |
| belt-sun/window | gpu_flare_ms | 0.006 | 0.006 | 0.006 to 0.006 |
| belt-sun/window | gpu_composite_ms | 0.172 | 0.173 | 0.172 to 0.172 |
| belt-sun/window | gpu_spatial_aa_ms | 0.120 | 0.122 | 0.120 to 0.120 |
| belt-sun/window | gpu_meter_ms | 0.008 | 0.009 | 0.007 to 0.008 |
| belt-sun/window | gpu_present_ms | 0.026 | 0.027 | 0.026 to 0.026 |
| belt-sun/fullscreen | gpu_cull_ms | 0.252 | 0.256 | 0.251 to 0.253 |
| belt-sun/fullscreen | gpu_body_shadow_ms | 0.020 | 0.021 | 0.019 to 0.020 |
| belt-sun/fullscreen | gpu_belt_light_ms | 0.498 | 0.513 | 0.496 to 0.504 |
| belt-sun/fullscreen | gpu_belt_disc_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_surface_prepass_ms | 0.001 | 0.106 | 0.001 to 0.001 |
| belt-sun/fullscreen | gpu_surface_sky_ms | 0.890 | 0.905 | 0.886 to 0.890 |
| belt-sun/fullscreen | gpu_surface_bodies_ms | 0.014 | 0.032 | 0.013 to 0.014 |
| belt-sun/fullscreen | gpu_surface_rocks_ms | 0.485 | 0.627 | 0.481 to 0.486 |
| belt-sun/fullscreen | gpu_surface_splats_ms | 0.118 | 0.120 | 0.118 to 0.118 |
| belt-sun/fullscreen | gpu_surface_clouds_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_surface_motion_ms | 0.252 | 0.256 | 0.251 to 0.252 |
| belt-sun/fullscreen | gpu_atmospheres_ms | 0.284 | 0.289 | 0.282 to 0.284 |
| belt-sun/fullscreen | gpu_belt_dust_ms | 1.112 | 1.304 | 1.107 to 1.114 |
| belt-sun/fullscreen | gpu_splat_mask_ms | 0.114 | 0.116 | 0.113 to 0.114 |
| belt-sun/fullscreen | gpu_temporal_ms | 0.693 | 0.841 | 0.692 to 0.696 |
| belt-sun/fullscreen | gpu_streaks_ms | 0.000 | 0.000 | 0.000 to 0.000 |
| belt-sun/fullscreen | gpu_bloom_ms | 0.214 | 0.220 | 0.214 to 0.215 |
| belt-sun/fullscreen | gpu_sun_visibility_ms | 0.032 | 0.033 | 0.032 to 0.032 |
| belt-sun/fullscreen | gpu_flare_ms | 0.015 | 0.015 | 0.015 to 0.015 |
| belt-sun/fullscreen | gpu_composite_ms | 0.577 | 0.620 | 0.574 to 0.578 |
| belt-sun/fullscreen | gpu_spatial_aa_ms | 0.392 | 0.406 | 0.391 to 0.394 |
| belt-sun/fullscreen | gpu_meter_ms | 0.016 | 0.019 | 0.016 to 0.016 |
| belt-sun/fullscreen | gpu_present_ms | 0.077 | 0.078 | 0.077 to 0.078 |

## Compared with motion-vectors

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| earth/window | cpu_submit_and_wait_ms | +0.032 | +0.8 | within threshold |
| earth/window | gpu_ms | -0.145 | -4.4 | within threshold |
| earth/window | cpu_prepare_ms | +0.030 | +12.1 | within threshold |
| earth/window | gpu_cull_shadow_ms | +0.007 | +1.2 | within threshold |
| earth/window | gpu_surface_ms | +0.007 | +0.7 | within threshold |
| earth/window | gpu_atmosphere_ms | -0.097 | -9.9 | within threshold |
| earth/window | gpu_post_ms | +0.013 | +2.1 | within threshold |
| earth/window | gpu_cull_ms | +0.006 | +3.3 | within threshold |
| earth/window | gpu_body_shadow_ms | +0.000 | +1.9 | within threshold |
| earth/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| earth/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_sky_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_bodies_ms | +0.004 | +1.3 | within threshold |
| earth/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_splats_ms | +0.001 | +25.0 | within threshold |
| earth/window | gpu_surface_clouds_ms | +0.001 | +0.6 | within threshold |
| earth/window | gpu_surface_motion_ms | +0.001 | +2.1 | within threshold |
| earth/window | gpu_atmospheres_ms | +0.002 | +0.3 | within threshold |
| earth/window | gpu_belt_dust_ms | -0.099 | -49.0 | within threshold |
| earth/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_temporal_ms | +0.005 | +2.5 | within threshold |
| earth/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_bloom_ms | +0.003 | +3.7 | within threshold |
| earth/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_composite_ms | +0.002 | +1.2 | within threshold |
| earth/window | gpu_spatial_aa_ms | +0.002 | +1.6 | within threshold |
| earth/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | cpu_submit_and_wait_ms | -0.137 | -1.5 | within threshold |
| earth/fullscreen | gpu_ms | -0.261 | -3.2 | within threshold |
| earth/fullscreen | cpu_prepare_ms | +0.018 | +6.3 | within threshold |
| earth/fullscreen | gpu_cull_shadow_ms | +0.008 | +1.1 | within threshold |
| earth/fullscreen | gpu_surface_ms | -0.022 | -0.8 | within threshold |
| earth/fullscreen | gpu_atmosphere_ms | -0.265 | -9.8 | within threshold |
| earth/fullscreen | gpu_post_ms | +0.026 | +1.3 | within threshold |
| earth/fullscreen | gpu_cull_ms | +0.006 | +3.1 | within threshold |
| earth/fullscreen | gpu_body_shadow_ms | +0.000 | +1.0 | within threshold |
| earth/fullscreen | gpu_belt_light_ms | +0.002 | +0.4 | within threshold |
| earth/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_sky_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_rocks_ms | +0.001 | +4.3 | within threshold |
| earth/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_surface_clouds_ms | +0.001 | +0.3 | within threshold |
| earth/fullscreen | gpu_surface_motion_ms | +0.001 | +0.7 | within threshold |
| earth/fullscreen | gpu_atmospheres_ms | +0.010 | +0.5 | within threshold |
| earth/fullscreen | gpu_belt_dust_ms | -0.272 | -46.5 | within threshold |
| earth/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_temporal_ms | +0.012 | +1.8 | within threshold |
| earth/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_bloom_ms | +0.001 | +0.4 | within threshold |
| earth/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_composite_ms | +0.005 | +0.9 | within threshold |
| earth/fullscreen | gpu_spatial_aa_ms | +0.007 | +1.9 | within threshold |
| earth/fullscreen | gpu_meter_ms | +0.001 | +7.1 | within threshold |
| earth/fullscreen | gpu_present_ms | +0.002 | +2.5 | within threshold |
| giant/window | cpu_submit_and_wait_ms | -0.553 | -11.6 | within threshold |
| giant/window | gpu_ms | -0.618 | -15.5 | within threshold |
| giant/window | cpu_prepare_ms | +0.125 | +50.7 | within threshold |
| giant/window | gpu_cull_shadow_ms | +0.012 | +2.0 | within threshold |
| giant/window | gpu_surface_ms | +0.013 | +1.4 | within threshold |
| giant/window | gpu_atmosphere_ms | -0.653 | -37.0 | within threshold |
| giant/window | gpu_post_ms | +0.012 | +2.0 | within threshold |
| giant/window | gpu_cull_ms | +0.007 | +2.8 | within threshold |
| giant/window | gpu_body_shadow_ms | +0.000 | +2.0 | within threshold |
| giant/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| giant/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_surface_sky_ms | +0.001 | +0.4 | within threshold |
| giant/window | gpu_surface_bodies_ms | +0.004 | +1.4 | within threshold |
| giant/window | gpu_surface_rocks_ms | +0.001 | +2.8 | within threshold |
| giant/window | gpu_surface_splats_ms | +0.004 | +3.9 | within threshold |
| giant/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_surface_motion_ms | +0.001 | +2.0 | within threshold |
| giant/window | gpu_atmospheres_ms | +0.003 | +0.6 | within threshold |
| giant/window | gpu_belt_dust_ms | -0.658 | -59.4 | within threshold |
| giant/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_temporal_ms | +0.005 | +2.4 | within threshold |
| giant/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_bloom_ms | +0.002 | +2.5 | within threshold |
| giant/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_composite_ms | +0.003 | +1.8 | within threshold |
| giant/window | gpu_spatial_aa_ms | +0.003 | +2.5 | within threshold |
| giant/window | gpu_meter_ms | +0.001 | +14.3 | within threshold |
| giant/window | gpu_present_ms | +0.001 | +4.0 | within threshold |
| giant/fullscreen | cpu_submit_and_wait_ms | -2.235 | -18.9 | within threshold |
| giant/fullscreen | gpu_ms | -2.261 | -20.4 | within threshold |
| giant/fullscreen | cpu_prepare_ms | +0.001 | +0.4 | within threshold |
| giant/fullscreen | gpu_cull_shadow_ms | +0.012 | +1.5 | within threshold |
| giant/fullscreen | gpu_surface_ms | +0.006 | +0.2 | within threshold |
| giant/fullscreen | gpu_atmosphere_ms | -2.300 | -41.5 | within threshold |
| giant/fullscreen | gpu_post_ms | +0.036 | +1.8 | within threshold |
| giant/fullscreen | gpu_cull_ms | +0.008 | +3.0 | within threshold |
| giant/fullscreen | gpu_body_shadow_ms | +0.000 | +2.6 | within threshold |
| giant/fullscreen | gpu_belt_light_ms | +0.005 | +1.1 | within threshold |
| giant/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_surface_sky_ms | +0.001 | +0.1 | within threshold |
| giant/fullscreen | gpu_surface_bodies_ms | +0.005 | +0.8 | within threshold |
| giant/fullscreen | gpu_surface_rocks_ms | +0.001 | +0.9 | within threshold |
| giant/fullscreen | gpu_surface_splats_ms | +0.005 | +2.7 | within threshold |
| giant/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_surface_motion_ms | +0.002 | +1.3 | within threshold |
| giant/fullscreen | gpu_atmospheres_ms | +0.006 | +0.4 | within threshold |
| giant/fullscreen | gpu_belt_dust_ms | -2.291 | -59.8 | within threshold |
| giant/fullscreen | gpu_splat_mask_ms | +0.006 | +3.1 | within threshold |
| giant/fullscreen | gpu_temporal_ms | +0.016 | +2.3 | within threshold |
| giant/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_bloom_ms | +0.003 | +1.2 | within threshold |
| giant/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_composite_ms | +0.008 | +1.4 | within threshold |
| giant/fullscreen | gpu_spatial_aa_ms | +0.010 | +2.8 | within threshold |
| giant/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_present_ms | +0.002 | +2.7 | within threshold |
| moon/window | cpu_submit_and_wait_ms | +0.002 | +0.1 | within threshold |
| moon/window | gpu_ms | -0.095 | -4.8 | within threshold |
| moon/window | cpu_prepare_ms | +0.019 | +8.2 | within threshold |
| moon/window | gpu_cull_shadow_ms | -0.109 | -17.3 | within threshold |
| moon/window | gpu_surface_ms | +0.005 | +0.8 | within threshold |
| moon/window | gpu_atmosphere_ms | -0.004 | -5.4 | within threshold |
| moon/window | gpu_post_ms | +0.010 | +1.6 | within threshold |
| moon/window | gpu_cull_ms | +0.005 | +4.3 | within threshold |
| moon/window | gpu_body_shadow_ms | +0.000 | +2.1 | within threshold |
| moon/window | gpu_belt_light_ms | -0.115 | -23.8 | within threshold |
| moon/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_sky_ms | +0.001 | +0.5 | within threshold |
| moon/window | gpu_surface_bodies_ms | +0.002 | +1.6 | within threshold |
| moon/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_splats_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_surface_motion_ms | +0.001 | +2.1 | within threshold |
| moon/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_belt_dust_ms | -0.003 | -4.4 | within threshold |
| moon/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_temporal_ms | +0.004 | +2.0 | within threshold |
| moon/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_bloom_ms | +0.002 | +2.7 | within threshold |
| moon/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_composite_ms | +0.002 | +1.2 | within threshold |
| moon/window | gpu_spatial_aa_ms | +0.002 | +1.5 | within threshold |
| moon/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| moon/fullscreen | cpu_submit_and_wait_ms | -0.050 | -0.9 | within threshold |
| moon/fullscreen | gpu_ms | -0.067 | -1.3 | within threshold |
| moon/fullscreen | cpu_prepare_ms | +0.036 | +14.3 | within threshold |
| moon/fullscreen | gpu_cull_shadow_ms | +0.009 | +1.4 | within threshold |
| moon/fullscreen | gpu_surface_ms | -0.059 | -3.0 | within threshold |
| moon/fullscreen | gpu_atmosphere_ms | -0.099 | -28.0 | within threshold |
| moon/fullscreen | gpu_post_ms | +0.028 | +1.4 | within threshold |
| moon/fullscreen | gpu_cull_ms | +0.005 | +3.8 | within threshold |
| moon/fullscreen | gpu_body_shadow_ms | +0.000 | +1.3 | within threshold |
| moon/fullscreen | gpu_belt_light_ms | +0.001 | +0.2 | within threshold |
| moon/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_surface_prepass_ms | -0.111 | -98.2 | within threshold |
| moon/fullscreen | gpu_surface_sky_ms | +0.002 | +0.2 | within threshold |
| moon/fullscreen | gpu_surface_bodies_ms | +0.004 | +1.6 | within threshold |
| moon/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_splats_ms | +0.001 | n/a | within threshold |
| moon/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_surface_motion_ms | +0.001 | +0.7 | within threshold |
| moon/fullscreen | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_belt_dust_ms | -0.098 | -28.5 | within threshold |
| moon/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_temporal_ms | +0.012 | +1.8 | within threshold |
| moon/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_bloom_ms | +0.001 | +0.4 | within threshold |
| moon/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_composite_ms | +0.005 | +0.9 | within threshold |
| moon/fullscreen | gpu_spatial_aa_ms | +0.008 | +2.0 | within threshold |
| moon/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_present_ms | +0.001 | +1.4 | within threshold |
| mars/window | cpu_submit_and_wait_ms | +0.009 | +0.3 | within threshold |
| mars/window | gpu_ms | -0.084 | -3.6 | within threshold |
| mars/window | cpu_prepare_ms | +0.019 | +8.3 | within threshold |
| mars/window | gpu_cull_shadow_ms | -0.110 | -17.4 | within threshold |
| mars/window | gpu_surface_ms | +0.005 | +0.8 | within threshold |
| mars/window | gpu_atmosphere_ms | -0.001 | -0.2 | within threshold |
| mars/window | gpu_post_ms | +0.010 | +1.7 | within threshold |
| mars/window | gpu_cull_ms | +0.005 | +4.4 | within threshold |
| mars/window | gpu_body_shadow_ms | +0.000 | +1.8 | within threshold |
| mars/window | gpu_belt_light_ms | -0.116 | -24.0 | within threshold |
| mars/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_sky_ms | +0.002 | +0.8 | within threshold |
| mars/window | gpu_surface_bodies_ms | +0.002 | +1.7 | within threshold |
| mars/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_splats_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_surface_motion_ms | +0.001 | +2.0 | within threshold |
| mars/window | gpu_atmospheres_ms | +0.002 | +0.6 | within threshold |
| mars/window | gpu_belt_dust_ms | -0.003 | -4.5 | within threshold |
| mars/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_temporal_ms | +0.004 | +2.0 | within threshold |
| mars/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_bloom_ms | +0.001 | +1.4 | within threshold |
| mars/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_composite_ms | +0.002 | +1.2 | within threshold |
| mars/window | gpu_spatial_aa_ms | +0.002 | +1.8 | within threshold |
| mars/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| mars/fullscreen | cpu_submit_and_wait_ms | -0.053 | -0.8 | within threshold |
| mars/fullscreen | gpu_ms | -0.094 | -1.6 | within threshold |
| mars/fullscreen | cpu_prepare_ms | +0.016 | +5.9 | within threshold |
| mars/fullscreen | gpu_cull_shadow_ms | +0.011 | +1.6 | within threshold |
| mars/fullscreen | gpu_surface_ms | -0.068 | -3.3 | within threshold |
| mars/fullscreen | gpu_atmosphere_ms | -0.120 | -9.0 | within threshold |
| mars/fullscreen | gpu_post_ms | +0.030 | +1.6 | within threshold |
| mars/fullscreen | gpu_cull_ms | +0.005 | +2.8 | within threshold |
| mars/fullscreen | gpu_body_shadow_ms | +0.000 | +1.3 | within threshold |
| mars/fullscreen | gpu_belt_light_ms | +0.002 | +0.3 | within threshold |
| mars/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_surface_prepass_ms | -0.110 | -98.2 | within threshold |
| mars/fullscreen | gpu_surface_sky_ms | +0.001 | +0.1 | within threshold |
| mars/fullscreen | gpu_surface_bodies_ms | +0.003 | +1.3 | within threshold |
| mars/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_surface_motion_ms | +0.001 | +0.7 | within threshold |
| mars/fullscreen | gpu_atmospheres_ms | +0.001 | +0.1 | within threshold |
| mars/fullscreen | gpu_belt_dust_ms | -0.120 | -32.1 | within threshold |
| mars/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_temporal_ms | +0.013 | +1.9 | within threshold |
| mars/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_bloom_ms | +0.002 | +0.8 | within threshold |
| mars/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_composite_ms | +0.006 | +1.1 | within threshold |
| mars/fullscreen | gpu_spatial_aa_ms | +0.008 | +2.4 | within threshold |
| mars/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_present_ms | +0.002 | +2.7 | within threshold |
| dawn/window | cpu_submit_and_wait_ms | +0.017 | +0.5 | within threshold |
| dawn/window | gpu_ms | +0.028 | +1.0 | within threshold |
| dawn/window | cpu_prepare_ms | +0.019 | +7.7 | within threshold |
| dawn/window | gpu_cull_shadow_ms | +0.009 | +1.9 | within threshold |
| dawn/window | gpu_surface_ms | +0.007 | +0.9 | within threshold |
| dawn/window | gpu_atmosphere_ms | +0.003 | +0.3 | within threshold |
| dawn/window | gpu_post_ms | +0.014 | +2.1 | within threshold |
| dawn/window | gpu_cull_ms | +0.005 | +4.2 | within threshold |
| dawn/window | gpu_body_shadow_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| dawn/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_sky_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_bodies_ms | +0.003 | +0.9 | within threshold |
| dawn/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_splats_ms | +0.001 | n/a | within threshold |
| dawn/window | gpu_surface_clouds_ms | +0.001 | +0.5 | within threshold |
| dawn/window | gpu_surface_motion_ms | +0.001 | +2.1 | within threshold |
| dawn/window | gpu_atmospheres_ms | +0.005 | +0.6 | within threshold |
| dawn/window | gpu_belt_dust_ms | -0.003 | -4.5 | within threshold |
| dawn/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_temporal_ms | +0.004 | +1.9 | within threshold |
| dawn/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_bloom_ms | +0.005 | +5.6 | within threshold |
| dawn/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_composite_ms | +0.002 | +1.2 | within threshold |
| dawn/window | gpu_spatial_aa_ms | +0.002 | +1.7 | within threshold |
| dawn/window | gpu_meter_ms | +0.001 | +14.3 | within threshold |
| dawn/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| dawn/fullscreen | cpu_submit_and_wait_ms | +0.011 | +0.1 | within threshold |
| dawn/fullscreen | gpu_ms | -0.028 | -0.4 | within threshold |
| dawn/fullscreen | cpu_prepare_ms | +0.006 | +2.1 | within threshold |
| dawn/fullscreen | gpu_cull_shadow_ms | +0.006 | +0.9 | within threshold |
| dawn/fullscreen | gpu_surface_ms | -0.046 | -2.0 | within threshold |
| dawn/fullscreen | gpu_atmosphere_ms | -0.020 | -0.8 | within threshold |
| dawn/fullscreen | gpu_post_ms | +0.027 | +1.3 | within threshold |
| dawn/fullscreen | gpu_cull_ms | +0.005 | +3.6 | within threshold |
| dawn/fullscreen | gpu_body_shadow_ms | -0.000 | -0.3 | within threshold |
| dawn/fullscreen | gpu_belt_light_ms | +0.004 | +0.8 | within threshold |
| dawn/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_sky_ms | -0.001 | -0.2 | within threshold |
| dawn/fullscreen | gpu_surface_bodies_ms | -0.015 | -2.1 | within threshold |
| dawn/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_clouds_ms | -0.001 | -0.2 | within threshold |
| dawn/fullscreen | gpu_surface_motion_ms | +0.001 | +0.7 | within threshold |
| dawn/fullscreen | gpu_atmospheres_ms | -0.007 | -0.3 | within threshold |
| dawn/fullscreen | gpu_belt_dust_ms | -0.012 | -5.3 | within threshold |
| dawn/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_temporal_ms | +0.011 | +1.6 | within threshold |
| dawn/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_bloom_ms | +0.004 | +1.9 | within threshold |
| dawn/fullscreen | gpu_sun_visibility_ms | +0.001 | +3.4 | within threshold |
| dawn/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_composite_ms | +0.004 | +0.7 | within threshold |
| dawn/fullscreen | gpu_spatial_aa_ms | +0.006 | +1.7 | within threshold |
| dawn/fullscreen | gpu_meter_ms | +0.000 | +0.5 | within threshold |
| dawn/fullscreen | gpu_present_ms | +0.001 | +1.4 | within threshold |
| belt/window | cpu_submit_and_wait_ms | -0.202 | -4.4 | within threshold |
| belt/window | gpu_ms | -0.199 | -5.1 | within threshold |
| belt/window | cpu_prepare_ms | +0.017 | +6.7 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.012 | +1.9 | within threshold |
| belt/window | gpu_surface_ms | +0.009 | +0.8 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.222 | -14.9 | within threshold |
| belt/window | gpu_post_ms | +0.008 | +1.2 | within threshold |
| belt/window | gpu_cull_ms | +0.007 | +3.3 | within threshold |
| belt/window | gpu_body_shadow_ms | +0.000 | +1.7 | within threshold |
| belt/window | gpu_belt_light_ms | +0.004 | +1.1 | within threshold |
| belt/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_sky_ms | +0.001 | +0.9 | within threshold |
| belt/window | gpu_surface_bodies_ms | +0.004 | +0.9 | within threshold |
| belt/window | gpu_surface_rocks_ms | +0.004 | +1.2 | within threshold |
| belt/window | gpu_surface_splats_ms | +0.001 | +2.1 | within threshold |
| belt/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_surface_motion_ms | +0.001 | +1.3 | within threshold |
| belt/window | gpu_atmospheres_ms | +0.005 | +0.6 | within threshold |
| belt/window | gpu_belt_dust_ms | -0.228 | -38.4 | within threshold |
| belt/window | gpu_splat_mask_ms | +0.001 | +1.7 | within threshold |
| belt/window | gpu_temporal_ms | +0.004 | +1.9 | within threshold |
| belt/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_bloom_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_composite_ms | +0.001 | +0.6 | within threshold |
| belt/window | gpu_spatial_aa_ms | +0.002 | +1.5 | within threshold |
| belt/window | gpu_meter_ms | +0.001 | +13.8 | within threshold |
| belt/window | gpu_present_ms | +0.001 | +3.2 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | -0.826 | -6.9 | within threshold |
| belt/fullscreen | gpu_ms | -0.930 | -8.3 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.116 | +38.6 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | -0.109 | -14.4 | within threshold |
| belt/fullscreen | gpu_surface_ms | -0.068 | -2.0 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | -0.810 | -17.3 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.011 | +0.5 | within threshold |
| belt/fullscreen | gpu_cull_ms | +0.006 | +2.5 | within threshold |
| belt/fullscreen | gpu_body_shadow_ms | +0.000 | +1.3 | within threshold |
| belt/fullscreen | gpu_belt_light_ms | -0.118 | -24.2 | within threshold |
| belt/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_sky_ms | -0.001 | -0.1 | within threshold |
| belt/fullscreen | gpu_surface_bodies_ms | -0.002 | -0.2 | within threshold |
| belt/fullscreen | gpu_surface_rocks_ms | +0.003 | +0.2 | within threshold |
| belt/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_surface_motion_ms | -0.001 | -0.2 | within threshold |
| belt/fullscreen | gpu_atmospheres_ms | -0.016 | -0.6 | within threshold |
| belt/fullscreen | gpu_belt_dust_ms | -0.769 | -38.9 | within threshold |
| belt/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_temporal_ms | +0.009 | +1.3 | within threshold |
| belt/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_bloom_ms | -0.002 | -0.7 | within threshold |
| belt/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_composite_ms | +0.002 | +0.3 | within threshold |
| belt/fullscreen | gpu_spatial_aa_ms | +0.006 | +1.4 | within threshold |
| belt/fullscreen | gpu_meter_ms | +0.000 | +1.8 | within threshold |
| belt/fullscreen | gpu_present_ms | +0.001 | +1.5 | within threshold |
| belt-sun/window | cpu_submit_and_wait_ms | -0.284 | -8.5 | within threshold |
| belt-sun/window | gpu_ms | -0.383 | -13.9 | within threshold |
| belt-sun/window | cpu_prepare_ms | +0.017 | +6.8 | within threshold |
| belt-sun/window | gpu_cull_shadow_ms | +0.008 | +1.4 | within threshold |
| belt-sun/window | gpu_surface_ms | +0.001 | +0.2 | within threshold |
| belt-sun/window | gpu_atmosphere_ms | -0.296 | -37.9 | within threshold |
| belt-sun/window | gpu_post_ms | +0.005 | +0.8 | within threshold |
| belt-sun/window | gpu_cull_ms | +0.006 | +2.8 | within threshold |
| belt-sun/window | gpu_body_shadow_ms | +0.000 | +1.3 | within threshold |
| belt-sun/window | gpu_belt_light_ms | +0.003 | +0.9 | within threshold |
| belt-sun/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_sky_ms | -0.001 | -0.4 | within threshold |
| belt-sun/window | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_splats_ms | +0.002 | +2.7 | within threshold |
| belt-sun/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_surface_motion_ms | -0.001 | -1.3 | within threshold |
| belt-sun/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_belt_dust_ms | -0.296 | -45.3 | within threshold |
| belt-sun/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_temporal_ms | +0.002 | +1.0 | within threshold |
| belt-sun/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_bloom_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_flare_ms | +0.001 | +20.0 | within threshold |
| belt-sun/window | gpu_composite_ms | +0.001 | +0.6 | within threshold |
| belt-sun/window | gpu_spatial_aa_ms | +0.001 | +0.9 | within threshold |
| belt-sun/window | gpu_meter_ms | +0.000 | +5.4 | within threshold |
| belt-sun/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | cpu_submit_and_wait_ms | -0.853 | -10.7 | within threshold |
| belt-sun/fullscreen | gpu_ms | -0.928 | -12.7 | within threshold |
| belt-sun/fullscreen | cpu_prepare_ms | +0.037 | +12.9 | within threshold |
| belt-sun/fullscreen | gpu_cull_shadow_ms | +0.011 | +1.5 | within threshold |
| belt-sun/fullscreen | gpu_surface_ms | -0.053 | -2.6 | within threshold |
| belt-sun/fullscreen | gpu_atmosphere_ms | -0.916 | -37.7 | within threshold |
| belt-sun/fullscreen | gpu_post_ms | +0.014 | +0.7 | within threshold |
| belt-sun/fullscreen | gpu_cull_ms | +0.007 | +2.9 | within threshold |
| belt-sun/fullscreen | gpu_body_shadow_ms | -0.000 | -0.2 | within threshold |
| belt-sun/fullscreen | gpu_belt_light_ms | +0.004 | +0.8 | within threshold |
| belt-sun/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_sky_ms | +0.002 | +0.2 | within threshold |
| belt-sun/fullscreen | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_rocks_ms | -0.015 | -3.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_splats_ms | +0.003 | +2.7 | within threshold |
| belt-sun/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_surface_motion_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_belt_dust_ms | -0.919 | -45.2 | within threshold |
| belt-sun/fullscreen | gpu_splat_mask_ms | +0.003 | +2.8 | within threshold |
| belt-sun/fullscreen | gpu_temporal_ms | +0.009 | +1.3 | within threshold |
| belt-sun/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_bloom_ms | -0.005 | -2.3 | within threshold |
| belt-sun/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_composite_ms | +0.004 | +0.6 | within threshold |
| belt-sun/fullscreen | gpu_spatial_aa_ms | +0.006 | +1.6 | within threshold |
| belt-sun/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_present_ms | +0.001 | +1.4 | within threshold |

## Compared with headless-baseline

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| earth/window | cpu_submit_and_wait_ms | +0.063 | +1.6 | within threshold |
| earth/window | gpu_ms | +0.018 | +0.6 | within threshold |
| earth/window | cpu_prepare_ms | +0.019 | +7.4 | within threshold |
| earth/window | gpu_cull_shadow_ms | +0.049 | +9.2 | within threshold |
| earth/window | gpu_surface_ms | +0.056 | +5.9 | within threshold |
| earth/window | gpu_atmosphere_ms | -0.136 | -13.4 | within threshold |
| earth/window | gpu_post_ms | +0.044 | +7.3 | within threshold |
| earth/window | gpu_cull_ms | +0.043 | +26.8 | within threshold |
| earth/window | gpu_body_shadow_ms | +0.001 | +4.4 | within threshold |
| earth/window | gpu_belt_light_ms | +0.006 | +1.7 | within threshold |
| earth/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_sky_ms | +0.007 | +3.4 | within threshold |
| earth/window | gpu_surface_bodies_ms | -0.008 | -2.5 | within threshold |
| earth/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_surface_splats_ms | +0.001 | +25.0 | within threshold |
| earth/window | gpu_surface_clouds_ms | +0.004 | +2.4 | within threshold |
| earth/window | gpu_atmospheres_ms | -0.032 | -4.0 | within threshold |
| earth/window | gpu_belt_dust_ms | -0.100 | -49.2 | within threshold |
| earth/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_temporal_ms | +0.018 | +9.5 | within threshold |
| earth/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/window | gpu_bloom_ms | +0.003 | +3.7 | within threshold |
| earth/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_composite_ms | +0.018 | +12.1 | within threshold |
| earth/window | gpu_spatial_aa_ms | +0.003 | +2.4 | within threshold |
| earth/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| earth/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | cpu_submit_and_wait_ms | -0.063 | -0.7 | within threshold |
| earth/fullscreen | gpu_ms | -0.090 | -1.1 | within threshold |
| earth/fullscreen | cpu_prepare_ms | +0.010 | +3.2 | within threshold |
| earth/fullscreen | gpu_cull_shadow_ms | +0.052 | +7.7 | within threshold |
| earth/fullscreen | gpu_surface_ms | +0.182 | +7.2 | within threshold |
| earth/fullscreen | gpu_atmosphere_ms | -0.511 | -17.4 | within threshold |
| earth/fullscreen | gpu_post_ms | +0.150 | +8.0 | within threshold |
| earth/fullscreen | gpu_cull_ms | +0.046 | +28.6 | within threshold |
| earth/fullscreen | gpu_body_shadow_ms | +0.001 | +6.7 | within threshold |
| earth/fullscreen | gpu_belt_light_ms | +0.008 | +1.7 | within threshold |
| earth/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_surface_prepass_ms | -0.001 | -33.3 | within threshold |
| earth/fullscreen | gpu_surface_sky_ms | +0.028 | +3.1 | within threshold |
| earth/fullscreen | gpu_surface_bodies_ms | -0.014 | -2.1 | within threshold |
| earth/fullscreen | gpu_surface_rocks_ms | +0.001 | +4.3 | within threshold |
| earth/fullscreen | gpu_surface_splats_ms | +0.001 | +33.3 | within threshold |
| earth/fullscreen | gpu_surface_clouds_ms | +0.010 | +2.8 | within threshold |
| earth/fullscreen | gpu_atmospheres_ms | -0.251 | -10.7 | within threshold |
| earth/fullscreen | gpu_belt_dust_ms | -0.258 | -45.2 | within threshold |
| earth/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_temporal_ms | +0.067 | +10.5 | within threshold |
| earth/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| earth/fullscreen | gpu_bloom_ms | +0.005 | +2.1 | within threshold |
| earth/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| earth/fullscreen | gpu_flare_ms | +0.001 | +20.0 | within threshold |
| earth/fullscreen | gpu_composite_ms | +0.062 | +12.2 | within threshold |
| earth/fullscreen | gpu_spatial_aa_ms | +0.013 | +3.5 | within threshold |
| earth/fullscreen | gpu_meter_ms | +0.001 | +7.1 | within threshold |
| earth/fullscreen | gpu_present_ms | +0.003 | +3.9 | within threshold |
| giant/window | cpu_submit_and_wait_ms | -0.561 | -11.8 | within threshold |
| giant/window | gpu_ms | -0.601 | -15.1 | within threshold |
| giant/window | cpu_prepare_ms | +0.115 | +45.0 | within threshold |
| giant/window | gpu_cull_shadow_ms | +0.059 | +10.1 | within threshold |
| giant/window | gpu_surface_ms | +0.048 | +5.2 | within threshold |
| giant/window | gpu_atmosphere_ms | -0.751 | -40.3 | within threshold |
| giant/window | gpu_post_ms | +0.044 | +7.4 | within threshold |
| giant/window | gpu_cull_ms | +0.052 | +25.4 | within threshold |
| giant/window | gpu_body_shadow_ms | +0.001 | +5.8 | within threshold |
| giant/window | gpu_belt_light_ms | +0.006 | +1.7 | within threshold |
| giant/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_surface_sky_ms | +0.009 | +3.8 | within threshold |
| giant/window | gpu_surface_bodies_ms | -0.017 | -5.6 | within threshold |
| giant/window | gpu_surface_rocks_ms | -0.001 | -2.6 | within threshold |
| giant/window | gpu_surface_splats_ms | +0.005 | +5.0 | within threshold |
| giant/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_atmospheres_ms | +0.001 | +0.2 | within threshold |
| giant/window | gpu_belt_dust_ms | -0.754 | -62.6 | within threshold |
| giant/window | gpu_splat_mask_ms | +0.001 | +0.9 | within threshold |
| giant/window | gpu_temporal_ms | +0.017 | +8.8 | within threshold |
| giant/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/window | gpu_bloom_ms | +0.003 | +3.8 | within threshold |
| giant/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| giant/window | gpu_composite_ms | +0.019 | +12.8 | within threshold |
| giant/window | gpu_spatial_aa_ms | +0.004 | +3.4 | within threshold |
| giant/window | gpu_meter_ms | +0.001 | +14.3 | within threshold |
| giant/window | gpu_present_ms | +0.001 | +4.0 | within threshold |
| giant/fullscreen | cpu_submit_and_wait_ms | -2.352 | -19.7 | within threshold |
| giant/fullscreen | gpu_ms | -2.227 | -20.2 | within threshold |
| giant/fullscreen | cpu_prepare_ms | -0.069 | -18.4 | within threshold |
| giant/fullscreen | gpu_cull_shadow_ms | +0.088 | +12.5 | within threshold |
| giant/fullscreen | gpu_surface_ms | +0.263 | +10.8 | regression |
| giant/fullscreen | gpu_atmosphere_ms | -2.723 | -45.7 | within threshold |
| giant/fullscreen | gpu_post_ms | +0.153 | +8.1 | within threshold |
| giant/fullscreen | gpu_cull_ms | +0.062 | +28.7 | within threshold |
| giant/fullscreen | gpu_body_shadow_ms | +0.001 | +6.5 | within threshold |
| giant/fullscreen | gpu_belt_light_ms | +0.120 | +32.4 | within threshold |
| giant/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_surface_sky_ms | +0.033 | +3.4 | within threshold |
| giant/fullscreen | gpu_surface_bodies_ms | +0.045 | +7.3 | within threshold |
| giant/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_surface_splats_ms | +0.011 | +6.2 | within threshold |
| giant/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_atmospheres_ms | -0.148 | -9.0 | within threshold |
| giant/fullscreen | gpu_belt_dust_ms | -2.592 | -62.8 | within threshold |
| giant/fullscreen | gpu_splat_mask_ms | +0.014 | +7.7 | within threshold |
| giant/fullscreen | gpu_temporal_ms | +0.068 | +10.3 | within threshold |
| giant/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| giant/fullscreen | gpu_bloom_ms | +0.007 | +2.9 | within threshold |
| giant/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_flare_ms | +0.001 | +9.1 | within threshold |
| giant/fullscreen | gpu_composite_ms | +0.065 | +12.6 | within threshold |
| giant/fullscreen | gpu_spatial_aa_ms | +0.015 | +4.2 | within threshold |
| giant/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| giant/fullscreen | gpu_present_ms | +0.003 | +4.1 | within threshold |
| moon/window | cpu_submit_and_wait_ms | +0.096 | +3.9 | within threshold |
| moon/window | gpu_ms | +0.024 | +1.3 | within threshold |
| moon/window | cpu_prepare_ms | +0.009 | +3.9 | within threshold |
| moon/window | gpu_cull_shadow_ms | -0.091 | -14.9 | within threshold |
| moon/window | gpu_surface_ms | +0.056 | +9.4 | within threshold |
| moon/window | gpu_atmosphere_ms | +0.012 | +20.7 | within threshold |
| moon/window | gpu_post_ms | +0.044 | +7.5 | within threshold |
| moon/window | gpu_cull_ms | +0.024 | +23.0 | within threshold |
| moon/window | gpu_body_shadow_ms | +0.001 | +6.3 | within threshold |
| moon/window | gpu_belt_light_ms | -0.058 | -13.6 | within threshold |
| moon/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_sky_ms | +0.009 | +4.2 | within threshold |
| moon/window | gpu_surface_bodies_ms | -0.005 | -3.7 | within threshold |
| moon/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| moon/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_belt_dust_ms | +0.013 | +25.0 | within threshold |
| moon/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_temporal_ms | +0.018 | +9.7 | within threshold |
| moon/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/window | gpu_bloom_ms | +0.002 | +2.7 | within threshold |
| moon/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_composite_ms | +0.018 | +12.1 | within threshold |
| moon/window | gpu_spatial_aa_ms | +0.004 | +3.1 | within threshold |
| moon/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| moon/fullscreen | cpu_submit_and_wait_ms | +0.277 | +5.2 | regression |
| moon/fullscreen | gpu_ms | +0.368 | +8.0 | regression |
| moon/fullscreen | cpu_prepare_ms | -0.069 | -19.4 | within threshold |
| moon/fullscreen | gpu_cull_shadow_ms | +0.035 | +5.6 | within threshold |
| moon/fullscreen | gpu_surface_ms | +0.189 | +10.8 | within threshold |
| moon/fullscreen | gpu_atmosphere_ms | -0.049 | -16.1 | within threshold |
| moon/fullscreen | gpu_post_ms | +0.150 | +8.0 | within threshold |
| moon/fullscreen | gpu_cull_ms | +0.025 | +22.7 | within threshold |
| moon/fullscreen | gpu_body_shadow_ms | +0.001 | +6.4 | within threshold |
| moon/fullscreen | gpu_belt_light_ms | +0.007 | +1.5 | within threshold |
| moon/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_surface_prepass_ms | +0.001 | +100.0 | within threshold |
| moon/fullscreen | gpu_surface_sky_ms | +0.024 | +2.6 | within threshold |
| moon/fullscreen | gpu_surface_bodies_ms | +0.001 | +0.4 | within threshold |
| moon/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_atmospheres_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_belt_dust_ms | -0.049 | -16.6 | within threshold |
| moon/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_temporal_ms | +0.066 | +10.5 | within threshold |
| moon/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| moon/fullscreen | gpu_bloom_ms | +0.004 | +1.7 | within threshold |
| moon/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_composite_ms | +0.061 | +12.1 | within threshold |
| moon/fullscreen | gpu_spatial_aa_ms | +0.014 | +3.6 | within threshold |
| moon/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| moon/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| mars/window | cpu_submit_and_wait_ms | +0.129 | +4.5 | within threshold |
| mars/window | gpu_ms | +0.130 | +6.1 | within threshold |
| mars/window | cpu_prepare_ms | +0.008 | +3.4 | within threshold |
| mars/window | gpu_cull_shadow_ms | +0.031 | +6.3 | within threshold |
| mars/window | gpu_surface_ms | +0.062 | +10.2 | within threshold |
| mars/window | gpu_atmosphere_ms | +0.006 | +1.4 | within threshold |
| mars/window | gpu_post_ms | +0.044 | +7.8 | within threshold |
| mars/window | gpu_cull_ms | +0.024 | +23.0 | within threshold |
| mars/window | gpu_body_shadow_ms | +0.001 | +3.9 | within threshold |
| mars/window | gpu_belt_light_ms | +0.006 | +1.7 | within threshold |
| mars/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_sky_ms | +0.010 | +4.1 | within threshold |
| mars/window | gpu_surface_bodies_ms | -0.001 | -0.8 | within threshold |
| mars/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_surface_splats_ms | -0.001 | -100.0 | within threshold |
| mars/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_atmospheres_ms | -0.006 | -1.6 | within threshold |
| mars/window | gpu_belt_dust_ms | +0.012 | +23.1 | within threshold |
| mars/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_temporal_ms | +0.020 | +11.1 | within threshold |
| mars/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/window | gpu_bloom_ms | +0.002 | +2.7 | within threshold |
| mars/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_composite_ms | +0.019 | +12.8 | within threshold |
| mars/window | gpu_spatial_aa_ms | +0.004 | +3.7 | within threshold |
| mars/window | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| mars/fullscreen | cpu_submit_and_wait_ms | +0.091 | +1.4 | within threshold |
| mars/fullscreen | gpu_ms | +0.238 | +4.2 | within threshold |
| mars/fullscreen | cpu_prepare_ms | -0.094 | -24.9 | within threshold |
| mars/fullscreen | gpu_cull_shadow_ms | +0.051 | +7.9 | within threshold |
| mars/fullscreen | gpu_surface_ms | +0.215 | +12.0 | regression |
| mars/fullscreen | gpu_atmosphere_ms | -0.257 | -17.5 | within threshold |
| mars/fullscreen | gpu_post_ms | +0.154 | +8.6 | within threshold |
| mars/fullscreen | gpu_cull_ms | +0.034 | +23.3 | within threshold |
| mars/fullscreen | gpu_body_shadow_ms | +0.001 | +3.9 | within threshold |
| mars/fullscreen | gpu_belt_light_ms | +0.129 | +35.1 | within threshold |
| mars/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_surface_prepass_ms | +0.001 | +100.0 | within threshold |
| mars/fullscreen | gpu_surface_sky_ms | +0.027 | +2.7 | within threshold |
| mars/fullscreen | gpu_surface_bodies_ms | +0.007 | +3.0 | within threshold |
| mars/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_atmospheres_ms | -0.182 | -16.1 | within threshold |
| mars/fullscreen | gpu_belt_dust_ms | -0.077 | -23.2 | within threshold |
| mars/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_temporal_ms | +0.070 | +11.4 | within threshold |
| mars/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| mars/fullscreen | gpu_bloom_ms | +0.004 | +1.7 | within threshold |
| mars/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_flare_ms | +0.001 | +9.1 | within threshold |
| mars/fullscreen | gpu_composite_ms | +0.062 | +12.3 | within threshold |
| mars/fullscreen | gpu_spatial_aa_ms | +0.013 | +4.0 | within threshold |
| mars/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| mars/fullscreen | gpu_present_ms | +0.003 | +4.1 | within threshold |
| dawn/window | cpu_submit_and_wait_ms | +0.122 | +3.5 | within threshold |
| dawn/window | gpu_ms | +0.129 | +4.6 | within threshold |
| dawn/window | cpu_prepare_ms | +0.009 | +3.6 | within threshold |
| dawn/window | gpu_cull_shadow_ms | +0.030 | +6.2 | within threshold |
| dawn/window | gpu_surface_ms | +0.052 | +6.9 | within threshold |
| dawn/window | gpu_atmosphere_ms | +0.001 | +0.1 | within threshold |
| dawn/window | gpu_post_ms | +0.047 | +7.4 | within threshold |
| dawn/window | gpu_cull_ms | +0.025 | +23.3 | within threshold |
| dawn/window | gpu_body_shadow_ms | +0.001 | +4.4 | within threshold |
| dawn/window | gpu_belt_light_ms | +0.006 | +1.7 | within threshold |
| dawn/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_sky_ms | +0.002 | +1.6 | within threshold |
| dawn/window | gpu_surface_bodies_ms | -0.006 | -1.8 | within threshold |
| dawn/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_surface_clouds_ms | +0.004 | +2.1 | within threshold |
| dawn/window | gpu_atmospheres_ms | -0.012 | -1.4 | within threshold |
| dawn/window | gpu_belt_dust_ms | +0.012 | +23.5 | within threshold |
| dawn/window | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_temporal_ms | +0.017 | +8.8 | within threshold |
| dawn/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/window | gpu_bloom_ms | +0.006 | +6.7 | within threshold |
| dawn/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| dawn/window | gpu_composite_ms | +0.019 | +12.3 | within threshold |
| dawn/window | gpu_spatial_aa_ms | +0.003 | +2.6 | within threshold |
| dawn/window | gpu_meter_ms | +0.001 | +14.3 | within threshold |
| dawn/window | gpu_present_ms | +0.001 | +4.2 | within threshold |
| dawn/fullscreen | cpu_submit_and_wait_ms | +0.025 | +0.3 | within threshold |
| dawn/fullscreen | gpu_ms | +0.179 | +2.5 | within threshold |
| dawn/fullscreen | cpu_prepare_ms | -0.081 | -21.3 | within threshold |
| dawn/fullscreen | gpu_cull_shadow_ms | +0.035 | +5.7 | within threshold |
| dawn/fullscreen | gpu_surface_ms | +0.179 | +8.9 | within threshold |
| dawn/fullscreen | gpu_atmosphere_ms | -0.254 | -9.1 | within threshold |
| dawn/fullscreen | gpu_post_ms | +0.147 | +7.8 | within threshold |
| dawn/fullscreen | gpu_cull_ms | +0.025 | +23.6 | within threshold |
| dawn/fullscreen | gpu_body_shadow_ms | +0.001 | +4.5 | within threshold |
| dawn/fullscreen | gpu_belt_light_ms | +0.126 | +33.8 | within threshold |
| dawn/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_sky_ms | +0.015 | +2.4 | within threshold |
| dawn/fullscreen | gpu_surface_bodies_ms | -0.002 | -0.3 | within threshold |
| dawn/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_splats_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_surface_clouds_ms | +0.011 | +2.4 | within threshold |
| dawn/fullscreen | gpu_atmospheres_ms | -0.300 | -11.5 | within threshold |
| dawn/fullscreen | gpu_belt_dust_ms | +0.045 | +26.0 | within threshold |
| dawn/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| dawn/fullscreen | gpu_temporal_ms | +0.065 | +10.0 | within threshold |
| dawn/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| dawn/fullscreen | gpu_bloom_ms | +0.007 | +3.3 | within threshold |
| dawn/fullscreen | gpu_sun_visibility_ms | +0.001 | +3.4 | within threshold |
| dawn/fullscreen | gpu_flare_ms | +0.001 | +4.5 | within threshold |
| dawn/fullscreen | gpu_composite_ms | +0.063 | +12.0 | within threshold |
| dawn/fullscreen | gpu_spatial_aa_ms | +0.012 | +3.5 | within threshold |
| dawn/fullscreen | gpu_meter_ms | +0.000 | +0.5 | within threshold |
| dawn/fullscreen | gpu_present_ms | +0.002 | +2.8 | within threshold |
| belt/window | cpu_submit_and_wait_ms | -0.142 | -3.1 | within threshold |
| belt/window | gpu_ms | -0.053 | -1.4 | within threshold |
| belt/window | cpu_prepare_ms | +0.001 | +0.3 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.050 | +8.8 | within threshold |
| belt/window | gpu_surface_ms | +0.113 | +10.9 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.251 | -16.6 | within threshold |
| belt/window | gpu_post_ms | +0.035 | +5.2 | within threshold |
| belt/window | gpu_cull_ms | +0.045 | +23.8 | within threshold |
| belt/window | gpu_body_shadow_ms | +0.001 | +5.5 | within threshold |
| belt/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| belt/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_sky_ms | +0.004 | +3.9 | within threshold |
| belt/window | gpu_surface_bodies_ms | +0.031 | +7.5 | within threshold |
| belt/window | gpu_surface_rocks_ms | -0.001 | -0.3 | within threshold |
| belt/window | gpu_surface_splats_ms | +0.001 | +2.1 | within threshold |
| belt/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_atmospheres_ms | +0.031 | +3.8 | within threshold |
| belt/window | gpu_belt_dust_ms | -0.284 | -43.6 | within threshold |
| belt/window | gpu_splat_mask_ms | +0.001 | +1.7 | within threshold |
| belt/window | gpu_temporal_ms | +0.015 | +7.4 | within threshold |
| belt/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/window | gpu_bloom_ms | +0.001 | +1.2 | within threshold |
| belt/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_flare_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_composite_ms | +0.018 | +11.6 | within threshold |
| belt/window | gpu_spatial_aa_ms | +0.004 | +2.6 | within threshold |
| belt/window | gpu_meter_ms | +0.001 | +14.3 | within threshold |
| belt/window | gpu_present_ms | +0.001 | +3.2 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | -0.244 | -2.1 | within threshold |
| belt/fullscreen | gpu_ms | -0.178 | -1.7 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.013 | +3.1 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.062 | +10.6 | within threshold |
| belt/fullscreen | gpu_surface_ms | +0.354 | +11.7 | regression |
| belt/fullscreen | gpu_atmosphere_ms | -0.834 | -17.7 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.138 | +6.7 | within threshold |
| belt/fullscreen | gpu_cull_ms | +0.053 | +26.5 | within threshold |
| belt/fullscreen | gpu_body_shadow_ms | +0.001 | +4.8 | within threshold |
| belt/fullscreen | gpu_belt_light_ms | +0.007 | +2.0 | within threshold |
| belt/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_sky_ms | +0.009 | +1.8 | within threshold |
| belt/fullscreen | gpu_surface_bodies_ms | +0.108 | +11.8 | within threshold |
| belt/fullscreen | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_splats_ms | +0.002 | +3.1 | within threshold |
| belt/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_atmospheres_ms | +0.106 | +4.3 | within threshold |
| belt/fullscreen | gpu_belt_dust_ms | -0.943 | -43.8 | within threshold |
| belt/fullscreen | gpu_splat_mask_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_temporal_ms | +0.055 | +8.0 | within threshold |
| belt/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt/fullscreen | gpu_bloom_ms | +0.001 | +0.2 | within threshold |
| belt/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_flare_ms | +0.001 | +4.5 | within threshold |
| belt/fullscreen | gpu_composite_ms | +0.063 | +11.9 | within threshold |
| belt/fullscreen | gpu_spatial_aa_ms | +0.013 | +3.1 | within threshold |
| belt/fullscreen | gpu_meter_ms | +0.000 | +1.8 | within threshold |
| belt/fullscreen | gpu_present_ms | +0.002 | +2.7 | within threshold |
| belt-sun/window | cpu_submit_and_wait_ms | -0.254 | -7.6 | within threshold |
| belt-sun/window | gpu_ms | -0.258 | -9.8 | within threshold |
| belt-sun/window | cpu_prepare_ms | +0.010 | +3.8 | within threshold |
| belt-sun/window | gpu_cull_shadow_ms | +0.050 | +8.9 | within threshold |
| belt-sun/window | gpu_surface_ms | +0.088 | +16.8 | within threshold |
| belt-sun/window | gpu_atmosphere_ms | -0.436 | -47.4 | within threshold |
| belt-sun/window | gpu_post_ms | +0.041 | +6.7 | within threshold |
| belt-sun/window | gpu_cull_ms | +0.044 | +23.3 | within threshold |
| belt-sun/window | gpu_body_shadow_ms | +0.001 | +5.5 | within threshold |
| belt-sun/window | gpu_belt_light_ms | +0.005 | +1.4 | within threshold |
| belt-sun/window | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_sky_ms | +0.004 | +1.8 | within threshold |
| belt-sun/window | gpu_surface_bodies_ms | -0.001 | -50.0 | within threshold |
| belt-sun/window | gpu_surface_rocks_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_surface_splats_ms | +0.005 | +7.1 | within threshold |
| belt-sun/window | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_atmospheres_ms | -0.085 | -65.4 | within threshold |
| belt-sun/window | gpu_belt_dust_ms | -0.353 | -49.7 | within threshold |
| belt-sun/window | gpu_splat_mask_ms | +0.001 | +1.3 | within threshold |
| belt-sun/window | gpu_temporal_ms | +0.017 | +9.1 | within threshold |
| belt-sun/window | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/window | gpu_bloom_ms | +0.001 | +1.4 | within threshold |
| belt-sun/window | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/window | gpu_flare_ms | +0.001 | +20.0 | within threshold |
| belt-sun/window | gpu_composite_ms | +0.018 | +12.0 | within threshold |
| belt-sun/window | gpu_spatial_aa_ms | +0.003 | +2.6 | within threshold |
| belt-sun/window | gpu_meter_ms | +0.000 | +5.4 | within threshold |
| belt-sun/window | gpu_present_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | cpu_submit_and_wait_ms | -0.847 | -10.6 | within threshold |
| belt-sun/fullscreen | gpu_ms | -0.816 | -11.4 | within threshold |
| belt-sun/fullscreen | cpu_prepare_ms | +0.017 | +5.4 | within threshold |
| belt-sun/fullscreen | gpu_cull_shadow_ms | +0.060 | +8.5 | within threshold |
| belt-sun/fullscreen | gpu_surface_ms | +0.292 | +17.1 | regression |
| belt-sun/fullscreen | gpu_atmosphere_ms | -1.333 | -46.9 | within threshold |
| belt-sun/fullscreen | gpu_post_ms | +0.143 | +7.6 | within threshold |
| belt-sun/fullscreen | gpu_cull_ms | +0.054 | +27.1 | within threshold |
| belt-sun/fullscreen | gpu_body_shadow_ms | +0.001 | +4.6 | within threshold |
| belt-sun/fullscreen | gpu_belt_light_ms | +0.006 | +1.1 | within threshold |
| belt-sun/fullscreen | gpu_belt_disc_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_surface_prepass_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_sky_ms | +0.018 | +2.1 | within threshold |
| belt-sun/fullscreen | gpu_surface_bodies_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_surface_rocks_ms | +0.001 | +0.2 | within threshold |
| belt-sun/fullscreen | gpu_surface_splats_ms | +0.006 | +5.5 | within threshold |
| belt-sun/fullscreen | gpu_surface_clouds_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_atmospheres_ms | -0.281 | -49.7 | within threshold |
| belt-sun/fullscreen | gpu_belt_dust_ms | -1.063 | -48.9 | within threshold |
| belt-sun/fullscreen | gpu_splat_mask_ms | +0.010 | +9.9 | within threshold |
| belt-sun/fullscreen | gpu_temporal_ms | +0.068 | +10.8 | within threshold |
| belt-sun/fullscreen | gpu_streaks_ms | +0.000 | n/a | within threshold |
| belt-sun/fullscreen | gpu_bloom_ms | -0.001 | -0.5 | within threshold |
| belt-sun/fullscreen | gpu_sun_visibility_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_flare_ms | +0.001 | +7.1 | within threshold |
| belt-sun/fullscreen | gpu_composite_ms | +0.061 | +11.9 | within threshold |
| belt-sun/fullscreen | gpu_spatial_aa_ms | +0.012 | +3.2 | within threshold |
| belt-sun/fullscreen | gpu_meter_ms | +0.000 | +0.0 | within threshold |
| belt-sun/fullscreen | gpu_present_ms | +0.002 | +2.7 | within threshold |
