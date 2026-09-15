# Render performance: session-followups-belt

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `0db771e205c37a2b53ce4705337e6439ea064d4e`. Dirty source: False.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 6.326 | 5.642 | 6.286 | 0.589 | 2.245 | 1.743 | 0.797 |
| belt/fullscreen | 3440×1440 | 16.197 | 15.408 | 19.671 | 0.607 | 6.359 | 6.094 | 2.533 |

## Compared with gpu-scopes-after

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | +0.348 | +5.8 | regression |
| belt/window | gpu_ms | +0.365 | +6.9 | regression |
| belt/window | cpu_prepare_ms | -0.019 | -6.2 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.004 | +0.7 | within threshold |
| belt/window | gpu_surface_ms | +0.059 | +2.7 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.042 | -2.4 | within threshold |
| belt/window | gpu_post_ms | +0.156 | +24.3 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.822 | +5.3 | regression |
| belt/fullscreen | gpu_ms | +0.880 | +6.1 | regression |
| belt/fullscreen | cpu_prepare_ms | -0.033 | -8.7 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.005 | +0.9 | within threshold |
| belt/fullscreen | gpu_surface_ms | +0.468 | +7.9 | regression |
| belt/fullscreen | gpu_atmosphere_ms | +0.449 | +7.9 | regression |
| belt/fullscreen | gpu_post_ms | +0.051 | +2.1 | within threshold |
