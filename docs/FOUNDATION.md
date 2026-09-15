# Graphics foundation status

Pinned NoGraphicsAPI revision: `8e414bd0a8010b9f721d06d470860e27aa69c071` (upstream `main`, inspected 2026-09-11). The in-tree fork adds an opt-in conventional Vulkan descriptor backend while retaining the public C++ API.

## Local toolchain

- MSVC 19.44.35222 (v14.44 toolset), initialized with `vcvarsall.bat x64`. `tools/build.ps1` locates it through `vswhere`; set `ORBITAL_VCVARS` for a standalone or custom installation.
- Vulkan headers 1.4.357 at `.tools/Vulkan-Headers/include`, pinned externally at commit `e3b1eec08173d6b825cd3ac88c885a63b621504a`.
- Vulkan loader import library (`vulkan-1.lib`) from an external installation, passed to CMake as `Vulkan_LIBRARY` (typically in an ignored `CMakeUserPresets.json`). It is not redistributed.
- Slang 2026.14.1 at `.tools/slang/bin/slangc.exe`. `tools/bootstrap.ps1` fetches the tagged Windows archive and verifies SHA-256 `5ED0A59D650A0AF0ACA45D5DB4E083B3D8FB5CEA05748747DD95DFBE9C580658`.
- The tested device is NVIDIA GeForce GTX 1080 Ti, Vulkan 1.4.312, driver 582.66. It lacks `VK_EXT_descriptor_heap`, `VK_KHR_device_address_commands`, `VK_KHR_shader_untyped_pointers`, and `VK_EXT_mesh_shader`.

## Compatibility ABI

Configure NoGraphicsAPI with `NOGRAPHICSAPI_FORCE_CONVENTIONAL_BACKEND=ON`. `DeviceCaps::conventional_descriptor_backend` reports the active path. On this path:

- Root bytes are ordinary Vulkan push constants, visible to all stages. The maximum is `DeviceCaps::max_push_data_size`.
- GPU buffer pointers remain Vulkan buffer device addresses, exposed to shaders as typed Slang pointers.
- Set 0, binding 0 is a fixed array of 40 separate sampled images.
- Set 0, binding 1 is a fixed array of 4 separate samplers.
- Entry points are `vertexMain` and `fragmentMain` on both backends, as upstream; Slang is compiled with `-fvk-use-entrypoint-name`.
- Mesh shaders and storage image descriptors are unsupported. `DeviceCaps::mesh_shaders` is false.

The descriptor-heap allocation/write/bind calls retain their signatures. Writes update the conventional descriptor set using the byte offset as the descriptor index. Direct and indexed draw, push roots, dynamic rendering, HDR/depth attachments, buffer copies, texture upload/readback, barriers, and swapchain presentation use core Vulkan operations.

The Slang shaders implement this ABI in `shaders/scene/bindings.slang`, `shaders/scene/frame_bindings.slang` and `shaders/scene_shared.h`. Reflection verifies texture binding 0/count 51 (`ORBITAL_TEXTURE_COUNT`), sampler binding 1/count 4, and a 32-byte push root per pipeline: three typed 64-bit GPU pointers plus `base` and `mode` for the surface and fullscreen shaders (the surface pipelines share one vertex stage, `surface.slang`, and have one fragment shader per body kind over `surface/types.slang` and the material/lighting helpers; the kind and mode numbers are defined once in `scene_shared.h`), the rock data and scratch pointers for the culling compute shader, and a vertex pointer with a pixel scale for the overlay. Compile with SPIR-V 1.6, column-major matrix layout and `-fvk-use-entrypoint-name`.

## Build evidence and remaining validation

Run `tools/bootstrap.ps1` to install the pinned Vulkan headers and shader compiler, then `tools/build.ps1 release`. Set `ORBITAL_VCVARS` when MSVC is installed outside Visual Studio discovery. The Vulkan loader/import library is intentionally not redistributed: install the Vulkan SDK or provide `Vulkan_LIBRARY` to CMake.

The fork builds successfully with MSVC using the commands below:

```bat
call "%ORBITAL_VCVARS%" x64
cmake -S third_party\NoGraphicsAPI -B third_party\NoGraphicsAPI\build-compat -G Ninja -DVulkan_INCLUDE_DIR=.tools\Vulkan-Headers\include -DVulkan_LIBRARY=<path-to>\vulkan-1.lib -DNOGRAPHICSAPI_FORCE_CONVENTIONAL_BACKEND=ON
cmake --build third_party\NoGraphicsAPI\build-compat
```

Compilation and an eight-frame GTX 1080 Ti render/capture run are proven. The run exercised textured rendering, depth, an HDR offscreen target sampled into the swapchain, presentation, and readback capture. Resize and a validation-layer run remain separate acceptance work. Conventional timestamps now copy query results into live readback heaps with core Vulkan commands; application timing integration is pending. The compatibility descriptor set assumes descriptors referenced by a shader have been written. Compute pipelines, dispatch, and direct/indexed indirect draws now have conventional fallbacks (buffer lookups by device address for index and argument ranges), and `draw_indexed_indirect_count` is a conventional-only addition over `vkCmdDrawIndexedIndirectCount`. `DeviceDesc::vsync` and `set_vsync` (another addition) select FIFO presentation or mailbox, falling back to immediate, with the swapchain recreated on the next acquire. `DeviceDesc::swapchain_color_space`, `surface_format_supported` and `set_swapchain_output` (additions for HDR output) enable `VK_EXT_swapchain_colorspace` when present and let the application query and switch the swapchain to a format and colour space pair (extended linear sRGB with 16-bit float, or HDR10 PQ with 10-bit), again recreated on the next acquire. Dispatch-indirect, address-only copies, and mesh calls still require fallbacks or explicit avoidance.

Conventional descriptor views are retired when their source texture is destroyed. Descriptor or sampler rewrites remain immediate operations, matching the API's general lifetime contract: the application must first wait for every GPU use. Current singleton descriptor writes during initialization/serialized resize satisfy that contract. `wait_idle` must not be called while a swapchain image is acquired; resize handling should finish or abandon the acquired frame before rebuilding resources.

## Validation layer

`tools/build-validation.ps1` reproducibly builds the official Khronos Validation Layers tag `vulkan-sdk-1.4.357.0` at commit `f4874eee15c78d7bdb2b7e60659d539f14741500`. Its pinned dependency graph supplies SPIRV-Tools 2026.3. The local install is `.tools/Vulkan-ValidationLayers/bin`; it does not register a layer globally or install a driver.

The repeatable [renderer validation suite](RENDER_VALIDATION.md) now checks that the layer is active with a negative synchronization control, then exercises real rendering and window lifecycle paths. Enable it with `tools/build.ps1 -RenderTests`; CTest runs it alongside the CPU suites. A separate GPU-assisted mode checks shader accesses. The 2026-09-13 run found and fixed the galaxy shader's unnecessary Int64 capability and the star pipeline's missing depth attachment format. This supersedes earlier one-off validation evidence. Repeat after changing the backend, resource lifetimes or shader ABI.
