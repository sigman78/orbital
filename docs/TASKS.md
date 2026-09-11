# Implementation tasks and delegation

Implementation has started and the native demo renders on GTX 1080 Ti. Foundation, scene generation, sourced assets, geometry, camera, Slang compilation and the rendering pipeline are integrated. Acceptance remains in review: validation-layer checks, motion stability, sustained performance and RTX 4080 testing are not all complete. [VALIDATION.md](VALIDATION.md) records evidence; [HANDOFF.md](HANDOFF.md) records current ownership and next work. The task definitions below retain the original acceptance bar.

These are integration milestones, not the complete work breakdown. Read [WORKSTREAMS.md](WORKSTREAMS.md) for W01–W18 assignable subsystem packages and [SYSTEMS.md](SYSTEMS.md) for their detailed requirements. Independent system generation, asset tooling, geometry/LOD and camera work may proceed alongside T01/T02 after shared contracts are agreed.

## Execution rules

- Read README.md, HANDOFF.md and relevant requirements before starting.
- Assign each agent a bounded task, prerequisites, owned files and acceptance criteria. Agents must not overwrite other agents' work.
- One integration owner controls root CMake files, dependency pins, shared shader ABI and application wiring. Other agents propose changes to those files or coordinate explicit ownership transfer.
- Freeze the minimum renderer/scene interface before parallel GPU feature implementation. Shared resource layout changes require integration-owner review.
- Keep task status current: not started, active, blocked, review, done. Include the actual commit or files, commands run and observed results when closing work.
- An unavailable GPU is an unverified target, not a passing test. A compilation success is not a rendering success.
- Stop a dependent task when its foundation assumption fails; continue independent work where useful. Escalate a change of graphics foundation with evidence.

## Milestone 0 — prove feasibility

### T01: Toolchain and device inventory

Owner: foundation agent. Dependencies: none. Owns: proposed `cmake/` tool discovery and a new `docs/VALIDATION.md` environment section.

Locate MSVC, Windows SDK, Vulkan SDK, Slang and SPIRV-Tools; inspect upstream source at an immutable revision. Record versions and commands. Confirm extension/feature support on available devices. Define actionable setup steps for missing prerequisites, without changing GPU drivers automatically.

Done: reproducible configure/compiler check, capability inventory, selected dependency revision and explicit gaps. No claimed RTX 4080 result without testing one.

### T02: NoGraphicsAPI compatibility spike

Owner: foundation agent. Dependencies: T01. Owns: dependency fork/patches and a minimal smoke program, coordinated root build changes.

Audit upstream initialization and shader contracts. Prove candidate replacements for unsupported extensions on GTX 1080 Ti, including descriptors, shader data/root ABI, depth and HDR offscreen presentation. Keep unsupported optional features detectable. Test the upstream path when capable hardware is available.

Done: visible textured/depth-tested geometry rendered through NoGraphicsAPI's application-facing API, HDR target sampled into the swapchain, resize/exit exercised, validation results recorded. Document fork changes and limits. If unsuccessful, deliver a precise blocker report before further GPU feature work.

## Milestone 1 — runnable rendering skeleton

### T03: Build, application and contracts

Owner: integration/application agent. Dependencies: T02. Owns: root CMake/presets, `src/app/`, shared contracts, packaging.

Establish target-based builds and shader dependencies, window/input, logging, error handling, configuration, scene/camera data contract and resource lifetime rules. Add free-flight/orbit camera and fixed bookmarks. Include a short smoke-run mode with a bounded frame count and nonzero failure exit status.

Done: documented Debug/Release builds, runnable packaged executable independent of working directory, camera controls, resize/minimize/restore, consistent CPU/shader layouts.

### T04: HDR renderer and sun

Owner: renderer agent. Dependencies: T03. Owns: `src/render/` core and post-process shaders.

Implement depth, linear HDR targets, opaque material baseline, pass synchronization, exposure, bloom, tone mapping, solar disc and occlusion-aware flares. Add GPU timing where supported and choose initial anti-aliasing.

