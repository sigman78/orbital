# Code style and conventions

This document records the decisions behind the C++ in `src/` and `tests/`. Formatting is mechanical
(`.clang-format`, run `tools/format.ps1`); everything below is about what the code does, not how it is
indented.

## Language level

- C++20 as the floor, MSVC `/permissive-`. Use what the standard gives us before writing our own:
  `std::span`, `std::string_view`, `std::optional`, `std::format`, `std::source_location`, designated
  initializers, `constexpr`/`consteval`, defaulted comparisons, `if constexpr`.
- Compile time first. Constants are `constexpr` with a name, tables are `static constexpr`, small
  helpers are `constexpr` so they can be used in both contexts. There are no runtime lookups for
  values known at build time.
- Templates only where they remove duplication of a small, obvious piece of code: `Vec3<T>` in
  `core/math.hpp` is the model. No CRTP, no traits hierarchies, no expression templates, no SFINAE.
  A concept is acceptable when it replaces an overload set; otherwise prefer a plain overload.
- `auto` for iterators, lambdas and values whose type is spelled on the right-hand side. Public
  function signatures always spell their types.

## Error handling

Three tiers, chosen by who could act on the failure:

| Situation | Mechanism |
| --- | --- |
| Programmer invariant (index in range, chain ends at 1x1, slot unused) | `ORBITAL_ASSERT(condition)` from `core/panic.hpp`. Active in every build. |
| Unrecoverable at runtime (missing shader or asset, GPU device creation, fixed budget exhausted) | `panic("message")` from `core/panic.hpp`: prints the message with its source location, breaks into an attached debugger, exits with a non-zero code. |
| Expected and recoverable (bad command line, optional file, validation of user data) | Return `std::optional<T>` or `bool` plus a logged message. Never throw. |

We do not throw exceptions and do not catch them. The demo has nothing useful to do after a load
failure except tell the user what is missing, and a stack of `try`/`catch` only obscures that. Code
that a test needs to exercise on the failure path gets a `try_` variant that returns `std::optional`
(`try_load_png`); the plain variant panics.

## Text output

- `core/log.hpp`: `log::info`, `log::warn`, `log::error` take `std::format` syntax and append a
  newline. `info` and `warn` go to stdout, `error` to stderr. That is the only text output layer.
- No `<iostream>`, `<fstream>` or `<sstream>` anywhere. Stream operators make formatting decisions
  invisible and pull in static initialisers; `std::format` states the layout in one place. A custom
  `<<` logging layer was considered and rejected for the same reason: it would only re-create what
  `std::format` already gives us.
- `std::printf` is fine in tests and tools where a format string with no arguments is the whole point.

## Files

- `core/file.hpp` wraps `fopen`/`fread`/`fwrite` (`_wfopen` on Windows for correct path encoding)
  behind `file::read`, `file::write` and `file::write_text`, all keyed on `std::filesystem::path`.
  Directory handling is `std::filesystem`. Whole-file reads and writes are the model; nothing streams.

## Numeric types

- `double` is used for simulation time and for positions in the scene graph and camera. The system
  runs for minutes at compressed but still large distances, and the deterministic tests pin
  positions to 1e-12. Everything else is `float` or an integer: GPU-facing data, image processing,
  per-instance culling after the camera-relative subtraction, statistics.
- Positions are made camera-relative in `double` and converted with `to_float` once. Nothing
  downstream of that conversion touches `double`.
- Integer math for pixel kernels. Only the 256-entry sRGB table is built with `double`, once.

## Data over argument lists

- A function that would take more than three or four arguments takes a struct with designated
  initialization instead: `Renderer::draw(FrameInput{...})`, `MaterialDesc`, `PipelineDesc`. The
  call site then reads as a list of named settings, and adding a field does not break callers.
- Aggregates with default member initializers rather than constructors wherever the type is plain
  data. Constructors are for types that own resources or maintain an invariant.
- Enumerations are `enum class`. Texture slots, render modes and camera modes are enumerations, not
  integer literals.

## Ownership and copies

- Large buffers (decoded images, mip chains, meshes, instance lists, file contents) have exactly one
  owner and travel by move. Functions return them by value and callers `std::move` them onward; a
  copy of a `std::vector` with megabytes behind it is a bug unless a comment says why.
- Read-only access is `const&` or `std::span`; `std::string_view` for text. A parameter is taken by
  value only when the callee stores it, and then the caller moves into it.
- Range-for over containers of non-trivial elements uses `const auto&`. `auto` copies are reserved
  for small value types such as `Vec3` and `BodyState`.
- Per-frame containers (instances, culled groups) are members that are cleared and reused, never
  reallocated each frame. Chains and staging buffers are sized once up front.
- Shared read-only data that several owners need is referenced, not duplicated; copy-on-write is
  the fallback if sharing ever turns into mutation, and it has not been needed yet.

## Magic numbers

A literal is acceptable when the context makes it self-explanatory: `* 0.5f`, `/ 255.0f`, array sizes
that equal a literal a line above. Everything else gets a `constexpr` name next to the code that
uses it (`shadow_map_size`, `staging_budget`, `baseline_belt_count`), with a comment when the value
was tuned rather than derived.

## Function size

- One function does one thing that fits on a screen. Frame building, culling, pass recording and
  resource creation are separate functions even when only called once.
- The exception is leaf code that is hot and deliberately dense: the SIMD kernels in
  `assets/kernels.cpp`. There, the loop body is the point and splitting it would hide the data flow.
- Cyclomatic complexity is kept down by early returns and by tables instead of `if` chains
  (bookmark definitions, material sources, texture slots).

## Layout and naming

- `src/core`: logging, panic, files, math. Depends on nothing else.
- `src/scene`: deterministic system generation and geometry. Depends on `core`.
- `src/assets`: image I/O, pixel kernels, materials. Depends on `core`.
- `src/app`: window, input, camera, HUD, `main`. Depends on everything.
- `src/render`: the Vulkan renderer. Depends on everything except `app` (the HUD image is passed
  in). Split by responsibility: resources and materials in one file, per-frame recording in another,
  a shared private header for the implementation struct.
- Types `CamelCase`; functions, variables, namespaces and files `snake_case`; private members with a
  trailing underscore; constants `snake_case` like any other value. Project includes are rooted at
  `src/` (`#include "scene/system.hpp"`), never relative (`../`).
- Vendored code under `third_party/` keeps its own style and is never edited to match ours.

## Tests

Executable-assertion tests (`assert` stays enabled through `/UNDEBUG`), one executable per module,
run through CTest. Tests exercise public headers only, compare against scalar references where an
optimised path exists, and print timings for the hot kernels so regressions are visible in CI logs.
