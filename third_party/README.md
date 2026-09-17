# Third-party code

## NoGraphicsAPI (vendored fork)

`NoGraphicsAPI/` is a vendored copy of [sebbbi/NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI)
at upstream commit `8e414bd0a8010b9f721d06d470860e27aa69c071` (MIT, see `NoGraphicsAPI/LICENSE`).

The copy carries local compatibility changes: an opt-in conventional Vulkan 1.3 descriptor backend
(`NOGRAPHICSAPI_ENABLE_CONVENTIONAL_BACKEND` / `NOGRAPHICSAPI_FORCE_CONVENTIONAL_BACKEND`) for GPUs
without the experimental descriptor and address-command extensions. The conventional set layout has three bindings: binding 0 (sampled 2D images), binding 1 (samplers), and binding 2 (sampled array images for `Texture2DArray` textures). The public C++ API and shader entry-point names are unchanged.
See [docs/FOUNDATION.md](../docs/FOUNDATION.md) for the investigation.

`NoGraphicsAPI-compat.patch` is the full diff against the upstream commit. To regenerate it after
editing the fork:

Use a clean checkout at the pinned commit, copy the tracked files under `NoGraphicsAPI/`
over their matching upstream paths, and remove any upstream files absent from the vendored
tree. Include newly added files with `git add -N .`, then generate the patch from that
checkout with `git diff --binary --src-prefix=a/ --dst-prefix=b/`. Save its output as
UTF-8 without a BOM in `NoGraphicsAPI-compat.patch`. Do not copy build outputs or `.git`
directories, and do not stage the modified files before generating the diff.

Verify with `git apply --check` and `git apply` in a second clean checkout of the pinned
commit. The resulting file set and contents must match the tracked vendored tree (allowing
only CRLF/LF differences). Keep this patch synchronized whenever the fork changes.

Vendored code keeps its upstream formatting (`NoGraphicsAPI/.clang-format`) and is excluded from
the project's clang-format and clang-tidy runs.

## Wuffs

`wuffs/wuffs-v0.4.c` is the single-file release of [google/wuffs](https://github.com/google/wuffs)
at commit `0f214ba59c20c0c9c7ba841ecc3683f863965312`, Apache-2.0 OR MIT (`wuffs/LICENSE`). Only the PNG decoder and its
dependencies (adler32, crc32, deflate, zlib) are compiled, in `src/assets/image.cpp`. Wuffs is a
memory-safe, SIMD-accelerated decoder and is what loads every texture.

## stb

`stb/stb_image_write.h` (v1.16) from [nothings/stb](https://github.com/nothings/stb) at commit
`2c980bb59875b0d32144a71867fbdebb2f77cd20`, public domain / MIT (`stb/LICENSE`), encodes PNG
screenshots. Wuffs has no PNG encoder, so this is kept for export only.

## SMAA

`smaa/LICENSE.txt` covers the binary lookup tables in `smaa/` (`area.bin`, `search.bin`) and the
port of `SMAA.hlsl` in `shaders/aa/smaa.slang`, from [iryoku/smaa](https://github.com/iryoku/smaa) (Jimenez et al.,
MIT-style licence). The tables are embedded by a CMake build step; see [resource details](smaa/README.md).

## Dear ImGui

`imgui/` is [ocornut/imgui](https://github.com/ocornut/imgui) 1.91.9b (MIT, see `imgui/LICENSE.txt`): the core sources and the Win32
and SDL2 backends. Both backends are from the same 1.91.9b tag. The core is platform-agnostic and used by the application; the selected backend is compiled into the
platform layer, and the draw lists are rendered by the demo's own NoGraphicsAPI backend.

## Arm astcenc (optional offline tool)

`astcenc/` contains the unmodified build sources of Arm astc-encoder 5.7.0 at
commit `9fbec68053d507023bb30e2a449f900a915882b9`, Apache-2.0. See
[vendoring details](astcenc/ORBITAL.md) and [texture compression](../docs/TEXTURE_COMPRESSION.md).
The default build does not compile it; the demo does not link it.

## bc7enc (optional offline tool)

`bc7enc/` contains the unmodified block encoder from Richard Geldreich's bc7enc_rdo.
MIT/public domain, with license notice in the source. See
[vendoring details](bc7enc/ORBITAL.md). Only the optional `bc7_compress` executable
links it; the demo has no encoder dependency.

## Khronos Vulkan Validation Layers (optional development tool)

Sources and dependencies are materialized in `.tools` by `tools/build-validation.ps1`,
with exact revisions in `tools/validation-dependencies.json`. The local install retains
the Apache-2.0 license and a binary hash manifest. It is not linked or shipped with the
demo. See [renderer validation](../docs/RENDER_VALIDATION.md) for bootstrap and test commands.
