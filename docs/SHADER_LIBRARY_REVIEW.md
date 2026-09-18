# Shader library review and refactoring proposal

Reviewed 2026-09-16 against working tree at `15fd412`. This is a proposal, not a description of completed refactoring. Three Sol subagents independently reviewed correctness, composability, and decomposition; the primary review checked their findings against source, host integration, and compiler results.

## Assessment

The library is a good feature-oriented demo renderer, but its reusable functions still inherit the assumptions of one particular planetary system. The next useful refactor is to **make body, material, light, and pass inputs explicit**, preserving the existing algorithms and shader families. More directory moves alone will not make a second independently configured planet possible.

The strongest existing foundations are worth keeping:

- Feature folders, small resource-independent utilities, named descriptor slots, and shared C++/Slang ABI declarations.
- Separate surface fragment pipelines with shared geometry, rather than one shader branching over every material family.
- Camera-relative positions prepared on the CPU, explicit detail tiers, packed asteroid records, and deliberately bounded integration work.
- Compiler-generated include dependencies, optional SPIR-V validation, isolated helper checking, and existing image/temporal regression tools.
- Deliberate pass fusion in postprocessing. Decomposing functions does not require adding render targets or GPU passes.

The source inventory is 58 Slang files, 3,802 lines, 33 entry points, and 30 guarded helpers. This is small enough to improve incrementally; it does not yet justify a general shader graph, runtime material interpreter, or wholesale renderer rewrite.

## Evidence and limits

| Check performed during this review | Result |
| --- | --- |
| `cmake --build build/release --target orbital_shaders` | Passed; incremental build recompiled two fragments. |
| Fresh compilation of every `[shader(...)]` entry point to temporary outputs, using the CMake target/profile/matrix/entry-name options, followed by `spirv-val --target-env vulkan1.3` | All 33 passed. One uint-to-bool warning at `surface_earth.slang:102`. |
| `python tools/check-shader-helpers.py` | Failed at `planets/cloud_geometry.slang:2`: undefined `ORBITAL_CLOUD_HEIGHT`. |
| Independent enumeration and checking of every guarded helper, continuing after failures | 29 passed; only `cloud_geometry.slang` failed. |
| Source review of bindings, host frame packing, pipeline blend modes, build rules, and CI | Findings below. |

No GPU captures, visual comparisons, performance runs, or full C++ test suite were run for this documentation-only review. Successful compilation and SPIR-V validation do not establish image correctness, descriptor lifetime safety, or performance. Existing measured results in other documents are prior evidence, not results reproduced here.

## Prioritized findings

### 1. Restore the helper dependency contract immediately

**Confirmed defect; small isolated fix.** [cloud_geometry.slang](../shaders/planets/cloud_geometry.slang), line 2, uses `ORBITAL_CLOUD_HEIGHT` without including its declaration. Current entry points happen to provide it through preceding includes, so production compilation succeeds while isolated compilation fails.

Include the owning shared header directly. Later, if cloud parameters become per body, replace the global height with an explicit parameter. Do not solve the failure by making the checker inject an umbrella include: that would hide the dependency it is intended to detect.

The [CI workflow](../.github/workflows/ci.yml) currently runs formatting and CPU-only builds. Add a pinned shader compiler/validator job that runs both entry-point compilation and the helper check. Neither check needs a rendering device. Have the helper checker report all failures in one invocation and use the production profile/layout options where applicable.

### 2. Replace singleton body settings with per-body records

**Architectural prerequisite for the requested growth, not a current showcase bug.** [scene_shared.h](../shaders/scene_shared.h), `Frame`, combines view state with `bodies[8]`, one `sun`, one Earth configuration, one giant configuration, and one belt. `scene.y` identifies the giant/belt owner. [resource_slots.h](../shaders/resource_slots.h) assigns maps to named bodies: `TEX_EARTH_*`, `TEX_GAS_*`, `TEX_MOON_*`, and so on.

