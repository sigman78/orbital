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

- Workspace: the repository root; no existing application files were found before documentation authoring.
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
- Default shaders use Slang, offline SPIR-V compilation and scene_shared.h shared data layouts. Compiler binaries are pinned by bootstrap.ps1.
- The material pipeline loads realistic sourced maps with provenance; the earlier procedural texture generator was removed. Textures are stored as PNG and decoded with the vendored Wuffs decoder, so loading has no platform dependency. See ASSETS.md.
- Rendering is linear floating-point HDR internally, with ACES-fit tone mapping and sRGB presentation to an SDR swapchain. Native HDR-monitor output remains deferred.
- Longitude-wrap derivatives are corrected for spherical texture sampling. Temporal reprojection and area-weighted tiny-asteroid billboards address aliasing; residual phase/motion artifacts require the evidence in STABILITY.md, not a blanket resolved claim.
- Scene distances are compressed for an authored fictional-system tour. Giant/Earth radii are substantially differentiated; this is not a true-scale Solar System simulation.
- RTX 4080 quality/performance sign-off remains open. Only GTX 1080 Ti has been exercised locally.
- Per subsequent user direction, belt budgets increased 10x to 35k/65k. Seeded plane-cut polyhedra replace smooth displaced rocks, with split face normals and scanned dielectric PBR roughness. A separate polar cluster index conservatively rejects whole offscreen/planet-occluded regions while preserving original IDs and nested tiers. Existing four mesh-instance batches plus one distant-billboard batch avoid per-object draw calls. This is CPU clustered culling, not GPU indirect compaction or hierarchical-Z occlusion.
- Render prettify pass: the atmosphere march is clipped by the opaque depth buffer so rocks in front of a shell are never blended over. Rock shadows on the giant are replaced by an analytic belt shadow (ring profile shared between geometry::belt_ring_density and the shader). Surface detail is procedural where the source maps run out (scaled by texel magnification): Earth land grain and cloud structure, cloud shadows on the ground, Jupiter zonal drift with domain-warped swirls and layered turbulence, Moon relief and self-shadowing derived from the albedo (broad brightness as terrain, small bright features sunk as craters; no height map is sourced). The belt is thinned into rings, its radial bands orbit at slightly different rates, and every rock tumbles at its own rate. Lens ghosts are soft oblique ellipses along the sun axis instead of rings. The gas map is stored at 2K to hide JPEG blocking.
- Mars and its moons, sourced relief: the body list is data driven (up to eight; the renderer finds the terrestrial, gas giant and desert anchors by class). Mars is at true size relative to Earth with a thin dusty atmosphere; Phobos and Deimos are enlarged and orbit fast enough to watch, drawn as seeded rocks with the scanned rock PBR maps. The Moon and Mars shade from NASA elevation models baked into normal+height maps (LOLA, MOLA) instead of relief guessed from albedo; short height-field traces give crater-wall self-shadowing. Exposure adaptation is centre weighted and asymmetric in real time (fast to close down, slow to open), within about a stop; the final composite adds faint lateral chromatic aberration and midtone film grain.
- Rock library and material: rocks are icospheres displaced by a seeded shape function (lumps, ridged noise, bowl craters with rims, plane cuts) generated at six subdivision levels from one function, so normals and silhouettes agree across levels; 16 shapes times 6 levels are drawn as instanced groups by projected size. Meshes are indexed with shared vertices and shape-function normals, ready for meshoptimizer meshlets and mesh shaders later; tessellation was rejected because the GPU layer has no such stages and parallax covers the close-range relief. Three 2K CC0 rock sets are triplanar mapped in object space with a seeded primary/secondary blend and parallax occlusion on the dominant plane of close rocks.
- GPU-driven belt culling replaces the CPU cluster pass: a compute shader runs one thread per rock (frustum with jitter padding, analytic occlusion by every body, projected-size level pick, billboard demotion), then a count, prefix and scatter sequence writes per-group instance lists and indirect draw arguments that the CPU submits as one indirect draw per group. Everything goes through buffer addresses because the conventional descriptor set has no storage buffers; the fork gained conventional-backend paths for compute pipelines and indirect draws. Depth-pyramid occlusion was not added: the only occluders that matter here are the bodies, which the analytic test covers, and the conventional path has no storage images to build a pyramid with.
