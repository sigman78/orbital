# Detailed work packages

This file expands TASKS.md into independently assignable subsystem work. The integrated prototype now covers the core CPU systems and render pipeline; packages below retain their target acceptance requirements, not a claim of full completion. See HANDOFF.md and VALIDATION.md for current state. Milestone IDs refer to TASKS.md; package IDs below are the preferred delegation units. One agent can own sequential packages, but should receive a bounded assignment rather than the entire project.

## Contracts first

The integration owner publishes the initial schema and coordinate/material/ABI contracts with input from subsystem owners. Files under shared contracts, root build configuration and renderer resource management have one active owner. Agents use stub data or CPU-side tooling to work before GPU integration; they must not invent incompatible private versions of shared records.

| Package | Scope and owned area | Dependencies | Required result |
| --- | --- | --- | --- |
| W01 Toolchain/backend | `cmake/` discovery, NoGraphicsAPI fork, capability smoke | None | T01/T02 proof and explicit limitations |
| W02 System schema/generator | `src/scene/system*`, configuration and generation tests | Initial schema agreement | Seeded star/planet/moon/belt description, stable IDs, independent seeds, validity checks, inspectable default system |
| W03 Motion/scale | `src/scene/orbit*`, transform evaluation | W02 schema | Direct time evaluation, rotation/orbits, parent transforms, camera-relative conversion contract, separation/finite-value checks |
| W04 Asset pipeline | `src/assets/` loading, asset manifest/import tooling | Initial asset contract | Versioned seed/parameter cache, regeneration CLI, atomic outputs, provenance/license records and resource accounting |
| W05 Terrestrial materials | Earth generation module and its generation shaders | W02/W04 contracts | Continents, relief, biome/ice/ocean/roughness/normal maps with seam-safe mipmaps and visual evidence |
| W06 Gas-giant materials | Gas generation module and its shaders | W02/W04 contracts | Bands, coherent warping, storms, palette and animation parameters with full-disc/detail evidence |
| W07 Rocky assets/acquisition | Rock/crater generation, supporting asset manifest entries | W04 | Reusable rocky mesh/material inputs; vetted supporting assets only where useful; complete redistribution provenance |
| W08 Planet geometry/LOD | `src/scene/geometry/` planet meshes and LOD selection | Coordinate/map contract | Indexed sphere levels, bounds, normals, projected-error selection/hysteresis, seam/silhouette checks |
| W09 Asteroid distribution/LOD | Belt instance generation/culling and mesh variants | W02/W07, W08 conventions | Thousands of deterministic instances, culling and detail tiers, bounds and count diagnostics |
| W10 Shader toolchain | Shader CMake functions, ABI checks and common access helpers | W01 compiler/backend findings | Standalone shader target, include tracking, SPIR-V validation, packaged outputs, explicit variant matrix |
| W11 Camera/input/tour | `src/app/` input/camera and tour data | W02/W03 coordinate contract | Free-flight/orbit/bookmarks/tour, focus handling, approach limits, deterministic capture camera |
| W12 Renderer core | `src/render/` ownership/frame/pass infrastructure | W01/W10 | Safe frame lifetime, uploads, depth/HDR/presentation, resize, clear pass contract and timings |
| W13 Surface rendering | Opaque planet/rock shaders and material integration | W05–W08, W12 | Physically based land/ocean/gas/rock shading, correct color space and stable filtered detail |
| W14 Shadows/eclipses | Local shadow feature and shared solar visibility | W03/W09/W12 | Bounded local shadows, analytic eclipses, reusable fractional solar visibility and fixed-view checks |
| W15 Atmosphere/clouds | Atmosphere/cloud modules and shaders | W05 cloud contract, W12/W14 | Limb/terminator scattering, cloud motion and shadows, compositing/depth correctness, quality tiers |
| W16 HDR/sun/background | Post-process, stars and optical shaders | W12/W14 | Exposure/bloom/tone map, solar disc, occluded flare, filtered restrained background |
| W17 Showcase integration | Default scene/tour tuning, controls and quality presets | W09/W11/W13–W16 | Strong opening composition, required bookmark coverage, working controls, coherent quality scaling |
| W18 Validation/delivery | Tests, benchmark runner, captures, runtime packaging/docs | W17 | T09/T10 acceptance evidence, reproducible launch/build, measured or explicitly unverified hardware results |

## Parallel work before GPU compatibility is complete

Once the integration owner establishes initial contracts, W02 system generation, W04 asset orchestration, W08 CPU geometry and W11 camera math/tour data can begin independently of a working graphics backend. W03 follows W02. Surface algorithm prototyping can produce inspectable CPU/offline artifacts under W04's contract while W01/W10 establish the shader path.

Do not claim these offline results satisfy GPU rendering acceptance. Their purpose is to complete real subsystem work and reduce integration uncertainty while the backend is developed.

With four agent slots including the coordinator, a practical first implementation wave is one foundation owner (W01), one system owner (W02 then W03), and one geometry owner (W08), while the coordinator establishes asset/interface contracts. Later waves assign asset generation, shader tooling, camera and effects according to readiness. The slot count is a scheduling limit, not a reason to combine unrelated ownership.

## Detailed acceptance additions

- **W02/W03:** Multiple seeds produce valid systems; fixed seed/schema/time reproduces descriptions/transforms. Changing cloud resolution leaves body placement unchanged. Include invalid-config diagnostics.
- **W04/W07:** A clean asset run succeeds without undisclosed local files. Cache hits avoid regeneration; version/parameter changes invalidate appropriately. Every redistributed acquired asset has a recorded license.
- **W05/W06:** Save sphere-domain diagnostic maps and rendered views when available. Check poles, seams, mip levels and distinct seeds. Required major features remain coherent at distance.
- **W08/W09:** Verify winding/index bounds, conservative bounds, LOD thresholds and culling with deterministic cases. Inspect motion for popping and belt density loss.
- **W10:** A deliberate shader error fails the build; editing a shared include rebuilds affected shaders. Validate CPU/shader layout and backend-specific bindings with a GPU smoke shader.
- **W11:** Compare camera movement at different frame rates, test target motion, pause/input independence, focus loss and fast travel. Save bookmark/tour data as reviewable configuration.
- **W12–W16:** Record resource inputs/outputs and barriers, fixed-view evidence, cost and approximation limits. Test partial and complete sun occlusion, overlapping body depths and resize.
- **W17/W18:** Verify all required features on the baseline path; profile high-tier settings on actual RTX 4080 hardware when available. Keep visual, capability and performance sign-off separate.

## Handoff contract between agents

Every package returns owned files, public interface/schema changes, example invocation or consumer snippet, required assets/shader outputs, validation commands/results, outstanding assumptions and the next consumer. Record status/evidence here and link the corresponding milestone in TASKS.md. Integrating agents must read the producer's contract instead of reconstructing it from implementation details.
