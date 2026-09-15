# Documentation

| Document | Purpose |
| --- | --- |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Layers, scene, belt, frame sequence, resources, application, tools and tests, as implemented |
| [DECISIONS.md](DECISIONS.md) | The engineering log: every rendering decision with the measurements behind it |
| [PLANS-2026-09-15.md](PLANS-2026-09-15.md) | Plans as of that date: immediate tasks and the longer horizon that brings the vision together |
| [FOUNDATION.md](FOUNDATION.md) | The NoGraphicsAPI compatibility backend, shader ABI, toolchain pins and validation |
| [ASSETS.md](ASSETS.md) | Sourced material maps: provenance, processing and attribution |
| [GALAXY_LAYERS.md](GALAXY_LAYERS.md) | Gaia background research: layer budgets, original comparison, implementation and cleanup after selection |
| [BLOOM_STABILITY.md](BLOOM_STABILITY.md) | Bloom sampling and sun-occlusion fixes, reproduction view and GPU regression checks |
| [PERFORMANCE.md](PERFORMANCE.md) | Fixed-scene benchmark suite, timing definitions and cumulative regression comparisons |
| [RENDER_VALIDATION.md](RENDER_VALIDATION.md) | Pinned Vulkan validation tooling, renderer correctness tests and motion/belt investigation |
| [CODE_STYLE.md](CODE_STYLE.md) | C++ conventions: language level, error handling, logging, ownership, layering |
| [CODE_QUALITY_REVIEW.md](CODE_QUALITY_REVIEW.md) | Scene/renderer/assets review: correctness findings, API contracts, ownership and complexity |
| [REFACTOR_PLAN.md](REFACTOR_PLAN.md) | Immediate defect fixes, validation evidence and staged scene/renderer/assets refactors |

Record design changes in DECISIONS.md with what was measured; keep ARCHITECTURE.md describing what exists,
not what is planned.
