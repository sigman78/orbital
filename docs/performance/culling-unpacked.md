# Render performance: unpacked-culling

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `fb24e47f62541463642a7151dbe010f7165ae6c8`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 6.003 | 5.385 | 5.695 | 0.584 | 2.483 | 1.714 | 0.636 |
| belt/fullscreen | 3440×1440 | 15.114 | 14.347 | 19.148 | 0.608 | 5.822 | 5.568 | 2.403 |