[Showcase::resolve](../src/render/showcase.cpp) deliberately requires exactly one belt and one body of each major showcase role. [renderer_frame_data.cpp](../src/render/renderer_frame_data.cpp) packs those roles into the singleton fields. Consequently, duplicating a giant instance cannot give it independent maps, flow parameters, atmosphere, or ring resources.

Introduce per-body material/profile references and a per-draw body index, initially backed by records containing exactly today's settings. Keep the existing material-family pipelines. Treat the current showcase as an adapter that produces these records, rather than making the generic shader library resolve Earth/giant/desert roles.

Use two differently configured bodies of the same family as the first acceptance scene. A larger body array alone would not prove the problem solved.

### 3. Separate surface properties, visibility, and lighting policy

**Composability limitation.** [surface/types.slang](../shaders/surface/types.slang) puts `water` and `visibility` inside `Material`. [surface/brdf.slang](../shaders/surface/brdf.slang), `shadeSurface`, combines dielectric BRDF evaluation, ocean-specific tuning, normal-variance filtering, direct visibility, and a fixed-color ambient fill. It has no resource access, which is good, but its `ddx`/`ddy` operations make it fragment-stage dependent.

[surface/lighting.slang](../shaders/surface/lighting.slang), `surfaceAt`, loads instances and chooses body versus asteroid visibility policy. [giant_material.slang](../shaders/planets/giant_material.slang) further multiplies visibility by a belt shadow. These choices are valid for today's look but make the material result dependent on its surrounding scene.

Use three conceptual outputs:

- `SurfaceGeometry`: position, geometric/shading frame, local coordinates, and explicitly prepared footprint information.
- `SurfaceSample`: linear reflectance, shading normal, roughness, specular parameters, and emission when needed.
- `LightingContext`: view direction, sampled light direction/radiance, direct transmittance, and fill policy.

Keep material-local relief visibility as a separately named result if it depends on the sampled light. Do not multiply it into an ambiguous all-purpose `visibility` field. Keep the ocean recipe as a named material policy rather than forcing all materials to understand an Earth-specific water flag. Preserve the current numerical result through an adapter before changing the BRDF model.

Move derivative-based roughness filtering into a fragment adapter if the BRDF is to be called from compute/baking code. Resource independence and stage independence are separate properties.

### 4. Expose resource dependencies at feature boundaries

**Reuse limitation.** [clouds.slang](../shaders/planets/clouds.slang) samples fixed Earth resources; [giant_material.slang](../shaders/planets/giant_material.slang) samples fixed giant resources and reads `Frame.giant*`; [galaxy_layers.slang](../shaders/sky/galaxy_layers.slang) reaches directly into the global root. [beltfar.slang](../shaders/belt/beltfar.slang) combines geometry, depth access, sunlight baking, map filtering, and integration while importing `frame_bindings.slang`.

Pass typed parameter records and explicit textures/samplers into reusable feature helpers. Keep descriptor-array lookup and root decoding in thin entry-point adapters. A helper may remain resource-aware, but its resource dependency should be visible in its signature or an explicit resource bundle.

Existing reverse feature dependencies are documented, not accidental include cycles: scene shadows import belt helpers; shared geometry imports cloud geometry; instance decoding imports rock classification. Extract only the truly shared math/encoding portions. Keep belt-specific visibility in the feature adapter rather than moving the whole belt implementation into `lib/` to satisfy a directory rule.

### 5. Formalize numerical approximations before broadening their inputs

