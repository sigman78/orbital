# Render performance: app-cleanup

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `c9cba729e5993399371715ed2adc24ae42d14020`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 6.008 | 5.237 | 5.712 | 0.584 | 2.180 | 1.812 | 0.640 |
| belt/fullscreen | 3440×1440 | 15.272 | 14.512 | 19.281 | 0.602 | 5.856 | 5.644 | 2.476 |

## Compared with frame-calculations-after

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | +0.002 | +0.0 | within threshold |
| belt/window | gpu_ms | -0.016 | -0.3 | within threshold |
| belt/window | cpu_prepare_ms | +0.022 | +7.4 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.009 | +1.5 | within threshold |
| belt/window | gpu_surface_ms | -0.086 | -3.8 | within threshold |
| belt/window | gpu_atmosphere_ms | +0.185 | +11.4 | within threshold |
| belt/window | gpu_post_ms | +0.005 | +0.8 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | -0.092 | -0.6 | within threshold |
| belt/fullscreen | gpu_ms | -0.057 | -0.4 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.014 | +3.9 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.007 | +1.2 | within threshold |
| belt/fullscreen | gpu_surface_ms | -0.068 | -1.1 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.007 | +0.1 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.065 | +2.7 | within threshold |

## Compared with packed-culling

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | -0.000 | -0.0 | within threshold |
| belt/window | gpu_ms | -0.127 | -2.4 | within threshold |
| belt/window | cpu_prepare_ms | +0.062 | +23.5 | within threshold |
| belt/window | gpu_cull_shadow_ms | -0.000 | -0.0 | within threshold |
| belt/window | gpu_surface_ms | -0.147 | -6.3 | within threshold |
| belt/window | gpu_atmosphere_ms | +0.079 | +4.6 | within threshold |
| belt/window | gpu_post_ms | -0.001 | -0.2 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | -0.019 | -0.1 | within threshold |
| belt/fullscreen | gpu_ms | -0.058 | -0.4 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.063 | +20.6 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | -0.000 | -0.0 | within threshold |
| belt/fullscreen | gpu_surface_ms | -0.055 | -0.9 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.030 | +0.5 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.009 | +0.4 | within threshold |
