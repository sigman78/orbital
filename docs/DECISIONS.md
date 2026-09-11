# Decisions and unresolved questions

## Accepted direction

| ID | Decision | Reason |
| --- | --- | --- |
| D01 | Native Windows desktop, C++ and CMake, MSVC | Explicit user request |
| D02 | NoGraphicsAPI is the foundation | Explicit user request; substitutions require discussion |
| D03 | RTX 4080 primary; GTX 1080 Ti baseline | User clarified intended hardware tiers |
| D04 | Procedural Earth-like planet, gas giant, smaller bodies and belt | Explicit requested content |
| D05 | Realistic shading, shadows, HDR, sun and flares | Explicit requested rendering scope |
| D06 | Produce planning/handoff documents before implementation | Current user request |

## Proposed engineering choices

The prior plan proposed a conventional Vulkan compatibility path within a NoGraphicsAPI fork, artistic scene scale, a staged runnable implementation, and quality scaling. Treat these as the working direction, subject to the foundation spike. Do not assume compatibility has been established or that newer hardware automatically supports upstream extensions.

HDR means floating-point scene rendering and tone-mapped SDR output for the first release. Native HDR display presentation is deferred. Performance figures in the PRD are targets, not measured guarantees.

## Observed environment — 2026-09-11

- Workspace: `D:\non-esp\space-demo`; no existing application files were found before documentation authoring.
- PowerShell is the shell. CMake and Git are on PATH.
- `cl` was not on PATH; `vswhere -all -products '*' -format json` returned `[]`. The user says MSVC is installed; locate standalone/custom tools before concluding it is unavailable.
- `vulkaninfo` is available. Loader instance version: 1.4.309.
- Physical GPU reported: NVIDIA GeForce GTX 1080 Ti; device Vulkan API 1.4.312; NVIDIA driver 582.66.
- Full `vulkaninfo` output produced no matches for the four required extension names listed in ARCHITECTURE.md. This observation applies to the installed driver, not all future driver/hardware combinations.
- RTX 4080 availability and installed driver capabilities have not been verified.
- `slangc` was not found on PATH. SDK/compiler installations were not exhaustively searched.
- Overlay-layer version warnings appeared during Vulkan inspection. No graphics application was run.

## Open decisions and owners

| Question | Owner / resolution point |
| --- | --- |
| Can a limited compatibility backend preserve the required public API and shader behavior? | Foundation agent, T02; coordinating agent reviews proof |
| Which upstream revision and exact compiler/SDK versions are compatible? | Foundation agent, T01–T02 |
| Is an RTX 4080 available for actual testing, and which path does it support? | Coordinating agent, before high-tier sign-off |
| Coordinate/ABI conventions, texture representation and anti-aliasing method | Renderer owner, T03–T04; record before dependent integrations |
| Cloud volumetric complexity and final per-tier quality defaults | Atmosphere owner with profiling results, T06/T10 |

Append dated decisions with evidence and implications when these questions are resolved. Do not rewrite unknowns as facts.

## 2026-09-11 — implementation resolutions

- The conventional NoGraphicsAPI fallback is implemented and demonstrated on GTX 1080 Ti; details and upstream pin are in FOUNDATION.md. Demo shaders use this ABI even on newer hardware. This is not a replacement graphics library.
- Default shaders use Slang, offline SPIR-V compilation and scene_shared.h shared data layouts. GLSL is retained as a comparison path. Compiler binaries are pinned by bootstrap.ps1.
- Per user correction, the default material pipeline loads realistic sourced maps with provenance, not the optional simplistic procedural asset generator. Native Windows WIC avoids adding a decoding dependency. See ASSETS.md.
- Rendering is linear floating-point HDR internally, with ACES-fit tone mapping and sRGB presentation to an SDR swapchain. Native HDR-monitor output remains deferred.
- Longitude-wrap derivatives are corrected for spherical texture sampling. Temporal reprojection and area-weighted tiny-asteroid billboards address aliasing; residual phase/motion artifacts require the evidence in STABILITY.md, not a blanket resolved claim.
- Scene distances are compressed for an authored fictional-system tour. Giant/Earth radii are substantially differentiated; this is not a true-scale Solar System simulation.
- RTX 4080 quality/performance sign-off remains open. Only GTX 1080 Ti has been exercised locally.
- Per subsequent user direction, belt budgets increased 10x to 35k/65k. Seeded plane-cut polyhedra replace smooth displaced rocks, with split face normals and scanned dielectric PBR roughness. A separate polar cluster index conservatively rejects whole offscreen/planet-occluded regions while preserving original IDs and nested tiers. Existing four mesh-instance batches plus one distant-billboard batch avoid per-object draw calls. This is CPU clustered culling, not GPU indirect compaction or hierarchical-Z occlusion.
