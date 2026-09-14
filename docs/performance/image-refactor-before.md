# Render performance: refactor-image-before

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `d1e1e0b4346c545f0f7457a06daa87c8313d7e04`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 6.012 | 5.426 | 5.628 | 0.582 | 2.601 | 1.599 | 0.638 |
| belt/fullscreen | 3440×1440 | 15.341 | 14.591 | 19.512 | 0.604 | 6.108 | 5.891 | 2.057 |
