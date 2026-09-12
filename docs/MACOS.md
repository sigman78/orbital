# macOS

The desktop demo runs natively on Apple Silicon through Vulkan/MoltenVK. Verified on an M1 Max,
macOS 15.7.4, Apple Clang 17, MoltenVK 1.4.2 and Vulkan headers/loader 1.4.357.
Intel macOS builds have a shader bootstrap path but have not been tested on hardware.

## Build and run

Install Xcode with its command-line tools and Homebrew, then:

```sh
brew install cmake ninja sdl2 freetype fontconfig vulkan-headers vulkan-loader molten-vk \
  vulkan-tools spirv-tools vulkan-validationlayers
bash tools/bootstrap.sh
cmake --preset macos-release
cmake --build --preset macos-release --parallel 4
ctest --preset macos-release
./build/macos-release/orbital --width 960 --height 540 --ui
```

Bootstrap downloads Slang 2026.14.1 for the host architecture and checks its SHA-256. Homebrew's
`s-lang` package is a different language and is not the shader compiler. Vulkan headers must be
1.4.357 or newer; the driver must expose Vulkan 1.4, buffer device addresses, and swapchain maintenance.

SDL dimensions are logical points. On a 2x Retina display, `--width 960 --height 540` renders at
1920x1080. The actual drawable extent is printed in the console. Function-key controls may require
Fn depending on keyboard settings; `--ui` opens the panel without a hotkey.

Run from a graphical macOS login. Shaders and assets are located relative to the executable.
This is a development executable linked to Homebrew libraries, not a self-contained, signed `.app`.

## Rendering checks

```sh
cmake --preset macos-debug
cmake --build --preset macos-debug --parallel 4
ctest --preset macos-debug
cmake -P tools/smoke-macos.cmake
```

Debug builds enable the validation layer when available. With a custom SDK, set `VK_LAYER_PATH`
to its explicit-layer directory if needed. The smoke check enables synchronization validation and
runs Earth twice, high-quality belt, HUD/UI, maximize and fullscreen. Each run is bounded to 40 frames
and 180 seconds; captures, logs and a belt CSV go to `captures/macos-smoke`. PNG captures precede
presentation overlays and do not include the ImGui panel.

All six scenarios passed on the M1 Max without validation messages. Repeated fixed-time Earth
captures differed by at most 1/255 per channel. A 180-frame release belt run at 1920x1080, high quality,
fixed time and vsync off measured a post-warmup median of 10.86 ms CPU including GPU wait and
10.19 ms GPU timestamps. These are local measurements, not a performance guarantee.

To include the backend's portable tests and its two GPU tests, after the normal debug build:

```sh
cmake --preset macos-debug -DNOGRAPHICSAPI_BUILD_TESTS=ON
cmake --build --preset macos-debug --target orbital test_shader_types test_allocators test_api \
  test_command_context test_placed_texture --parallel 4
ctest --preset macos-debug
```

All 13 tests passed on ARM64; no GPU tests were skipped. The optional NoGraphicsAPI AVX2/FMA math
test is only registered on x86-64 (build `test_math` there as well). Orbital uses its own portable math.
The `macos-cpu` preset needs no Vulkan, SDL or Slang dependencies.

## Compatibility

- Linux and macOS share SDL window/input/ImGui code and FreeType/Fontconfig HUD rendering.
  macOS executable discovery uses `SDL_GetBasePath`.
- The backend accepts ARM64 and enables portability enumeration and the advertised
  `VK_KHR_portability_subset` device extension. GPU-pointer shaders retain the conventional ABI.
- `drawIndirectCount` is optional. On the tested M1 Max it is false, so the renderer submits the
  fixed 96-group indirect array; compute zeros unused compacted entries. Culling, instance counts
  and mesh selection remain on the GPU. Supported devices retain the compacted-count path.
- Vendored fast_float parses command-line numbers with `from_chars` semantics because Apple's
  floating-point `std::from_chars` requires macOS 26.
- ARM64 uses existing scalar material pixel kernels; Wuffs supplies its own ARM PNG paths.

Windows and Linux have not been rerun as part of this port. CI includes macOS desktop compilation
and the nine tests that need no GPU; rendering smoke remains a local hardware check.