| Finding and evidence | Classification | Recommended treatment |
| --- | --- | --- |
| `scene/view.slang`, `sceneDistance`, treats `d >= .999999` as empty. With near `.02` and far `2000`, the continuous projection reaches that threshold at about 1,818 units; a point at 1,900 has depth about `.999999474`. The same threshold appears in temporal sky classification, sun occlusion, and far-belt depth handling. | Confirmed depth-classification counterexample; visible impact unmeasured. Actual boundaries depend on float quantization. | Centralize background-depth semantics and distinguish clear depth from valid geometry. Test 1,800/1,900-unit foreground geometry in each consumer. Exact clear-value comparison removes the broad epsilon band but cannot distinguish geometry that rounds to the clear value near the far plane; an explicit occupancy signal or revised depth scheme is needed if that distinction matters. |
| `scene/visibility.slang`, `solarVisibility`, checks an occluder's projected distance `t > 0` but has no upper bound at the sun. At point `(0,0,0)`, sun `(10,0,0)`, body `(20,0,0)` with radius 1, it reports occlusion although the body is beyond the light. | Confirmed finite-light counterexample; not demonstrated in the current showcase. | Define finite versus directional light semantics. Bound finite-light occlusion to the light segment, with an explicit policy for overlapping light/occluder spheres. The penumbra is `max(t * 0.007, 0.015)`, distance-scaled with a floor rather than fixed; deriving it from the light's apparent angular radius is optional. |
| `planets/cloud_geometry.slang`, `cloudShellDirection`, uses `rise / max(N dot L, .25)`. For unit ground radius, shell 1.009, and tangent sunlight, its ray offset is `.036`; the exact sphere intersection offset is about `.13447`. `cloudShellView` already uses a spherical intersection for the view case. | Confirmed geometric approximation; visible severity unmeasured. | Document the approximation or share an exact forward ray/shell intersection with explicit altitude and no-hit handling. Make any visual change a separate fix, not part of an extraction commit. |
| `planets/surface_airless.slang`, `reliefShadow`, assumes one texel has constant tangent arc `2*pi/4096`. With the 4096x2048 equirectangular maps this is correct at the equator; longitudinal distance shrinks by `cos(latitude)`, reaching about 17.4% at 80 degrees. | Confirmed metric approximation; high-latitude shadow distortion is a risk, not a measured screenshot defect. | Trace using a spherical/tangent metric and define seam/pole behavior. Supply map dimensions and height scale from material metadata. |
| `planets/atmosphere.slang` computes RGB transmittance, reduces it to mean opacity, caps opacity at `.94`, and returns straight-alpha radiance. The atmosphere pipeline uses ordinary alpha blending. | Deliberate model limitation. | Preserve the look during refactoring. For colored or optically thick media, support `radiance + transmittanceRGB * background`; scalar alpha cannot express that operation exactly. The cap retains at least 6% of background. |
| Airless and giant shaders embed texture dimensions, height ranges, and flow bake scales. Giant advection requires a positive cycle; the current CPU clamps it. | Valid current asset/host assumptions that are unsafe as undocumented reusable APIs. | Put dimensions, height units, flow encoding, and valid parameter ranges in asset/material contracts; preserve CPU validation when adding new data paths. |

These findings do not justify replacing every artistic approximation with a physically based model. They justify naming the approximation, its domain, and the test that would expose its failure when procedural content expands that domain.

