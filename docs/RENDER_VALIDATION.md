# Renderer correctness and culling investigation

## Local validation dependency

`tools/validation-dependencies.json` pins Vulkan-ValidationLayers at
`vulkan-sdk-1.4.357.0` (`f4874eee15c78d7bdb2b7e60659d539f14741500`) and its
Vulkan-Headers, Vulkan-Utility-Libraries, SPIRV-Headers, SPIRV-Tools and mimalloc
dependencies. `tools/build-validation.ps1` verifies the resolved revisions and builds
the layer locally. The installation under `.tools/Vulkan-ValidationLayers` includes
the layer DLL/JSON, symbols, license, `spirv-val` and a manifest recording the pins
and DLL hash. No global driver installation or registry changes are required.

```powershell
./tools/build-validation.ps1 -Jobs 8
./tools/build.ps1 -Preset release -RenderTests
ctest --test-dir build/release --output-on-failure
python tools/check-renderer.py --build build/release --gpu-assisted --output .scratch/render-validation-gpu
python tools/check-motion-streaks.py --build build/release
```

Requirements: Windows desktop GPU, MSVC/CMake/Ninja, Python with Pillow; the motion
fixture also needs NumPy and the local Slang compiler. Set `ORBITAL_VCVARS` when MSVC
cannot be discovered. `-Rebuild` cleans the validation build before rebuilding.
The first dependency build takes several minutes; later builds are incremental.

`ORBITAL_RENDER_TESTS` is opt-in and requires the demo and testing targets. It adds
`render_validation_probe` and the serial `renderer_validation` CTest, labeled
`gpu;validation`. Ordinary CPU-only test configurations remain available. Once
installed, the local `spirv-val` is also used by the shader build.

## What the render test verifies

The harness verifies the installation manifest and DLL hash, enables the layer only
for child processes, and records settings, logs and captures in `.scratch/render-validation`.
It first runs a deliberate overlapping-transfer write without a barrier and requires
`SYNC-HAZARD-WRITE-AFTER-WRITE`. The equivalent synchronized probe must pass. This proves
that synchronization validation is active and its messages are captured.

Scene cases cover Earth, belt with TAA on/off, movement followed by rest, tour, UI and
a scripted fullscreen transition. Captures must decode and contain non-flat content.
The normal core/synchronization run also exercises the Windows lifecycle smoke test,
including resize, minimize/restore and quality changes. Findings fail the test; a fresh
passing report is written only after every case succeeds.

GPU-assisted validation runs separately with synchronization checks and shader
instrumentation. Core checks are disabled in this mode as advised by the layer;
the normal suite supplies those checks. Unused ray-tracing/mesh instrumentation is
disabled. Only `WARNING-Setting-Limit-Adjusted` setup notices are accepted and retained
in the report; other warnings/errors fail. Interactive lifecycle checks are reserved
for the normal suite, while scripted fullscreen runs in both modes.

Validation timings are not performance measurements. These finite scenes also do not
prove every shader path or resource lifetime correct.

## Correctness fixes and evidence

Validation exposed two existing errors:

1. The galaxy shader converted a pointer through `uint64_t`, declaring `Int64` without
   enabling `shaderInt64`. A direct pointer cast removes the unnecessary capability
   (`VUID-VkShaderModuleCreateInfo-pCode-08740`).
2. The catalogue-star pipeline used a D32 attachment but declared no depth format
   (`VUID-vkCmdDraw-dynamicRenderingUnusedAttachments-08914`). It now declares D32.
   The pipeline descriptor flag is named `has_depth_attachment`, distinguishing
   compatibility from dynamic depth-test enablement.

The release configuration in `build/refactor` passed all 11 CTest suites, including
core/synchronization validation and lifecycle tests. The separate GPU-assisted scene
suite passed. GCC/MinGW passed all ten CPU suites; all 29 shader helpers compile.

## Motion cues

Motion cues previously entered TAA history without writing their own depth. TAA therefore
reprojected them using scene depth behind them and could retain them after movement
stopped. They now render after temporal resolve into reused HDR storage and are added
before bloom/tone mapping. At rest their pass and sampling are skipped, so stale streak
storage contributes nothing and no extra image allocation is required.

`tools/check-motion-streaks.py` uses an isolated flat-scene fixture with controlled TAA
metadata while exercising the real camera, mote geometry, temporal resolve, bloom and
tone mapping. It compares against an otherwise identical no-streak shader. The previous
build fails: TAA leaves 1,480 pixels more than eight display codes away from reference
on the first stopped frame (maximum 26), with 550 remaining seven frames later. The
fixed build has zero differing pixels from the first stopped frame with TAA on or off;
moving streaks remain visible (maximum difference 98). Reports:
`.scratch/motion-streaks-before/report.json` and `.scratch/motion-streaks/report.json`.

## Culling memory placement

The reported near-belt slowdown also occurs with TAA disabled. New panel/CSV sub-pass
timers distinguish culling, body shadows, belt light maps and belt disc bakes while
preserving the existing aggregate columns. `--pan-stop-frame N` stops scripted `--pan`
movement reproducibly. Initial tests found culling dominating at roughly 8–10 ms, but
did not consistently reproduce a drop specifically at the stop.

The old 128 MiB host-visible heap mixed CPU inputs with GPU-written atomic counters,
indirect commands and generated instances. Default capacity was sufficient: 961,002
instance slots versus at most 520,000 rocks plus six bodies. Arbitrary `--rocks`
overrides lacked a capacity check. Initialization now rejects unsupported counts before
allocation/generation. The first frame skips statistics readback until a result exists.

