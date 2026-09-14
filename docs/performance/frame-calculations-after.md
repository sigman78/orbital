# Render performance: frame-calculations-after

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `bc93b9487c7faf769eec3147daf0e0e00d3e70f1`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 6.006 | 5.253 | 5.695 | 0.576 | 2.266 | 1.627 | 0.635 |
| belt/fullscreen | 3440×1440 | 15.365 | 14.570 | 24.365 | 0.595 | 5.924 | 5.638 | 2.411 |

## Compared with frame-calculations-before

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | +0.045 | +0.7 | within threshold |
| belt/window | gpu_ms | -0.061 | -1.1 | within threshold |
| belt/window | cpu_prepare_ms | -0.026 | -7.9 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.002 | +0.3 | within threshold |
| belt/window | gpu_surface_ms | +0.031 | +1.4 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.084 | -4.9 | within threshold |
| belt/window | gpu_post_ms | +0.004 | +0.6 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.074 | +0.5 | within threshold |
| belt/fullscreen | gpu_ms | +0.161 | +1.1 | within threshold |
| belt/fullscreen | cpu_prepare_ms | -0.067 | -15.9 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_ms | -0.032 | -0.5 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | -0.001 | -0.0 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.040 | +1.7 | within threshold |

## Compared with packed-culling

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | -0.002 | -0.0 | within threshold |
| belt/window | gpu_ms | -0.111 | -2.1 | within threshold |
| belt/window | cpu_prepare_ms | +0.039 | +15.0 | within threshold |
| belt/window | gpu_cull_shadow_ms | -0.009 | -1.6 | within threshold |
| belt/window | gpu_surface_ms | -0.062 | -2.7 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.105 | -6.1 | within threshold |
| belt/window | gpu_post_ms | -0.006 | -1.0 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.073 | +0.5 | within threshold |
| belt/fullscreen | gpu_ms | -0.001 | -0.0 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.049 | +16.1 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | -0.007 | -1.2 | within threshold |
| belt/fullscreen | gpu_surface_ms | +0.013 | +0.2 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.023 | +0.4 | within threshold |
| belt/fullscreen | gpu_post_ms | -0.056 | -2.3 | within threshold |
