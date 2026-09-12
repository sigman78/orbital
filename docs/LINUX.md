# Linux

The full Vulkan demo runs on Linux x86_64 with SDL2 (native Wayland or X11), FreeType and Fontconfig.
Requires a C++20 compiler with `std::format` (GCC 13+), CMake 3.25+ for presets, Ninja, Vulkan headers
1.4.357+, and a Vulkan 1.4 driver with swapchain maintenance support. Slang is pinned to 2026.14.1.

On Arch/CachyOS:

```sh
sudo pacman -S --needed base-devel cmake ninja vulkan-headers vulkan-icd-loader \
  vulkan-validation-layers vulkan-tools spirv-tools sdl2-compat freetype2 fontconfig
bash tools/bootstrap.sh
cmake --preset linux-release
cmake --build --preset linux-release --parallel 4
ctest --preset linux-release
./build/linux-release/orbital --ui
```

Bootstrap downloads and verifies the Linux Slang archive into ignored `.tools/slang`; it does not install
a GPU driver. Keep the driver appropriate for your GPU installed. On Debian/Ubuntu the development
packages are `libsdl2-dev libfreetype-dev libfontconfig-dev libvulkan-dev`; distribution Vulkan headers
may be too old. In that case provide Vulkan-Headers 1.4.357+ with `-DVulkan_INCLUDE_DIR=/path/to/include`.

CPU-only builds need neither shader tools nor Vulkan/windowing dependencies:

```sh
cmake --preset linux-cpu
cmake --build --preset linux-cpu --parallel 4
ctest --preset linux-cpu
```

## Rendering checks

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug --parallel 4
ctest --preset linux-debug
bash tools/smoke-linux.sh
SDL_VIDEODRIVER=x11 bash tools/smoke-linux.sh
```

Debug builds enable the Khronos validation layer when installed. Shader compilation runs `spirv-val`
when available. The smoke script checks bounded runs of Earth, high-quality belt, HUD/UI, maximize and
fullscreen, rejects validation messages, and saves PNGs, logs and a belt benchmark under `captures/`.
It retains two fixed-time captures for comparison; exact pixel identity is not required. Captures omit
the ImGui panel because it is drawn during presentation, after the captured image.

To include the vendored backend's six tests (two exercise the GPU):

```sh
cmake --preset linux-debug -DNOGRAPHICSAPI_BUILD_TESTS=ON
cmake --build --preset linux-debug --target test_shader_types test_math test_allocators \
  test_api test_command_context test_placed_texture --parallel 4
ctest --preset linux-debug
```

The normal build excludes vendored test targets, so build them explicitly before running CTest with
that option enabled. A GPU test reported as skipped is not a GPU pass.

## SSH

Run from your graphical terminal for interactive use. The smoke script can also run over SSH: when
display variables are absent, it imports only the current user's display/auth/runtime variables from
`systemctl --user show-environment`. This requires an existing graphical login. For example:

```sh
ssh sigman@192.168.1.198 'cd ~/orbital && bash tools/smoke-linux.sh'
```

SDL chooses the video backend; `SDL_VIDEODRIVER=wayland` or `SDL_VIDEODRIVER=x11` selects it explicitly.
Drawable size follows the compositor, including fractional scaling, so it can differ slightly from the
requested window size. Vulkan timing uses the queue's reported timestamp width, including wraparound.

## Verified hardware

Tested on CachyOS, GCC 16.2.1, Intel Graphics ADL GT2, Mesa 26.2.2, Vulkan 1.4.354, with a Wayland
desktop and its XWayland server. All 13 CPU/backend tests passed; debug rendering passed validation
for the smoke scenarios. A repeated 961x540 Earth capture differed by at most 1/255 per channel.
The full Windows demo has not been rerun as part of this Linux port.
