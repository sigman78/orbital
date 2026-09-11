# Graphics foundation status

Pinned NoGraphicsAPI revision: `8e414bd0a8010b9f721d06d470860e27aa69c071` (upstream `main`, inspected 2026-09-11). The in-tree fork adds an opt-in conventional Vulkan descriptor backend while retaining the public C++ API.

## Local toolchain

- MSVC 19.44.35222, compiler at `C:\dev\msvc.2021\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`; initialize with `C:\dev\msvc.2021\VC\Auxiliary\Build\vcvarsall.bat x64`.
- Vulkan headers 1.4.357 at `.tools/Vulkan-Headers/include`, pinned externally at commit `e3b1eec08173d6b825cd3ac88c885a63b621504a`.
- Vulkan loader import library at `D:\blender-build\blender\lib\windows_x64\vulkan\lib\vulkan-1.lib`.
- glslang 16.5.0 at `.tools/glslang/bin/glslang.exe`. `tools/bootstrap.ps1` fetches the tagged Windows archive and verifies SHA-256 `06B71298B750268C127F2EE7AE0EF7525E2068120C6C8A3A08B2F58CA6F325CE`.
- Slang 2026.14.1 at `.tools/slang/bin/slangc.exe`; the official tagged Windows archive SHA-256 is `5ED0A59D650A0AF0ACA45D5DB4E083B3D8FB5CEA05748747DD95DFBE9C580658`.
- The tested device is NVIDIA GeForce GTX 1080 Ti, Vulkan 1.4.312, driver 582.66. It lacks `VK_EXT_descriptor_heap`, `VK_KHR_device_address_commands`, `VK_KHR_shader_untyped_pointers`, and `VK_EXT_mesh_shader`.

## Compatibility ABI

Configure NoGraphicsAPI with `NOGRAPHICSAPI_FORCE_CONVENTIONAL_BACKEND=ON`. `DeviceCaps::conventional_descriptor_backend` reports the active path. On this path:

- Root bytes are ordinary Vulkan push constants, visible to all stages. The maximum is `DeviceCaps::max_push_data_size`.
- GPU buffer pointers remain Vulkan buffer device addresses and GLSL may use `GL_EXT_buffer_reference`.
- Set 0, binding 0 is a fixed array of 24 separate sampled images.
- Set 0, binding 1 is a fixed array of 4 separate samplers.
- Compatibility GLSL SPIR-V uses the standard `main` entry point; the experimental path retains `vertexMain` and `fragmentMain`.
- Mesh shaders and storage image descriptors are unsupported. `DeviceCaps::mesh_shaders` is false.

The descriptor-heap allocation/write/bind calls retain their signatures. Writes update the conventional descriptor set using the byte offset as the descriptor index. Direct and indexed draw, push roots, dynamic rendering, HDR/depth attachments, buffer copies, texture upload/readback, barriers, and swapchain presentation use core Vulkan operations.

The parallel Slang shader path preserves this ABI in `shaders/common.slang` and `shaders/scene_shared.h`. Reflection verifies texture binding 0/count 24, sampler binding 1/count 4, and a 32-byte push root containing three typed 64-bit GPU pointers plus `base` and `mode`. Compile with SPIR-V 1.6 and column-major matrix layout; omit `-fvk-use-entrypoint-name` so Vulkan entry points remain `main` as required by the compatibility backend.

## Build evidence and remaining validation

Run `tools/bootstrap.ps1` to install the pinned Vulkan headers and shader compiler, then `tools/build.ps1 release`. Set `ORBITAL_VCVARS` when MSVC is installed outside Visual Studio discovery. The Vulkan loader/import library is intentionally not redistributed: install the Vulkan SDK or provide `Vulkan_LIBRARY` to CMake. The local Blender path below is environment-specific build evidence.

The fork builds successfully with MSVC using the commands below:

```bat
call C:\dev\msvc.2021\VC\Auxiliary\Build\vcvarsall.bat x64
cmake -S third_party\NoGraphicsAPI -B third_party\NoGraphicsAPI\build-compat -G Ninja -DVulkan_INCLUDE_DIR=.tools\Vulkan-Headers\include -DVulkan_LIBRARY=D:\blender-build\blender\lib\windows_x64\vulkan\lib\vulkan-1.lib -DNOGRAPHICSAPI_FORCE_CONVENTIONAL_BACKEND=ON
cmake --build third_party\NoGraphicsAPI\build-compat
```

Compilation and an eight-frame GTX 1080 Ti render/capture run are proven. The run exercised textured rendering, depth, an HDR offscreen target sampled into the swapchain, presentation, and readback capture. Resize and a validation-layer run remain separate acceptance work. Conventional timestamps now copy query results into live readback heaps with core Vulkan commands; application timing integration is pending. The compatibility descriptor set assumes descriptors referenced by a shader have been written. Indirect/address-only commands, compute pipelines, and mesh calls still require conventional fallbacks or explicit avoidance.

Conventional descriptor views are retired when their source texture is destroyed. Descriptor or sampler rewrites remain immediate operations, matching the API's general lifetime contract: the application must first wait for every GPU use. Current singleton descriptor writes during initialization/serialized resize satisfy that contract. `wait_idle` must not be called while a swapchain image is acquired; resize handling should finish or abandon the acquired frame before rebuilding resources.

## Validation layer

`tools/build-validation.ps1` reproducibly builds the official Khronos Validation Layers tag `vulkan-sdk-1.4.357.0` at commit `f4874eee15c78d7bdb2b7e60659d539f14741500`. Its pinned dependency graph supplies SPIRV-Tools 2026.3. The local install is `.tools/Vulkan-ValidationLayers/bin`; it does not register a layer globally or install a driver.

### Final density/faceted/cluster validation — 2026-09-11

The isolated Debug tree was configured with `cmake --preset local-debug` and built with `cmake --build build/debug`; the Release cache was not configured or modified. `ctest --test-dir build/debug --output-on-failure` passed all five tests (`system`, `geometry`, `assets`, `camera`, and `materials`) in 7.97 seconds.

With `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`, `VK_LAYER_VALIDATE_SYNC=1`, the local `VK_LAYER_PATH`, and implicit layers disabled, the following final smoke completed successfully on the GTX 1080 Ti:

```text
build/debug/orbital.exe --frames 30 --width 960 --height 540 --time 0 --bookmark 4 --high --capture captures/final-validation-bookmark4.bmp
```

The process exited 0 after 30 frames. Loader output confirms insertion of the local Khronos validation layer; searches for `NoGraphicsAPI validation`, `Validation Error`, `SYNC-HAZARD`, and `VUID-` returned zero findings. The 2,073,654-byte capture and stdout/stderr logs are under `captures/final-validation-bookmark4.*`.

With `VK_LAYER_PATH` set to that directory and implicit layers disabled, `build/debug/orbital.exe --frames 30 --width 960 --height 540 --time 0` completed successfully on the GTX 1080 Ti with no validation warnings or errors. The installed layer DLL SHA-256 is `369ABD66F7148DD18A346760A14B346D60F8B049E4786F45378AA502D491C5F7`.

Synchronization validation was then enabled explicitly with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` and `VK_LAYER_VALIDATE_SYNC=1`. Loader diagnostics confirm the local DLL was inserted as the instance and device layer. The latest Debug executable completed the same 30-frame run without core or synchronization messages; logs are `captures/validation-sync.stdout.log` and `captures/validation-sync.stderr.log`.

The window lifecycle smoke also passed under synchronization validation, including minimize/restore, three resize shapes, camera/bookmark/tour/quality inputs, clean exit, and BMP capture. Logs are `captures/validation-window.stdout.log` and `captures/validation-window.stderr.log`. The smoke script now discovers its hidden test window by process ID and class instead of relying on `MainWindowHandle`, which remains zero for a deliberately hidden launch.

SPIRV-Tools 2026.3 `spirv-val --target-env vulkan1.4` accepted all seven packaged Release modules: atmosphere, background, fullscreen, post, surface vertex/fragment, and temporal. Per-file results are in `captures/spirv-validation.log`.