Done: lit test spheres, visible HDR highlight rolloff, stable exposure, fully occluded sun produces no flare, safely recreated render targets. Publish pass input/output contracts for T05–T07.

## Milestone 2 — convincing planets and surrounding bodies

### T05: Planet geometry and surface generation

Owner: planet agent. Dependencies: T03 contracts; integrate against T04. Owns: planet scene/generation modules and planet-specific shaders.

Implement seed-based sphere material generation, mipmaps/LOD, terrestrial biomes/ocean/ice and gas-giant band/warp/storm fields. Supply material and cloud-mask interfaces to T06. Add rotation and parameter presets.

Done: both planets recognizable and detailed in fixed captures, no obvious UV seams/polar pinching, stable distant sampling, ocean glints and differentiated roughness, reproducible seeds, generation memory/time reported.

### T06: Atmosphere, clouds and eclipses

Owner: atmosphere agent. Dependencies: T04 pass contracts, T05 body/material contracts. Owns: atmosphere/cloud modules and shaders.

Implement optical-depth/scattering approximation, moving cloud layer, cloud shadows, shared sun visibility and analytic body eclipse handling. Provide low/high sample settings and correct depth/compositing behavior.

Done: convincing limb and terminator at fixed views; eclipse and cloud-shadow cases visibly work; no glow through opaque planets; costs measured at both settings. Record approximation limits.

### T07: Moons, belt and local shadows

Owner: small-body agent. Dependencies: T03 scene contract and T04 renderer contract. Owns: asteroid/moon generation, instance submission, local shadow feature and shaders.

Generate rocky body variants, distribute thousands of seeded asteroid instances, implement culling/LOD and bounded local shadows. Coordinate shared solar occlusion data with T06.

Done: belt reads as three-dimensional in motion, distinct rocky silhouettes, stable instance distribution, documented instance/triangle/draw counts and shadow cost. No solar-system-wide shadow-map precision failure.

## Milestone 3 — cinematic experience and measured delivery

### T08: Tour, backdrop and controls

Owner: application/presentation agent. Dependencies: T03; final composition after T05–T07. Owns: camera tour, background shaders, application controls/help.

Compose opening shot and tour, add restrained stars/galactic background, quality/exposure controls, pause, bookmarks and diagnostics. Keep controls discoverable and avoid covering the hero scene. Preserve deterministic capture timing.

Done: interruption and camera-mode changes work; all required showcase views are accessible; help documents actual controls; clean screenshots can hide the overlay.

### T09: Integration and correctness

Owner: integration agent. Dependencies: T04–T08. Owns: integration fixes, focused tests and validation record.

Check color/depth conventions, shader ABI, generation invariants, synchronization, resource retirement and quality changes. Run resize/minimize tests and a ten-minute tour. Inspect fixed captures for visual defects.

Done: PRD correctness criteria met, validation status recorded, no known application validation errors on tested setup, no unbounded memory growth. Missing environment capabilities remain explicit.

### T10: Performance and delivery

Owner: coordinating/optimization agent. Dependencies: T09. Owns: measured quality defaults, final README, captures and benchmark documentation.

Profile representative views, optimize measured bottlenecks and set per-tier defaults. Document exact commands, hardware/driver, settings, warmup, median/p95 frame times and memory observations. Package executable/runtime assets and attribution notices.

Done: fresh build/run instructions verified, runnable delivery produced, controls and limitations documented, captures attached, performance targets measured or explicitly unmet/unverified. Do not claim completion of an untested hardware tier.

## Suggested assignment waves

1. Foundation agent: T01 then T02. Coordinator resolves tool availability and reviews feasibility findings.
2. Integration owner: T03; renderer owner: T04 once the contract is ready.
3. Planet and small-body agents: T05 and T07 in parallel; atmosphere owner begins T06 after necessary planet contracts are published. Stay within available agent slots.
4. Presentation work T08 overlaps feature completion; integration owner then handles T09 and coordinates T10.

## Agent return template

Return task ID/status, changed files, implementation summary, interface changes, exact validation commands and outcomes, captures/timings where relevant, known limitations, and next dependency unblocked. Distinguish completed work from proposals. Leave unrelated files untouched.
