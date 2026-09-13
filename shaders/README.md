# Shader organization

Entry points stay at the top level. Reusable code is separated by its dependencies,
not collected in an umbrella include.

| Directory | Responsibility | Dependency rule |
| --- | --- | --- |
| `lib/` | Geometry, noise, color transforms, projection, sampling, scattering and tone curves | No frame, push constants or global resources; pass images and samplers explicitly |
| `scene/` | Descriptor bindings, frame root, view reconstruction, body visibility and shadows | Frame-aware helpers take `Frame*`; only `frame_bindings.slang` declares the surface/fullscreen root |
| `surface/` | Varyings and material records, visibility setup, BRDF, cloud geometry and gas-giant effects | Include the specific material or lighting helper needed; the vertex shader does not import fragment shading |
| `post/` | FXAA and lens effects | Explicit inputs; no global push constants or descriptor arrays |

`surface/brdf.slang` and `surface/giant_lightning.slang` are pure feature helpers:
illumination, glint strength and lightning settings arrive as parameters. Giant material
sampling uses the scene descriptor table, while its frame settings are passed explicitly.
`belt.slang`, `beltfar.slang`, `clouds.slang` and `rockclass.slang` remain cohesive
feature helpers at the top level.

Every helper includes its own dependencies and has an include guard. Callers must not
depend on another entry point or on include order. Add general math to the smallest
appropriate `lib/` file; keep scene-specific effects in their feature family.

## Bindings and interfaces

`resource_slots.h` defines texture and sampler slot numbers for both C++ and Slang.
Use `TEX_*` and `SAMPLER_*` names rather than numeric descriptor indices.
`scene/bindings.slang` declares the descriptor arrays once. Culling and UI include
that file but declare their own push-constant types; they do not include the standard
frame root. GPU records and surface mode/kind values remain in `scene_shared.h`.

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
