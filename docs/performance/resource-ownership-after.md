# Render performance: resource-ownership-after

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `f213b919eac10c1391d27854c77d1594d2a932b6`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 6.019 | 5.291 | 5.677 | 0.575 | 2.309 | 1.729 | 0.633 |
| belt/fullscreen | 3440×1440 | 13.826 | 13.115 | 22.568 | 0.593 | 5.416 | 5.076 | 2.024 |

## Compared with resource-ownership-before

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | +0.701 | +13.2 | regression |
| belt/window | gpu_ms | +0.602 | +12.8 | noisy increase |
| belt/window | cpu_prepare_ms | +0.040 | +16.1 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.004 | +0.7 | within threshold |
| belt/window | gpu_surface_ms | +0.423 | +22.5 | noisy increase |
| belt/window | gpu_atmosphere_ms | +0.134 | +8.4 | within threshold |
| belt/window | gpu_post_ms | +0.004 | +0.7 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | -0.083 | -0.6 | within threshold |
| belt/fullscreen | gpu_ms | +0.064 | +0.5 | within threshold |
| belt/fullscreen | cpu_prepare_ms | -0.017 | -4.7 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.004 | +0.6 | within threshold |
| belt/fullscreen | gpu_surface_ms | -0.040 | -0.7 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.055 | +1.1 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.017 | +0.8 | within threshold |
