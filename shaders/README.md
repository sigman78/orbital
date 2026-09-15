# Shader organization

Shaders are grouped by rendering feature. Entry points live beside the helpers they
own; shared utilities and scene interfaces have separate directories.

| Directory | Responsibility |
| --- | --- |
| `aa/` | FXAA, SMAA and temporal accumulation/reprojection |
| `planets/` | Earth, airless and gas-giant materials, clouds and atmosphere |
| `belt/` | Culling, rock classification, splats, distant belt integration, dust and disc maps |
| `sky/` | Background, galaxy and stars |
| `post/` | Exposure, bloom, tone mapping, presentation and lens effects |
| `overlay/` | UI rendering |
| `surface/` | Shared surface vertex stage, material/varying records, BRDF and lighting; rock material used by belt rocks and moonlets |
| `scene/` | Bindings, frame root, view reconstruction, visibility, shadows, fullscreen geometry and motion cues |
| `lib/` | Resource-independent geometry, noise, color, projection, sampling, scattering and tone curves |

Feature folders contain both entry points and reusable helpers. Every helper includes
its own dependencies and uses `#pragma once`; callers must not depend on include order
or include an entry point. General utilities take explicit inputs and do not depend on
scene or feature code. Frame-aware helpers take `Frame*`; resource bindings belong to
`scene/bindings.slang`.

Dependencies follow actual reuse: shared surface geometry uses planet cloud-shell
geometry, and scene shadows use belt extinction. These are narrow helper dependencies,
not imports of feature entry points. `planets/giant_lightning.slang` and
`surface/brdf.slang` are pure feature helpers. FXAA takes an explicit image and sampler;
SMAA and temporal entry points use scene bindings. Temporal AA has no dependency on
post-processing implementation. The post dispatcher currently invokes the FXAA helper.

CMake lists source paths by feature but retains the existing flat SPIR-V filenames,
so folder moves do not change runtime pipeline names or the C++ shader interface.
Only shared C++/Slang ABI headers remain at the root.

## Bindings and interfaces

`resource_slots.h` defines texture and sampler slot numbers for both C++ and Slang.
Use `TEX_*` and `SAMPLER_*` names rather than numeric descriptor indices.
`scene/bindings.slang` declares the descriptor arrays once. Culling and UI include
that file but declare their own push-constant types; they do not include the standard
frame root. GPU records and surface mode/kind values remain in `scene_shared.h`.

## Post-processing passes

`post/bloom.slang`, `post/composite.slang`, `post/flare.slang`,
`post/sun_visibility.slang`, `post/present.slang`, and `aa/fxaa_pass.slang` compile
as separate fragment entry points, sharing the fullscreen vertex shader;
`post/meter_histogram.slang` is a compute entry point with its own push-constant root
(`MeterRoot` in `scene_shared.h`), accumulating the exposure histogram over sixteen frames. Bloom's
prefilter and two blur directions use `Root.mode`, with the C++/shader contract in
`post/bloom_shared.h`. Other post entry points do not use a global effect selector.
The lens flare stack lives in `post/lens.slang`: its soft elements (main ring,
crescents, ghosts, spindles) are drawn by `post/flare.slang` into a target at the
frame over the panel's divisor (2, 4 or 8, carried in `Frame.lens_stack`) and
sampled by the composite, which draws the sharp elements (glare, starburst, streak)
itself and applies the stack's saturation to all of them.
Keep the composite's per-pixel effects in one pass to avoid extra intermediate
images. `post/hdr.slang` and `post/sun_occlusion.slang` are focused shared helpers;
entry points do not include other entry points.

## Validation

Build through CMake to compile the entry points. Slang writes a depfile per output,
so changing a nested include rebuilds its consumers without a hand-maintained list.

```powershell
./tools/build.ps1 -Preset release
python tools/check-shader-helpers.py
ctest --preset release
```

The helper check compiles each include independently, catching dependencies that
would otherwise be supplied accidentally by a preceding include. It also runs on
Linux; `--compiler` selects an alternate Slang installation.

For behavior-preserving changes, compare fixed-time captures using identical flags,
including the tone and AA modes, before and after. GPU accumulation can vary slightly
between runs, so compare small differences against repeated baseline captures.
