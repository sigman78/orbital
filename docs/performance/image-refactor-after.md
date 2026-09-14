# Render performance: refactor-image-after

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `d1e1e0b4346c545f0f7457a06daa87c8313d7e04`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 6.030 | 5.443 | 5.733 | 0.586 | 2.465 | 1.616 | 0.643 |
| belt/fullscreen | 3440×1440 | 15.228 | 14.510 | 19.053 | 0.604 | 5.982 | 5.893 | 2.059 |

## Compared with refactor-image-before

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | +0.018 | +0.3 | within threshold |
| belt/window | gpu_ms | +0.017 | +0.3 | within threshold |
| belt/window | cpu_prepare_ms | -0.005 | -1.8 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.004 | +0.7 | within threshold |
| belt/window | gpu_surface_ms | -0.136 | -5.2 | within threshold |
| belt/window | gpu_atmosphere_ms | +0.016 | +1.0 | within threshold |
| belt/window | gpu_post_ms | +0.005 | +0.8 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | -0.113 | -0.7 | within threshold |
| belt/fullscreen | gpu_ms | -0.081 | -0.6 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.006 | +2.1 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | -0.000 | -0.0 | within threshold |
| belt/fullscreen | gpu_surface_ms | -0.126 | -2.1 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.002 | +0.0 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.003 | +0.1 | within threshold |
