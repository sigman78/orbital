# Render performance: scene-contract-before

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `b6b1c22ed08377a96ba100b8823c251887196af3`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| belt/window | 1600×900 | 6.004 | 5.360 | 5.678 | 0.574 | 2.468 | 1.634 | 0.632 |
| belt/fullscreen | 3440×1440 | 15.307 | 14.537 | 24.232 | 0.594 | 5.929 | 5.595 | 2.430 |
