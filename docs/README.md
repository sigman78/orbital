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
| [CODE_QUALITY_REVIEW.md](CODE_QUALITY_REVIEW.md) | Scene/renderer/assets review: correctness findings, API contracts, ownership and complexity |
| [REFACTOR_PLAN.md](REFACTOR_PLAN.md) | Immediate defect fixes, validation evidence and staged scene/renderer/assets refactors |
| [SHADER_LIBRARY_REVIEW.md](SHADER_LIBRARY_REVIEW.md) | Shader correctness, composability, contracts, and staged refactoring for procedural planetary systems |
| [BACKEND_TEXTURE_ARRAYS.md](BACKEND_TEXTURE_ARRAYS.md) | Handoff design: texture array descriptors in the vendored GPU layer, goal, acceptance criteria, tasks |
| [DYNAMIC_TERRAIN.md](DYNAMIC_TERRAIN.md) | Handoff design: the minor planet's near tier as a CDLOD cube sphere with tiles in texture arrays |

Record design changes in DECISIONS.md with what was measured; keep ARCHITECTURE.md describing what exists,
not what is planned.
