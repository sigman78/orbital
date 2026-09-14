# Render performance: gpu-scopes-after

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `79ccaad9edc2146ea42a0bb978b1258412a8ec92`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 5.977 | 5.278 | 5.721 | 0.585 | 2.186 | 1.786 | 0.641 |
| belt/fullscreen | 3440×1440 | 15.375 | 14.528 | 18.794 | 0.602 | 5.891 | 5.646 | 2.482 |

## Compared with gpu-scopes-before

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | -0.005 | -0.1 | within threshold |
| belt/window | gpu_ms | +0.053 | +1.0 | within threshold |
| belt/window | cpu_prepare_ms | -0.053 | -14.6 | within threshold |
| belt/window | gpu_cull_shadow_ms | -0.001 | -0.1 | within threshold |
| belt/window | gpu_surface_ms | +0.011 | +0.5 | within threshold |
| belt/window | gpu_atmosphere_ms | +0.055 | +3.2 | within threshold |
| belt/window | gpu_post_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.118 | +0.8 | within threshold |
| belt/fullscreen | gpu_ms | +0.087 | +0.6 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.032 | +9.4 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | -0.000 | -0.1 | within threshold |
| belt/fullscreen | gpu_surface_ms | +0.048 | +0.8 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.044 | +0.8 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.004 | +0.1 | within threshold |

## Compared with packed-culling

Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.

| Scene / mode | Metric | Change, ms | Change, % | Assessment |
| --- | --- | ---: | ---: | --- |
| belt/window | cpu_submit_and_wait_ms | -0.031 | -0.5 | within threshold |
| belt/window | gpu_ms | -0.087 | -1.6 | within threshold |
| belt/window | cpu_prepare_ms | +0.047 | +18.1 | within threshold |
| belt/window | gpu_cull_shadow_ms | +0.000 | +0.0 | within threshold |
| belt/window | gpu_surface_ms | -0.142 | -6.1 | within threshold |
| belt/window | gpu_atmosphere_ms | +0.053 | +3.1 | within threshold |
| belt/window | gpu_post_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | cpu_submit_and_wait_ms | +0.083 | +0.5 | within threshold |
| belt/fullscreen | gpu_ms | -0.042 | -0.3 | within threshold |
| belt/fullscreen | cpu_prepare_ms | +0.073 | +23.7 | within threshold |
| belt/fullscreen | gpu_cull_shadow_ms | +0.000 | +0.0 | within threshold |
| belt/fullscreen | gpu_surface_ms | -0.020 | -0.3 | within threshold |
| belt/fullscreen | gpu_atmosphere_ms | +0.031 | +0.6 | within threshold |
| belt/fullscreen | gpu_post_ms | +0.015 | +0.6 | within threshold |
