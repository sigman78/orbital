# ORBITAL

Native Windows C++20 space demo built on a compatibility fork of [NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI). Earth-like world, gas giant, moon and seeded asteroid belt; sourced material maps, atmospheric scattering, shadows, internal HDR, bloom, exposure and temporal filtering.

![Earth with the gas giant rising behind it](docs/images/earth.jpg)

| ![City lights at dawn](docs/images/dawn.jpg) | ![Asteroid belt around the gas giant](docs/images/belt.jpg) |
| --- | --- |

The Jovian belt contains 280,000 asteroids in baseline and 520,000 in high quality, drawn from a library of 16 seeded shapes at 6 detail levels (20 to 20k triangles) with three scanned rock sets blended per rock, triplanar mapped with parallax on close rocks. A compute pass culls every rock on the GPU (frustum, planet occlusion, projected-size LOD) and writes indirect draws per shape and level; distant bodies use filtered billboards, and rocks lose sunlight to the belt's own density along the sun ray.

## Build

Requires Windows x64, MSVC C++ tools, CMake 3.24+, Ninja and Vulkan headers/loader import library (normally Vulkan SDK). NoGraphicsAPI is vendored under `third_party/NoGraphicsAPI`.

```powershell
./tools/bootstrap.ps1
./tools/build.ps1 -Preset release
ctest --preset release
./build/release/orbital.exe
```

Bootstrap downloads pinned shader tools; it does not install a GPU driver or full Vulkan SDK. Material textures are tracked in git as PNG; `tools/import-assets.ps1` records and reproduces their derivation from the upstream sources. Machine-specific overrides (for example an existing Vulkan loader library) go in an ignored `CMakeUserPresets.json`; in an MSVC developer shell, `cmake --preset <your-preset>` then `cmake --build --preset release`.

Slang is the default shader language, with offline compilation and a shared C++/shader layout header. See [foundation details](docs/FOUNDATION.md) for upstream compatibility changes.

## Explore

RMB + mouse looks around; WASD flies, Q/E moves vertically, Shift accelerates. Keys 1–6 select Earth, Jupiter, Moon, Mars, dawn and belt views. T starts/stops the tour, O orbits, F returns to free flight. Space pauses the system; +/- changes exposure, X toggles adaptation. F1 toggles help, F2 quality, F3 the belt transmittance map, F4 the belt extinction, F5 cycles anti-aliasing (off, temporal, temporal plus FXAA), F6 cycles the rock splat cut-off (off, 1.2, 2.5, 4 px), F7 lights the splats in both culling passes instead of the scatter pass only, F8 cycles the tone curve (Khronos PBR Neutral by default, AgX, ACES filmic), F12 saves a capture, Esc exits.

```powershell
./build/release/orbital.exe --width 1920 --height 1080 --tour
./build/release/orbital.exe --bookmark 5 --time 0 --frames 300 --high --rocks 650000 --benchmark belt.csv
./build/release/orbital.exe --bookmark 4 --time 0 --frames 124 --no-hud --capture captures/belt.png
./build/release/orbital.exe --duration 60 --tour --benchmark captures/tour.csv
./build/release/orbital.exe --bookmark 5 --time 0 --frames 40 --pan 3 --aa 1 --capture captures/pan.png
```

`--help` lists options. `--time` freezes simulation and exposure adaptation for reproducible captures; `--pan` strafes the camera at a fixed rate, to compare anti-aliasing modes in motion.

## Repository layout

| Path | Contents |
| --- | --- |
| `src/core` | Logging, panic, file I/O and vector/matrix math shared by everything else |
| `src/platform` | OS-agnostic window, input, process and text-overlay interfaces; `win32/` implements them |
| `src/app` | Options, camera, HUD layout and the frame loop; no OS calls |
| `src/scene` | Deterministic system generation and mesh geometry |
| `src/assets` | PNG image I/O (Wuffs decode, stb write), SIMD pixel kernels and material mip-chain generation |
| `src/render` | Vulkan renderer on top of NoGraphicsAPI: resources and materials, per-frame passes, GPU-side types |
| `shaders/` | Slang shaders; `scene_shared.h` is the shared C++/shader layout |
| `tests/` | Executable-assertion tests run through CTest |
| `tools/` | PowerShell scripts: bootstrap, asset import, build, format, smoke and stability checks |
| `cmake/` | Slang shader compilation helper |
| `docs/` | Product, architecture, decisions, validation and handoff documents ([index](docs/README.md)) |
| `third_party/` | Vendored NoGraphicsAPI fork with its patch against upstream, Wuffs and stb_image_write ([details](third_party/README.md)) |

## Development

Sources are formatted with clang-format and checked in CI; `.clang-tidy` provides advisory static analysis. Conventions (C++20, error handling, logging, ownership, settings scopes) are in [docs/CODE_STYLE.md](docs/CODE_STYLE.md); see [CONTRIBUTING.md](CONTRIBUTING.md) for the workflow.

```powershell
./tools/format.ps1          # format src/ and tests/
./tools/format.ps1 -Check   # verify without changing files
```

## Status

Working desktop implementation tested on GTX 1080 Ti; RTX 4080 remains untested. Internal HDR is tone-mapped to SDR; this is not native HDR-monitor output. The system uses compressed fictional distances. Atmosphere/clouds and temporal reconstruction are approximations, with further visual/performance acceptance work tracked in [validation](docs/VALIDATION.md) and [handoff](docs/HANDOFF.md).

## License

The demo is released under the [MIT License](LICENSE). NoGraphicsAPI is MIT licensed (`third_party/NoGraphicsAPI/LICENSE`); Wuffs is Apache-2.0 OR MIT (`third_party/wuffs/LICENSE`) and stb_image_write is public domain (`third_party/stb/LICENSE`). Material textures are CC BY 4.0 (Solar System Scope) and CC0 (Poly Haven); provenance, hashes and attribution requirements are in [docs/ASSETS.md](docs/ASSETS.md) and `assets/materials/manifest.json`. Keep attribution with redistributed assets.
