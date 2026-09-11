# Contributing

## Prerequisites

Windows x64, MSVC C++ tools, CMake 3.24+, Ninja and a Vulkan loader import library (normally from
the Vulkan SDK). `tools/bootstrap.ps1` fetches pinned shader tools. See the [README](README.md) for the full build walkthrough.

## Formatting

C++ under `src/` and `tests/` is formatted with clang-format (LLVM 17 or newer) using the
repository `.clang-format`. Run the formatter before committing:

```powershell
./tools/format.ps1          # rewrite files in place
./tools/format.ps1 -Check   # verify only; this is what CI runs
```

Shaders (`shaders/`) and vendored code (`third_party/`) are excluded from the formatter.
`.editorconfig` covers indentation and whitespace for everything else.

## Static analysis

`.clang-tidy` configures clang-tidy for `src/` and `tests/`. It is advisory and not enforced in CI:

```powershell
cmake --preset release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build/release src/scene/system.cpp
```

## Tests

```powershell
./tools/build.ps1 -Preset release
ctest --preset release
```

All CTest suites (system, geometry, camera, image and material loading) are CPU-only and run in CI with
`ORBITAL_BUILD_DEMO=OFF`. The window smoke test (`tools/smoke-window.ps1`) needs a GPU and runs locally.
`tools/check-stability.ps1` compares two fixed-time captures; see [docs/STABILITY.md](docs/STABILITY.md).

## Vendored NoGraphicsAPI

Changes to `third_party/NoGraphicsAPI` must keep the upstream style and be reflected in
`third_party/NoGraphicsAPI-compat.patch`. See [third_party/README.md](third_party/README.md).

## Documentation

Design and status documents live in `docs/`; start with [docs/README.md](docs/README.md).
Record design changes in `docs/DECISIONS.md` and keep task status in `docs/TASKS.md`.
