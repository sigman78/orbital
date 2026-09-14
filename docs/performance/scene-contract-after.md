# Render performance: scene-contract-after

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `b6b1c22ed08377a96ba100b8823c251887196af3`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 5.977 | 5.257 | 5.728 | 0.574 | 2.382 | 1.617 | 0.632 |
| belt/fullscreen | 3440×1440 | 15.261 | 14.368 | 24.464 | 0.595 | 5.866 | 5.604 | 2.391 |

## Compared with scene-contract-before

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | -0.027 | -0.5 | within threshold |
| belt/window | gpu_ms | -0.103 | -1.9 | within threshold |
| belt/window | cpu_prepare_ms | +0.004 | +1.3 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.000 | +0.1 | within threshold |
| belt/window | gpu_surface_ms | -0.087 | -3.5 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.017 | -1.1 | within threshold |
| belt/window | gpu_post_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | -0.046 | -0.3 | within threshold |
| belt/fullscreen | gpu_ms | -0.169 | -1.2 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.019 | +5.0 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.001 | +0.2 | within threshold |
| belt/fullscreen | gpu_surface_ms | -0.063 | -1.1 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.009 | +0.2 | within threshold |
| belt/fullscreen | gpu_post_ms | -0.039 | -1.6 | within threshold |

## Compared with packed-culling

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | -0.032 | -0.5 | within threshold |
| belt/window | gpu_ms | -0.107 | -2.0 | within threshold |
| belt/window | cpu_prepare_ms | +0.017 | +6.3 | within threshold |
| belt/window | gpu_cull_shadow_ms | -0.011 | -1.8 | within threshold |
| belt/window | gpu_surface_ms | +0.054 | +2.3 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.116 | -6.7 | within threshold |
| belt/window | gpu_post_ms | -0.009 | -1.4 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | -0.031 | -0.2 | within threshold |
| belt/fullscreen | gpu_ms | -0.203 | -1.4 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.086 | +28.1 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | -0.007 | -1.2 | within threshold |
| belt/fullscreen | gpu_surface_ms | -0.045 | -0.8 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | -0.010 | -0.2 | within threshold |
| belt/fullscreen | gpu_post_ms | -0.076 | -3.1 | within threshold |