There is also a **derivative-control-flow risk**: `giant_material.slang`, `giantDetail`, returns early on per-fragment `fine <= 0` before calling `sampleSphere`, which calculates `ddx`/`ddy`. Audit the emitted control flow; compute seam-correct gradients before divergent branches and pass them to sampling helpers. A `SampleGrad` call does not help if its supplied gradients were themselves computed in divergent control flow. This is a source-level portability risk, not a reproduced visual failure. The [Vulkan derivative rules](https://docs.vulkan.org/spec/latest/chapters/shaders.html#shaders-derivative-operations) require uniform control flow for explicit derivative instructions within the applicable derivative group.

### 6. Make the ABI and pass contracts executable

**Existing protection is useful but incomplete.** [gpu_types.hpp](../src/render/gpu_types.hpp) already asserts sizes and several offsets. Current `Frame` is 1,072 bytes, `Root` is 40 bytes, and the descriptor array contains 53 textures. [FOUNDATION.md](FOUNDATION.md) still mentions 40/51 textures and a 32-byte common root in its compatibility discussion. This is documentation drift, not evidence that the current shader ABI is broken.

Keep the shared headers and assertions. Add an offline layout/interface check for descriptor binding/count, push-constant ranges, shared record offsets/strides, and vertex/fragment locations. Compare compiler/reflection or SPIR-V layout information against explicit expectations; assertions on C++ sizes alone cannot catch every shader layout mismatch. Start with `Frame`, `Root`, and packed asteroid/culling records rather than building a general code generator.

`Root.mode` and `CullRoot.pass` have different meanings in different pipelines. Bloom already has [bloom_shared.h](../shaders/post/bloom_shared.h). SMAA and belt dust name their modes with local constants; belt blur (`root.mode == 0`) and culling (`root.pass` 0/1/2) use bare numbers documented only in comments. Apply the shared-constant pattern to those two first, and to SMAA and dust when the C++ recorder needs the names. `root.pass` also carries a caster count in the splat shaders, a different field in all but name. Splitting into separate entry points is optional when resources or stage behavior diverge; a uniform mode branch is not itself a defect.

For each pass, document and eventually validate: entry points, root type, input resources, output format/channels, resolution, blend/depth state, producer/consumer ordering, refresh cadence, and history invalidation. This belongs beside the host pass definition and shader interface, not only in prose.

## Proposed structure and interfaces

Retain feature directories. Strengthen the dependency boundary rather than requiring a large physical reorganization:

```text
Entry-point / renderer adapters
  decode roots, select resources, establish stage and pass conventions
        |
Feature algorithms: surface, atmosphere, belt, sky, post, AA
  accept explicit parameter/resource records
        |
Pure math / geometry / sampling kernels
  no Frame, root, fixed descriptors, or showcase roles

Shared ABI records underpin the adapters; semantic contracts underpin all layers.
```

Allow intentional feature composition at the adapter layer. A material evaluator should not silently pull in ring extinction; the caller should decide which visibility terms apply. Avoid an umbrella `common.slang` that imports the renderer again.

### Keep storage layout separate from the convenient shader API

First add shader-side accessors that decode the current packed `Frame` into named records. This improves call sites without changing GPU allocation or pointer layout. Then migrate fields by ownership and update frequency:

| Record | Owns | Typical lifetime |
| --- | --- | --- |
| `ViewParams` | Camera basis, projection/depth convention, viewport, current/previous jitter and view | Per view/frame |
| `LightParams` | Position/direction convention, spectrum/intensity, angular size | Per light/update |
| `BodyGpu` | Camera-relative transform/radius, material/profile indices, flags | Per body/frame |
| Family-specific material params | Surface tuning, map handles, encoding metadata, seed | Per material/update |
| `AtmosphereParams` / `BeltParams` | Profile and geometry parameters, resource references | Per body or belt/update |
| Pass params | Bloom, temporal, lens, presentation settings and resource selection | Per pass/frame |

These are proposed conceptual records, not prescribed byte layouts. Do not replace every `float4` with individual GPU allocations. Contiguous storage and existing buffer-device-address roots can remain while ownership becomes clearer. Use integer fields for new IDs/counts/flags and explicitly checked layouts.

The intended surface call flow is approximately this pseudocode:

```text
body       = loadBody(draw.bodyIndex)
geometry   = prepareSurfaceGeometry(varyings, view, body)
resources  = resolveMaterialResources(body.materialIndex)
sample     = evaluateAirlessSurface(geometry, materialParams, resources)
light      = sampleLight(geometry.position, lightParams)
visibility = evaluateSceneVisibility(geometry, body, light, occluders)
color      = shadeSurface(sample, geometry, light, visibility, fillPolicy)
```

Keep family-specific entry points so resource requirements and execution cost remain understandable. A procedural family can produce the same `SurfaceSample` from body-local coordinates and seed without editing the lighting core.

### Resource selection must include the backend

For the first two-body implementation, prefer per-draw material selection and coherent batching. Keep existing descriptor aliases as an adapter while proving independent material data. Replacing constant slot indices with material-dependent indexing requires checking emitted SPIR-V, device features, descriptor allocation, and backend support; changing shader signatures alone is insufficient.

If indices become divergent within a draw, Slang provides `NonUniformResourceIndex` to mark that resource-indexing behavior. It does not substitute for the corresponding device/backend support. See the [official Slang reference](https://docs.shader-slang.org/en/stable/external/core-module-reference/global-decls/nonuniformresourceindex-03ai.html). Do not make a bindless migration a prerequisite for the first cleanup.

Multiple belts also require per-belt baked maps, bounds, and invalidation on the CPU. [renderer_resources.cpp](../src/render/renderer_resources.cpp) currently builds from `system.belts.front()`; shader parameterization cannot create those missing resource lifecycles. Establish a bounded active-system/view working set, then assign visible bodies, relevant occluders, and nearby lights to it. Avoid a per-pixel loop over every body in every procedural system.

### Targeted decomposition

| Current location | Useful boundary | Preserve |
| --- | --- | --- |
| `planets/giant_material.slang` | Flow coordinates, resource sampling, deck combination, surface sample; explicit family params | Advection continuity, quality/detail fades, current family pipeline |
| `planets/atmosphere.slang` | Ray interval/occlusion, density profile, integration, output/compositing adapter | Step budgets and current scalar-alpha path until separately evaluated |
| `belt/beltfar.slang` | Slab/cylinder geometry, dust profile, sunlight bake, map sampling, far integration | Near/far energy and opacity handover |
| `post/composite.slang` | Scene-linear radiance composition, exposure/tone mapping, display look/HUD | Existing operation order and one GPU pass |
| `sky/galaxy_layers.slang` | Map acquisition and radiance transformation | Current resource/caching behavior |
| `surface/brdf.slang` | Fragment footprint filtering, BRDF evaluation, direct/fill composition | Existing tuned ocean and night-side appearance |

SMAA is the largest file at 342 lines, but algorithmic cohesion matters more than line count. Give its pass modes and resources explicit contracts before mechanically splitting the implementation.

Slang modules/imports are an optional later experiment once dependency boundaries are clear. They provide module loading and access control, but cannot repair singleton data ownership. Keep shared C++ ABI headers and trial one pure helper module before changing the build. [Official module documentation](https://docs.shader-slang.org/en/stable/external/slang/docs/user-guide/04-modules-and-access-control.html) describes the mechanism. Similarly, [Slang reflection](https://docs.shader-slang.org/en/stable/external/slang/docs/user-guide/09-reflection.html) is useful for layout checks; introducing `ParameterBlock` everywhere would be a separate binding/backend redesign.

## Minimum semantic contract

Write a compact shader-contract document, then put key preconditions at public helper definitions:

| Subject | Required declaration |
| --- | --- |
| Coordinates | Camera-relative versus body-local position; units; normalized direction requirements; matrix order/layout; handedness and UV orientation |
| Depth | Device depth versus linear view depth versus ray distance; near/far source; clear/background value; clipping policy |
| Temporal | Raster versus stable pixel grid, jitter units/sign, previous-frame coordinates, object-motion support, history channel meanings and reset conditions |
| Color | Reflectance versus scene radiance; texture decode; linear working primaries; exposure boundary; display encoding |
| Alpha | Coverage, opacity, premultiplied radiance, or packed metadata; required blend state |
| Materials | Roughness meaning/range, normal convention, height units, texture dimensions, flow encoding, seed stability |
| Integration | Density/extinction units, phase direction convention, step budget, early-out error/threshold, valid radius/scale-height ranges |
| Stage use | Fragment derivatives, implicit LOD, explicit-gradient/LOD alternatives, and whether compute reuse is supported |

Examples that must remain distinct: TAA history alpha carries depth; splat metadata carries coverage-weighted depth and coverage; far-belt output is premultiplied radiance/opacity; atmosphere output currently uses straight alpha. Naming all of them `float4 color` would conceal incompatible meanings.

Keep [TAA_COORDINATES.md](TAA_COORDINATES.md) and the implemented shader as the baseline for temporal contracts. [TAA_REVIEW.md](TAA_REVIEW.md) includes historical findings, some already addressed; do not reintroduce those fixes as open defects. Object motion, changing procedural detail, and independently moving media need explicit future history policies.

## Procedural-content requirements

Add these when introducing the first procedural material, rather than after many recipes depend on implicit choices:

1. Evaluate stable features in body-local coordinates with persistent seeds. Camera-relative position is appropriate for lighting precision, not a stable noise domain for terrain.
2. Carry a footprint/detail policy into procedural evaluation. Fade unresolved octaves and expensive relief; deterministic noise alone does not prevent aliasing or temporal shimmer.
3. Separate material identity from render tier. A mesh, billboard, and baked disc should derive compatible appearance from the same source parameters.
4. Define atmosphere thickness/scale heights in physical units or normalized radii, with conversion at one boundary. Today's normalized-radius coefficients imply a particular scaling model.
5. Keep previous transforms or another declared motion representation when adding moving bodies/material features to TAA. Do not expect camera-only reprojection to describe arbitrary procedural animation.
6. Budget work per visible body/system: material texture residency, atmosphere screen bounds/steps, belt-map resolution/update rate, and relevant lights/occluders. Generality must not silently multiply full-screen integration cost.

## Migration plan and acceptance gates

| Stage | Change | Acceptance gate |
| --- | --- | --- |
| 0: Repair and baseline | Fix cloud include, record toolchain, correct ABI documentation, add shader CI | All 30 helpers and all 33 entry points pass; baseline captures use identical settings and repeated runs |
| 1: Name existing contracts | Typed shader accessors over current Frame; shared pass constants; channel/stage/units documentation | Existing ABI and pipeline names unchanged; fixed-time captures match baseline within measured repeat variability |
| 2: Prove per-body materials | Add body/material records and explicit resources; migrate airless first, then giant; retain showcase adapter | Two bodies of one family have independent maps/settings; changing one leaves the other unchanged; ABI/interface checks pass |
| 3: Separate evaluation and lighting | Material/geometry/light records; isolate derivative filtering and medium visibility policy | Existing planets/rocks/cloud tiers remain visually equivalent; same material evaluator works in a small fixture |
| 4: Parameterize media and systems | Atmosphere profiles, belt records/resources, explicit lights and bounded occluder sets | Two independent atmospheres/belts; ordering, cache invalidation, active-set limits, and history resets exercised |
| 5: Improve models deliberately | Depth classification, finite-sun bounds, derivative placement, shell geometry, relief metric, optionally RGB medium composition | Targeted analytic fixtures plus documented visual differences and measured GPU cost |

Stage 5's small independent correctness fixes may be pulled forward. Keep behavior-changing patches separate from extraction patches so image differences remain attributable. Do not wait for a complete multi-system renderer to obtain the benefits of stages 0–3.

### Validation that earns its maintenance cost

- **Compilation/interface:** production entry points, isolated helpers, SPIR-V validation, ABI offsets/strides, stage interfaces, descriptor counts, and declared pass modes. Add simple dependency checks for pure helpers importing root/binding files.
- **Numerical fixtures:** finite-light occlusion before/beyond the light; cloud shell nadir/grazing rays; relief slopes at equator/high latitude; projection/depth round trips; zero-density and increasing-optical-depth media. Exercise actual Slang functions through small GPU fixtures where possible; a CPU formula alone cannot verify shader integration.
- **Image regressions:** Earth terminator/cloud fold, giant poles/flow phase, airless relief poles/seam, belt mesh/splat/far transitions, sun visibility, bloom, and SDR/HDR presentation. Reuse the existing TAA and bloom tools before adding another harness.
- **Temporal regressions:** static jitter phases, moving bodies, camera pan/stop, material/profile changes, resize/FOV changes, and history invalidation. Fixed-time still captures cannot establish temporal stability.
- **Performance:** matched resolution/bookmark/time/quality and warm-up, per-pass GPU timings, plus repeat-run spread as described in [PERFORMANCE.md](PERFORMANCE.md). Measure after changing resource access, derivative placement, integration, or draw batching; no speedup is claimed from cleaner source alone.

The practical definition of success is that adding a new planet of an existing family requires data and resources, adding a procedural material requires a focused evaluator, and adding an effect requires a declared pass contract. None should require editing a global collection of Earth-specific fields or relying on an include that happened to run first.
