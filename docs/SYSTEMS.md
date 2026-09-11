# Subsystem design and contracts

The project has multiple substantial systems. Graphics compatibility is one enabling workstream; it does not define the scope of the demo. This document expands ARCHITECTURE.md and is required reading for subsystem owners.

## 1. Procedural planetary system

Separate system generation (which bodies exist and how they are arranged) from surface generation (what each body looks like). A master seed deterministically generates a system description containing a star, a terrestrial body, a gas giant, rocky satellites and one or more asteroid distributions. The default showcase seed must guarantee required content and a strong composition; arbitrary supported seeds must preserve valid configurations.

Use stable body IDs and independent derived seeds for placement, surface, clouds, geology and geometry. Adding an asteroid or changing cloud quality must not reshuffle every planet. Store a generator schema/version with the seed so cache invalidation and reproducibility are explicit.

Candidate data contracts:

| Record | Contents |
| --- | --- |
| `SystemDescription` | Schema version, master seed, star, bodies, belts, scale policy |
| `BodyDescription` | Stable ID, parent ID, class, radius, axial tilt, rotation, orbital elements, material seed, atmosphere parameters |
| `StarDescription` | Radius, color/temperature parameter, lighting intensity, position |
| `BeltDescription` | Center/parent, inner/outer radius, thickness, density profile, seed, size distribution |
| `SceneSnapshot` | Time, double-precision body positions/orientations and bounds, active light |

Validate positive radii, finite parameters, parent acyclicity, separation constraints, moon clearance and belt bounds. Use simple analytic orbits and axial rotation for coherent motion; full N-body dynamics are out of scope. Explicitly label artistically compressed scales. Evaluate time directly so a fixed timestamp produces a repeatable snapshot without frame-step drift.

Deliver a human-readable default system configuration, a seed override and a diagnostic system listing. Allow hand-tuning of generated parameters for the opening composition while preserving their provenance.

## 2. Planet and supporting asset pipeline

Procedural planet surfaces are required. Acquisition is appropriate for supporting resources where useful, such as licensed star catalogs, reference images, fonts, or auxiliary rock detail; it must not replace the procedural planetary content. Reference images are visual guidance, not automatically redistributable assets.

Create an asset manifest recording ID, origin (generated/acquired), generator version or source URL, author, license, redistribution permission, modifications and checksum. Never package an asset with unknown redistribution terms. Prefer generated supporting assets when acquisition brings little benefit.

### Terrestrial generation stages

1. Generate broad continental fields in sphere space, with controllable land fraction and coherent coastlines.
2. Derive elevation with mountain/ridge and lowland structure; avoid uniform high-frequency noise across every biome.
3. Derive temperature/moisture proxies from latitude, elevation and seeded fields; classify terrain and polar ice.
4. Produce linear/sRGB-appropriate albedo, height, tangent/object-space normal representation, roughness and ocean masks.
5. Generate independent coherent cloud coverage/density fields and wind/rotation parameters.
6. Generate seam-safe mip levels; validate poles and cube-face boundaries at multiple distances.

### Gas-giant generation stages

Generate latitude-dependent bands, large-scale warped flow and localized vortex/storm fields. Derive color and subtle normal/density variation from a shared flow field so features remain coherent. Use a limited palette and broad recognizable structures before small detail. Separate static baked material from inexpensive animated advection/rotation.

### Rocky body generation

Generate low-frequency irregular shape, bounded displacement, crater/ridge detail and mineral/albedo variation. Use several reusable meshes/material variants for the belt; derive per-instance variation without producing thousands of unique texture sets.

### Build/runtime boundary

Provide an offline asset-generation entry point for repeatable default assets, plus a runtime seeded generation path or explicit regeneration mode. Begin with CPU/reference generation if GPU infrastructure is unavailable; avoid two indefinitely maintained implementations. A shared parameter/schema contract permits a later compute implementation.

Cache generated artifacts by generator version, seed, parameters, resolution and format. Write caches atomically; treat invalid/missing caches as regenerable. Define texture channel meanings, color spaces, normal conventions and mip behavior in a machine-readable or shared code contract. Prefer uncompressed initial assets for correctness; add compression only after selecting supported formats and verifying quality. Record generation duration, disk size and resident memory.

## 3. Geometry and LOD

Choose an icosphere or seam-managed cube-sphere after comparing silhouette quality and mapping requirements. Provide indexed mesh generation, consistent winding, normals/tangents as required, bounds and multiple resolution levels. A whole-body mesh LOD is sufficient for orbital viewing; a streaming terrain quadtree is not required without landing support.

Select LOD by projected size or screen-space geometric error rather than fixed world distance. Add hysteresis to avoid rapid switching; use compatible transitions or short fades only where needed. Displacement must remain bounded and included in culling bounds. Cloud and atmosphere shells need their own conservative extents.

