# Render performance: app-render-after

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `2df853dc9c9c91c87c554a1ca01763df00c6c97c`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 5.989 | 5.247 | 5.659 | 0.575 | 2.223 | 1.611 | 0.632 |
| belt/fullscreen | 3440×1440 | 15.305 | 14.479 | 24.233 | 0.592 | 5.876 | 5.635 | 2.439 |

## Compared with app-render-before

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | -0.026 | -0.4 | within threshold |
| belt/window | gpu_ms | -0.119 | -2.2 | within threshold |
| belt/window | cpu_prepare_ms | +0.028 | +10.2 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.003 | +0.5 | within threshold |
| belt/window | gpu_surface_ms | -0.228 | -9.3 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.103 | -6.0 | within threshold |
| belt/window | gpu_post_ms | +0.003 | +0.4 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.070 | +0.5 | within threshold |
| belt/fullscreen | gpu_ms | +0.105 | +0.7 | within threshold |
| belt/fullscreen | cpu_prepare_ms | -0.049 | -12.5 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.001 | +0.1 | within threshold |
| belt/fullscreen | gpu_surface_ms | -0.014 | -0.2 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.060 | +1.1 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.018 | +0.8 | within threshold |
