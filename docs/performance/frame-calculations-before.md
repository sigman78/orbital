# Render performance: frame-calculations-before

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `bc93b9487c7faf769eec3147daf0e0e00d3e70f1`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 5.962 | 5.314 | 5.694 | 0.574 | 2.235 | 1.711 | 0.631 |
| belt/fullscreen | 3440×1440 | 15.290 | 14.408 | 24.179 | 0.595 | 5.956 | 5.639 | 2.371 |
