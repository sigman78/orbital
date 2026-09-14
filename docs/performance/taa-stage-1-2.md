# Render performance: taa-stage-1-2

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `c4bbb606faaa4fd644f1fda08c63748475929cc1`. Dirty source: True.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| earth/window | 1600×900 | 4.540 | 3.906 | 4.399 | 0.548 | 1.375 | 1.355 | 0.599 |
| earth/fullscreen | 3440×1440 | 11.080 | 10.203 | 13.963 | 0.556 | 3.794 | 3.645 | 2.051 |
| giant/window | 1600×900 | 5.677 | 4.996 | 5.366 | 0.602 | 1.380 | 2.410 | 0.590 |
| giant/fullscreen | 3440×1440 | 14.197 | 13.346 | 18.143 | 0.626 | 3.539 | 7.047 | 2.052 |
| moon/window | 1600×900 | 2.852 | 2.178 | 2.561 | 0.499 | 0.902 | 0.182 | 0.584 |
| moon/fullscreen | 3440×1440 | 7.014 | 6.148 | 6.714 | 0.516 | 2.531 | 0.918 | 2.052 |
| mars/window | 1600×900 | 3.275 | 2.685 | 3.178 | 0.501 | 0.907 | 0.514 | 0.719 |
| mars/fullscreen | 3440×1440 | 8.072 | 7.311 | 8.165 | 0.543 | 2.606 | 2.109 | 1.983 |
| dawn/window | 1600×900 | 4.410 | 3.713 | 4.192 | 0.499 | 1.371 | 1.152 | 0.637 |
| dawn/fullscreen | 3440×1440 | 10.589 | 9.724 | 12.510 | 0.506 | 3.560 | 3.501 | 2.185 |
| belt/window | 1600×900 | 5.975 | 5.309 | 5.830 | 0.586 | 2.241 | 1.634 | 0.644 |
| belt/fullscreen | 3440×1440 | 15.262 | 14.385 | 23.380 | 0.611 | 5.793 | 5.606 | 2.496 |
| belt-sun/window | 1600×900 | 3.829 | 3.124 | 3.499 | 0.599 | 0.714 | 1.168 | 0.614 |
| belt-sun/fullscreen | 3440×1440 | 9.215 | 8.409 | 9.414 | 0.613 | 2.296 | 3.297 | 2.258 |
