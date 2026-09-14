# Render performance: packed-culling

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `fb24e47f62541463642a7151dbe010f7165ae6c8`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 6.008 | 5.365 | 5.770 | 0.585 | 2.328 | 1.733 | 0.641 |
| belt/fullscreen | 3440×1440 | 15.291 | 14.570 | 20.305 | 0.602 | 5.911 | 5.615 | 2.467 |

## Compared with unpacked-culling

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | +0.005 | +0.1 | within threshold |
| belt/window | gpu_ms | -0.020 | -0.4 | within threshold |
| belt/window | cpu_prepare_ms | -0.002 | -0.7 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.001 | +0.2 | within threshold |
| belt/window | gpu_surface_ms | -0.156 | -6.3 | within threshold |
| belt/window | gpu_atmosphere_ms | +0.018 | +1.1 | within threshold |
| belt/window | gpu_post_ms | +0.005 | +0.8 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.177 | +1.2 | within threshold |
| belt/fullscreen | gpu_ms | +0.223 | +1.6 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.004 | +1.3 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | -0.006 | -1.0 | within threshold |
| belt/fullscreen | gpu_surface_ms | +0.089 | +1.5 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.047 | +0.8 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.064 | +2.7 | within threshold |
