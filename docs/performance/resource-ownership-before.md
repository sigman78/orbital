# Render performance: resource-ownership-before

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `f213b919eac10c1391d27854c77d1594d2a932b6`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 5.318 | 4.689 | 4.858 | 0.571 | 1.885 | 1.595 | 0.629 |
| belt/fullscreen | 3440×1440 | 13.910 | 13.051 | 22.271 | 0.589 | 5.456 | 5.021 | 2.007 |
