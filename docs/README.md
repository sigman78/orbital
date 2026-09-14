# Documentation

| Document | Purpose |
| --- | --- |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Layers, scene, belt, frame sequence, resources, application, tools and tests, as implemented |
| [DECISIONS.md](DECISIONS.md) | The engineering log: every rendering decision with the measurements behind it |
| [FOUNDATION.md](FOUNDATION.md) | The NoGraphicsAPI compatibility backend, shader ABI, toolchain pins and validation |
| [ASSETS.md](ASSETS.md) | Sourced material maps: provenance, processing and attribution |
| [GALAXY_LAYERS.md](GALAXY_LAYERS.md) | Gaia background research: layer budgets, original comparison, implementation and cleanup after selection |
| [BLOOM_STABILITY.md](BLOOM_STABILITY.md) | Bloom sampling and sun-occlusion fixes, reproduction view and GPU regression checks |
| [PERFORMANCE.md](PERFORMANCE.md) | Fixed-scene benchmark suite, timing definitions and cumulative regression comparisons |
| [RENDER_VALIDATION.md](RENDER_VALIDATION.md) | Pinned Vulkan validation tooling, renderer correctness tests and motion/belt investigation |
| [CODE_STYLE.md](CODE_STYLE.md) | C++ conventions: language level, error handling, logging, ownership, layering |

Record design changes in DECISIONS.md with what was measured; keep ARCHITECTURE.md describing what exists,
not what is planned.
