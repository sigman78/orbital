# Render performance: gpu-scopes-before

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `79ccaad9edc2146ea42a0bb978b1258412a8ec92`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 5.982 | 5.225 | 5.634 | 0.585 | 2.175 | 1.731 | 0.641 |
| belt/fullscreen | 3440×1440 | 15.257 | 14.441 | 18.984 | 0.602 | 5.843 | 5.602 | 2.479 |
