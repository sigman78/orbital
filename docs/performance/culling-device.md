# Render performance: device-only-culling

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `fb24e47f62541463642a7151dbe010f7165ae6c8`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 6.014 | 5.414 | 5.684 | 0.589 | 2.577 | 1.609 | 0.640 |
| belt/fullscreen | 3440×1440 | 15.173 | 14.434 | 18.851 | 0.614 | 5.961 | 5.841 | 2.047 |

## Compared with mapped-culling-reference

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | -8.709 | -59.2 | within threshold |
| belt/window | gpu_ms | -8.758 | -61.8 | within threshold |
| belt/window | cpu_prepare_ms | +0.100 | +60.8 | within threshold |
| belt/window | gpu_cull_shadow_ms | -8.224 | -93.3 | within threshold |
| belt/window | gpu_surface_ms | -0.080 | -3.0 | within threshold |
| belt/window | gpu_atmosphere_ms | -0.248 | -13.4 | within threshold |
| belt/window | gpu_post_ms | -0.005 | -0.8 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | -12.397 | -45.0 | within threshold |
| belt/fullscreen | gpu_ms | -12.531 | -46.5 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.129 | +73.4 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | -12.601 | -95.4 | within threshold |
| belt/fullscreen | gpu_surface_ms | -0.239 | -3.8 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.529 | +10.0 | regression |
| belt/fullscreen | gpu_post_ms | +0.029 | +1.4 | within threshold |
