# Render performance: mapped-culling-reference

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `fb24e47f62541463642a7151dbe010f7165ae6c8`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 14.723 | 14.172 | 18.608 | 8.813 | 2.657 | 1.857 | 0.645 |
| belt/fullscreen | 3440×1440 | 27.570 | 26.965 | 31.309 | 13.215 | 6.200 | 5.312 | 2.018 |
