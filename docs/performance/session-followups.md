# Render performance: session-followups

GPU: NVIDIA GeForce GTX 1080 Ti | conventional NoGraphicsAPI backend. Revision: `0db771e205c37a2b53ce4705337e6439ea064d4e`. Dirty source: False.

Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.

| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| earth/window | 1600×900 | 4.859 | 4.212 | 4.625 | 0.555 | 1.489 | 1.255 | 0.609 |
| earth/fullscreen | 3440×1440 | 11.529 | 10.704 | 14.756 | 0.563 | 3.981 | 3.832 | 2.083 |
| giant/window | 1600×900 | 5.884 | 5.213 | 5.599 | 0.601 | 1.626 | 2.094 | 0.600 |
| giant/fullscreen | 3440×1440 | 14.877 | 14.048 | 19.542 | 0.617 | 3.697 | 7.361 | 2.403 |
| moon/window | 1600×900 | 3.155 | 2.361 | 3.072 | 0.509 | 0.900 | 0.183 | 0.710 |
| moon/fullscreen | 3440×1440 | 7.240 | 6.527 | 7.439 | 0.525 | 2.484 | 0.937 | 2.189 |
| mars/window | 1600×900 | 3.587 | 2.854 | 3.367 | 0.510 | 0.914 | 0.623 | 0.573 |
| mars/fullscreen | 3440×1440 | 8.267 | 7.514 | 9.873 | 0.552 | 2.501 | 2.223 | 1.979 |
| dawn/window | 1600×900 | 4.798 | 4.174 | 4.555 | 0.507 | 1.499 | 0.981 | 0.644 |
| dawn/fullscreen | 3440×1440 | 11.005 | 10.174 | 13.543 | 0.513 | 3.782 | 3.613 | 2.226 |
| belt/window | 1600×900 | 6.326 | 5.642 | 6.286 | 0.589 | 2.245 | 1.743 | 0.797 |
| belt/fullscreen | 3440×1440 | 16.197 | 15.408 | 19.671 | 0.607 | 6.359 | 6.094 | 2.533 |
| belt-sun/window | 1600×900 | 4.185 | 3.438 | 3.981 | 0.724 | 0.588 | 0.980 | 0.613 |
| belt-sun/fullscreen | 3440×1440 | 9.798 | 8.915 | 10.596 | 0.738 | 2.106 | 3.440 | 2.403 |