Asteroids use shared mesh LODs, frustum culling, compact instance data and distance-dependent tiny-body representations. Preserve belt mass/readability as geometry density falls. Expose visible/culled instance counts, triangles and LOD distribution for profiling. Do not make mesh shaders a prerequisite.

Acceptance views include a near orbital horizon, full planetary disc, distant planets and a belt flyby. Inspect cracks, disappearing silhouettes, backface errors, normal discontinuities and LOD popping.

## 4. Shader authoring and compilation

Treat shader tooling as a separate build subsystem. Pin compiler/tool versions against the selected backend revision. Document entry points, stages, target profiles, required capabilities, optimization/debug flags and matrix layout. Share CPU/shader ABI declarations where practical; verify size, alignment and pointer/index interpretation.

Define shader modules for common math/noise, surface generation, opaque planets, rocky instances, shadow depth, atmosphere, clouds, background, solar optics, luminance/exposure, bloom and final composition. Use a small explicit variant list for material class and backend capability; avoid unconstrained permutation growth.

CMake custom commands must track included shader files, rebuild affected outputs, fail on compilation errors and package outputs reliably. Validate emitted SPIR-V with a compatible validator. If backend shader access differs, centralize those differences and test representative shaders on each path. Provide a clean shader-build target that does not require launching the desktop application.

Development reload is optional: if implemented, compile to temporary outputs and retain the previous working pipeline on failure. Never destroy a pipeline still referenced by in-flight work.

## 5. Cameras, navigation and time

Implement free-flight with frame-rate-independent motion, mouse look, adjustable speed and predictable input capture/release. Implement orbit around a body with bounded zoom and a stable up convention. Camera switching must preserve a sensible position/orientation or move through a deliberate transition.

Use body-relative bookmarks so navigation follows moving planets. A camera tour specifies target body, local offsets, look direction/target, field of view and timing; use smooth interpolation with controlled acceleration. Include establishing, horizon/sunrise, terminator, gas-giant and asteroid views. Users can interrupt and restart it.

Maintain scene positions in double precision and build camera-relative render transforms. Determine conservative near/far planes, FOV and minimum approach distance from scene scale. Prevent camera entry into opaque bodies; a simple radial constraint is sufficient. Pause/time scale affect simulation without freezing input. Fixed time/seed/camera capture mode must produce comparable images.

Document actual bindings and support help/overlay toggles. Test input focus loss, differing frame rates, fast travel, orbit limits and switching while the target moves.

## 6. Renderer and effect ownership

Use explicit pass/resource contracts. The renderer owns frame targets, descriptor/heap allocation, uploads, synchronization, frame lifetime and presentation. Feature owners supply geometry/material inputs and pass implementations through that contract.

| Feature | Inputs | Outputs / integration contract |
| --- | --- | --- |
| Opaque surfaces | Camera-relative transforms, meshes, generated maps, sun, visibility | Linear HDR radiance and depth |
| Local shadows | Visible neighborhood and sun | Shadow visibility with bounded coverage and documented bias |
| Eclipses | Body spheres and solar geometry | Shared fractional sun visibility for surfaces/atmosphere/optics |
| Atmosphere | Body parameters, sun, depth, opaque color | Scattering/transmittance composited with correct body ordering |
| Clouds | Density, wind/time, atmosphere/sun visibility, depth | Cloud radiance/transmittance and surface shadow contribution |
| Solar optics | Projected sun and visibility | HDR solar emission/glare and smoothly occluded flares |
| Post-processing | HDR color, delta time, exposure setting | Bloom, exposure, tone-mapped presentation |

Define material units and approximations. Use energy-aware diffuse/specular shading and distinct ocean/land/rock roughness. Keep direct sun visibility consistent across all passes. Handle overlapping atmospheres and bodies by depth/order, not a fixed assumption that one particular planet is always nearer.

Choose spatial/temporal anti-aliasing deliberately and document its data needs. High-frequency procedural textures, stars and tiny asteroids require filtering; increasing mesh detail alone will not solve shimmer. Quality controls affect samples/resolutions/detail without changing seeded system identity.

## 7. Measurement and evidence

Provide deterministic bookmarks and a benchmark/tour mode early. Each subsystem reports its own costs: generation time/cache size, geometry/instance counts, shader compile time, GPU pass timings, CPU submission time and resource memory. Save image comparisons for day, terminator, night, eclipse, cloud shadow, near horizon, gas giant and belt views.

Use these measurements to select defaults for RTX 4080 and GTX 1080 Ti. Compatibility, visual completeness and performance are separate acceptance dimensions; report each honestly.