Culling scratch and generated instances now use a device-only heap sized for the
maximum configured rock count, including the high-quality tier. The mapped heap shrinks
to approximately 84.01 MiB; the default culling heap is 23.81 MiB. About 5.5 KiB of
parameters/body instances are uploaded each frame and 5.2 KiB of completed scratch is
read back. Transfer/compute/host dependencies are explicit; the existing timeline wait
protects CPU reuse. Culling rules, instance format, LOD and quality settings are unchanged.

Both old mapped memory and new device-only memory are device-local on this backend.
Measurements establish a memory-placement/size problem on this driver, not its exact
hardware mechanism. An intermediate experiment retaining the 128 MiB mapped pool while
moving culling output improved culling from 8.14 to 3.85 ms across three alternating
TAA-off runs. Shrinking the mapped pool as well yielded the larger improvement below.

GTX 1080 Ti, driver 582.66, standard belt protocol: TAA on, 360 frames, 120 warmup,
three runs per size, medians of run medians.

| Metric | Window 1600x900, before → after | Fullscreen 3440x1440, before → after |
| --- | ---: | ---: |
| Total GPU | 14.172 → 5.414 ms | 26.965 → 14.434 ms |
| Culling | 7.983 → 0.199 ms | 12.231 → 0.217 ms |
| Cull + shadows/maps | 8.813 → 0.589 ms | 13.215 → 0.614 ms |

Reports: [mapped baseline](performance/culling-mapped.md) and
[device-only result](performance/culling-device.md), with machine/protocol/hash metadata
in adjacent JSON files. Asset fingerprints match. An earlier comparison was rejected
because one directory had extra source/download files; the accepted rerun uses identical
trees. Raw CSV/logs and sub-pass medians remain in `.scratch/cull-memory-test/`.

The final allocation passed all 11 CTest suites, including core/synchronization and
window lifecycle checks, plus the separate GPU-assisted suite. Paired fixed-time
120-frame Earth/belt/tour captures have mean absolute RGBA differences of 0.000033,
0.000516 and 0.000040 display codes (maxima 1, 16 and 2). These tiny differences are
not bit-identical proof; GPU-generated instance order is not stable. Evidence:
`.scratch/cull-memory-test/captures/report.json` and
`.scratch/render-validation-gpu-cull/report.json`.

This removes the large stationary belt culling cost in measured views. The precise
moving-to-stopped transition has not been reproduced consistently; remaining behavior
should be checked interactively with this build. The normal release executable has
been rebuilt with the final allocation.

## Compact asteroid instances

Generated asteroid records now occupy 32 bytes rather than 48. The culling heap for
520,000 rocks and six bodies falls from 23.81 to 15.88 MiB, saving 7.93 MiB. Body
instances retain their 48-byte format; the generated asteroid region follows that
prefix. Indirect draw indices still include bodies, and the shared shader accessor
translates the index to the appropriate stride. Compile-time checks protect the
32-byte layout and packed identity capacity.

Position and radius remain float32. Mesh records preserve all three float32 Euler
angles and pack the rock id/composition into one word; shaders reconstruct the same
material tint and seed from that identity. Static `RockData` remains unchanged, so
spin rates and long-running orientation are not quantized.

Billboards use a different payload: half-precision RGB/rim lighting, full float32
ambient and coverage. The nonnegative rim's sign bit tags disc versus point splats.
Coverage stays full precision to preserve faint-asteroid opacity, and ambient stays
full precision because its ratio grows large in deep shadow. RGB is bounded by the
current fixed sun irradiance and normalized material inputs. Future changes to that
lighting range must revisit half-float representability.

The first scalar half-conversion implementation was rejected by core validation for
declaring Float16. The final implementation uses float32-input `packHalf2x16` and
`unpackHalf2x16ToFloat`, storing only uint words. Compiled shaders declare no Float16
capability; no new device feature was enabled. See the
[Slang packing reference](https://docs.shader-slang.org/en/latest/external/core-module-reference/global-decls/packhalf2x16-4.html).

Paired three-run belt benchmarks on the same machine/assets/settings:

| Total GPU | 48-byte records | 32-byte records |
| --- | ---: | ---: |
| Window 1600x900 | 5.385 ms | 5.365 ms |
| Fullscreen 3440x1440 | 14.347 ms | 14.570 ms |

These results support a memory saving, not a frame-time speedup. Reports and adjacent
JSON metadata: [unpacked](performance/culling-unpacked.md),
[packed](performance/culling-packed.md). Raw artifacts are in `.scratch/packed-culling`.

Six paired 1600x900, 120-frame captures cover Earth, belt TAA on/off, high quality,
sun-through-belt, and belt at simulation time 10,000. Mean absolute RGBA differences
range from 0.000040 to 0.001342 display codes; at most 20 pixels per capture differ
by more than four codes, with maximum 25. Differences include packing and unstable
GPU instance ordering; they are not bit-identical results. Commands, logs and report:
`.scratch/packed-culling/captures/`. All 11 release CTest suites pass, including
core/synchronization validation and window lifecycle checks. The separate GPU-assisted
suite also passes (`.scratch/packed-culling/validation-gpu/report.json`); inspection of
all 33 runtime SPIR-V modules confirms no Float16 capability. Release is rebuilt.
