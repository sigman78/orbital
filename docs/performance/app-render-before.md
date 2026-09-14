# Render performance: app-render-before

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `2df853dc9c9c91c87c554a1ca01763df00c6c97c`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 6.014 | 5.366 | 5.697 | 0.572 | 2.451 | 1.714 | 0.630 |
| belt/fullscreen | 3440×1440 | 15.236 | 14.374 | 24.303 | 0.592 | 5.891 | 5.575 | 2.420 |
