# Code style and conventions

This document records the decisions behind the C++ in `src/` and `tests/`, and the lessons from
applying them. Formatting is mechanical (`.clang-format`, run `tools/format.ps1`); everything below
is about what the code does, not how it is indented.

## Language level

- C++20 as the floor, MSVC `/permissive-`. Use what the standard gives us before writing our own:
  `std::span`, `std::string_view`, `std::optional`, `std::format`, `std::source_location`,
  `std::from_chars`, designated initializers, `constexpr`/`consteval`, defaulted comparisons,
  `if constexpr`.
- Compile time first. Constants are `constexpr` with a name, tables are `static constexpr`, small
  helpers are `constexpr` so they can be used in both contexts. There are no runtime lookups for
  values known at build time.
- Templates only where they remove duplication of a small, obvious piece of code: `Vec3<T>` and
  `Range<T>` in `core/math.hpp` and `parse_number<T>` in `main.cpp` are the models. No CRTP, no
  traits hierarchies, no expression templates, no SFINAE. A concept is acceptable when it replaces an
  overload set; otherwise prefer a plain overload.
- `auto` for iterators, lambdas and values whose type is spelled on the right-hand side. Public
  function signatures always spell their types.
- No `std::function`, no `std::stoi`-family parsing (they throw), no `<random>` in deterministic
  code (implementation-defined distributions; use SplitMix).

### Designated initializers, learned the hard way

- An initializer list is either all designated or all positional; the compiler rejects a mix.
  Prefer all designated for any struct with more than two fields, and write a small helper lambda
  when one call site would otherwise repeat the same field names ten times.
- Fields must be named in declaration order. Keep the order in the struct meaningful (identity,
  geometry, flags) so call sites read naturally.
- A struct with a reference member (`FrameInput::camera`, `Session`) still initializes with
  designated syntax and is the right way to pass a bundle of context by reference. It is not
  assignable, which is fine for a value that lives for one call.

## Platform layering

The tree is a strict stack. A module may include headers from the layers below it and never from
the layers above or beside it.

```
app        options, camera, HUD layout, frame loop, main        (no OS headers)
render     Vulkan renderer through NoGraphicsAPI                (no OS headers; opaque native handle)
platform   window, events, process, text overlay interfaces    platform/<os>/ implements them
scene      system generation, geometry                         assets  image I/O, kernels, materials
core       log, panic, file, math, types, small_vec            depends on nothing
```

### Interfaces

- `platform/window.hpp`, `platform/process.hpp` and `platform/text.hpp` are the whole OS surface the
  demo uses. They are written in the project's own types (`Extent2D`, `Bytes`, `std::string_view`,
  `std::filesystem::path`) and expose no OS type: no `HWND`, no `POINT`, no wide strings. The one
  concession is `Window::native_handle()`, an opaque `void*` that the renderer forwards to the
  graphics backend without interpreting it.
- Events are pulled, not pushed. `pump_events()` drains the OS queue into plain data (`key_presses()`,
  `mouse_look_began()`, `take_mouse_look_delta()`) that the frame loop reads afterwards. There are no
  callbacks into application code from inside the OS message loop, so application state is only ever
  touched from the frame loop.
- Keys are the `Key` enumeration. Letters and digits carry their ASCII codes so tables and range
  checks read naturally (`letter_key('W')`, `digit_of(key)`); everything else is a named value above
  the ASCII range. The backend maps virtual-key codes both ways in one place.
- Text crosses the boundary as UTF-8; the backend converts to whatever its API wants. Fonts, DPI,
  cursors and window classes are backend details and never appear in the interface.
- The text rasterizer takes a description (`OverlaySpec`: items, rules, size) and returns bytes.
  What the HUD says and where it goes is application code (`app/hud.cpp`); how glyphs get onto
  pixels is platform code.

### Backends

- One directory per operating system, `platform/win32/` today, each implementing every interface
  header in full. A backend is a CMake target (`orbital_platform`) and is the only target that links
  OS libraries; the demo executable links the target, not `user32` or `gdi32`.
- Backend code follows the same rules as the rest of the tree (panics, settings, RAII wrappers such
  as `Canvas` and `SelectedFont`) and may use OS types freely inside its own `.cpp` files.
- Adding a platform means adding `platform/<os>/` with the three implementation files, a CMake branch
  selecting it, and a swapchain path for it in the graphics backend. Nothing in `app/` or `render/`
  changes.

### Guarded exceptions

Four files keep small compile-time guards instead of a backend, because the alternative would be a
module with one function: `core/file.cpp` (`_wfopen_s` for Unicode paths), `core/panic.cpp` (debugger
break), `assets/kernels.cpp` (instruction-set intrinsics, which are about the CPU rather than the OS)
and the vendored-library include block in `assets/image.cpp` (compiler warning pragmas). Each guard
has a portable fallback. A `_WIN32`, `_MSC_VER` or `<windows.h>` anywhere else is a review finding.

### Compilers

- Do not lean on MSVC leniency: no non-standard extensions, no reliance on its two-phase lookup
  quirks or its permissive conversions. `/permissive-` is on.
- Everything below `platform` is built and tested with GCC on Linux in CI and with MinGW GCC locally
  through `tools/check-gcc.ps1`. That script links the C++ runtime statically on purpose: a
  dynamically linked test picks up whichever `libstdc++-6.dll` is first on PATH, and a stale one
  fails at load time with an entry-point dialog per executable.
- The desktop demo itself is Windows-only until a second backend exists; the layering is what makes
  that a contained job rather than a rewrite.

## Containers

- Standard containers are generic and allocate; use the one that fits the shape of the data, and
  prefer no container at all when a fixed array will do.
- `SmallVec<T, N>` (`core/small_vec.hpp`) is a fixed-capacity sequence stored entirely inline, with
  a size counter no wider than `N` needs and no heap fallback. Pushing past the capacity panics. Use
  it only where the bound is a compile-time fact (`static_assert(material_count <= capacity)`) or
  validated input (`max_body_count` in `validate_system`); `try_push_back` is for queues where
  dropping the overflow is the intended behaviour (key presses between two pumps). Anything else,
  including a function that accepts a caller-sized span, uses `std::vector`.
- `std::vector` only for large owning buffers (pixels, meshes, instance lists) and for containers
  that are cleared and reused. Never a `std::vector` of `std::vector`s for a grid; use one flat
  array plus offsets (the belt clusters are a counting sort into `belt_order`, and each cluster views
  its slice).
- No node-based or hashed containers (`std::map`, `std::unordered_map`, `std::set`, `std::list`) for
  small keyed sets. A linear search over a handful of bodies is faster than a hash lookup and does
  not allocate; `evaluate_system` went from two maps per frame to none.
- Fixed-size C arrays or `std::array` for tables and lookups whose size is known at compile time.

## Strings

- String constants are `constexpr std::string_view` (or `const char*` inside `constexpr` tables and
  wide literals at the Win32 boundary). `std::string` exists only while text is being built.
- Error and log messages are built on the failure path only; the success path formats nothing.
- Functions take `std::string_view` and `const std::filesystem::path&`, never `const std::string&`.

## Views over ownership

- When a callee only reads data, it takes an immutable view: `ByteView` for bytes,
  `std::span<const T>` for anything else, `std::string_view` for text. Ownership is transferred only
  when the callee keeps the data, and then by move.
- A view is valid only while its owner is immutable. Where a view outlives the call that created it
  (`BeltCluster::indices` into `Impl::belt_order`) the owner is documented as frozen after
  construction, and nothing appends to it afterwards.
- Leaf kernels keep raw pointer plus dimension parameters on purpose; everything above them has
  already validated sizes through the view types.

## Type aliases

- A standard type spelling that appears in more than one signature gets a name that says what it
  means: `Bytes` and `ByteView` (`core/types.hpp`), `MipChain`, `BodyStates`, `ValidationErrors`,
  `Uploads`. Readers then see the role of the data, not its container.
- Aliases live next to the type they describe (`MipChain` in `image.hpp`, `BodyStates` in
  `system.hpp`); only the byte aliases are global because every module uses them.
- `bytes_of` turns a span or vector of trivially copyable values into a `ByteView` for uploads and
  file writes, so no call site spells `reinterpret_cast`.

## Error handling

Three tiers, chosen by who could act on the failure:

| Situation | Mechanism |
| --- | --- |
| Programmer invariant (index in range, chain ends at 1x1, slot unused) | `ORBITAL_ASSERT(condition)` from `core/panic.hpp`. Active in every build. |
| Unrecoverable at runtime (missing shader or asset, GPU device creation, fixed budget exhausted) | `panic("message")` from `core/panic.hpp`: prints the message with its source location, breaks into an attached debugger, exits with a non-zero code. |
| Expected and recoverable (bad command line, optional file, validation of user data) | Return `std::optional<T>` or `bool` plus a logged message. Never throw. |

- We do not throw exceptions and do not catch them. The demo has nothing useful to do after a load
  failure except tell the user what is missing, and a stack of `try`/`catch` only obscures that.
- Code that a test needs to exercise on the failure path gets a `try_` variant that returns
  `std::optional` (`try_load_png`); the plain variant panics. A function that reports failure
  through `bool` logs the reason itself so callers only decide what to do next.
- `panic_if(condition, "message")` replaces an `if`/`panic` pair when the message is constant. When
  the message needs `std::format`, keep the pair so nothing is formatted on the success path.
- `panic` calls `std::_Exit`, so it is safe from worker threads and never runs destructors of
  half-initialized GPU objects. `ORBITAL_ASSERT` is a macro because it needs the stringized
  condition; `panic` is a plain function because `std::source_location` gives it the location.
- Windows API failures at startup (class registration, window creation, module path) are panics.
  Failures during a running frame (swapchain acquire, minimized window) are `false` returns.

## Text output

- `core/log.hpp`: `log::info`, `log::warn`, `log::error` take `std::format` syntax and append a
  newline. `info` and `warn` go to stdout, `error` to stderr. That is the only text output layer.
- No `<iostream>`, `<fstream>` or `<sstream>` anywhere. Stream operators make formatting decisions
  invisible and pull in static initialisers; `std::format` states the layout in one place. A custom
  `<<` logging layer was considered and rejected for the same reason: it would only re-create what
  `std::format` already gives us.
- Text that is built up (a CSV, a report) is a `std::string` filled with `std::format_to` and a
  `std::back_inserter`, then written once with `file::write_text`.
- `std::printf` is fine in tests and tools where a format string with no arguments is the whole point.
- Wide strings only at the Win32 boundary (`std::format(L"...")` for window titles).

## Files

- `core/file.hpp` wraps `fopen`/`fread`/`fwrite` (`_wfopen_s` on Windows for correct path encoding)
  behind `file::read`, `file::write` and `file::write_text`, all keyed on `std::filesystem::path`.
  Directory handling is `std::filesystem`. Whole-file reads and writes are the model; nothing streams.
- Encoders that want a write callback (stb_image_write) append to a `std::vector` and the vector is
  written once. Decoders read the whole file into memory and parse from it.

## Numeric types

- `double` is used for simulation time and for positions in the scene graph and camera. The system
  runs for minutes at compressed but still large distances, and the deterministic tests pin
  positions to 1e-12. Everything else is `float` or an integer: GPU-facing data, image processing,
  per-instance culling after the camera-relative subtraction, statistics, timings.
- Positions are made camera-relative in `double` and converted with `to_float` once. Nothing
  downstream of that conversion touches `double`. Rebuilding the belt culling this way changed a
  handful of edge pixels and nothing else, which is the expected cost.
- Integer math for pixel kernels, with the 2x2 sums widened to 16 bits. Only the 256-entry sRGB
  table is built with `double`, once, so its values stay identical across compilers.
- Mixed-type `std::min`/`std::max` do not compile; write the literal in the operand's type
  (`std::max(stats.frame_ms, 0.1f)`).

## Data over argument lists

- A function that would take more than three or four arguments takes a struct with designated
  initialization instead: `Renderer::draw(FrameInput{...})`, `MaterialDesc`, `PipelineDesc`,
  `ImageDesc`, `BeltParams`. The call site then reads as a list of named settings, and adding a
  field does not break callers.
- Aggregates with default member initializers rather than constructors wherever the type is plain
  data. Constructors are for types that own resources or maintain an invariant (`Window`, `File`).
- Enumerations are `enum class`. Texture slots, sampler slots, shader modes and camera modes are
  enumerations, not integer literals. Where an enumeration mirrors a shader-side constant, the
  comment names the shader and the enum lives next to the other GPU layout definitions
  (`render/renderer_impl.hpp`, `render/gpu_types.hpp`, `shaders/scene_shared.h` with its
  `static_assert`s on sizes and offsets).

## Ownership and copies

- Large buffers (decoded images, mip chains, meshes, instance lists, file contents) have exactly one
  owner and travel by move. Functions return them by value and callers `std::move` them onward; a
  copy of a `std::vector` with megabytes behind it is a bug unless a comment says why.
- Read-only access is `const&` or `std::span`; `std::string_view` for text. A parameter is taken by
  value only when the callee stores it, and then the caller moves into it. `std::span<Upload>`
  accepts both a `std::vector` and a local array, so single-item calls need no wrapper.
- Range-for over containers of non-trivial elements uses `const auto&`. `auto` copies are reserved
  for small value types such as `Vec3` and `BodyState`.
- Per-frame containers (instances, culled groups) are members that are cleared and reused, never
  reallocated each frame. Chains and staging buffers are sized once up front.
- Shared read-only data that several owners need is referenced, not duplicated; copy-on-write is
  the fallback if sharing ever turns into mutation, and it has not been needed yet.

## Small value types and constant groups

- Values that always travel as a pair or triple get a small named type instead of two loose fields:
  `Range<T>` for limits and clamps (`Range<float> depth{0.02f, 2000.0f}`), `Extent2D` for widths and
  heights, `Vec3` for anything spatial. The type carries the operations (`clamp`, `contains`,
  `aspect`) so callers stop re-deriving them.
- Constants are grouped by the decision they encode, not by the file or module they happen to sit
  in. A group is named after that decision (`exposure_meter`, `shadow_placement`, `belt_culling`,
  `cluster_grid`) and lives next to the code that reads it; a header only when two files read it.
- The shape of a group follows what it is:
  - a **namespace** when it is a scope of compile-time constants that nothing ever varies
    (`namespace exposure_meter { inline constexpr float key = 0.18f; ... }`). Namespaces reopen, so
    each concern defines its own constants beside its consumer, and nested namespaces give the same
    `belt_culling::billboard::min_pixels` path a nested struct would.
  - a **struct with instances** when the group is a value: something that exists in more than one
    configuration or gets passed around. `QualityTier` has a `baseline_quality` and a `high_quality`
    instance that `FrameInput::high_quality` selects between.
  - a **struct with invariants** when the members constrain each other and a `static_assert` or
    a derived accessor should say so: `HeapLayout` relates the heap size to its offsets and exposes
    `instance_capacity()`.
  - a **function-local `constexpr`** when exactly one function reads it.
- Two levels of nesting at most. A module-wide `XSettings` struct that collects unrelated decisions
  under one prefix is the shape to avoid: it reads as a path but it is a grab bag, and `RenderSettings`
  was replaced with six concern-sized groups for that reason.
- Each tuned value gets a trailing comment saying what it means or where it came from. Derived
  values are computed from the group next to their use, not stored twice.
- A constant that describes the scene belongs in the scene description, not in a renderer group:
  Earth's day-map alignment is `BodyDescription::rotation_phase`, not a render offset.

## Magic numbers

A literal is acceptable when the context makes it self-explanatory: `* 0.5f`, `/ 255.0f`, array sizes
that equal a literal a line above, the `3` triangle vertices of a fullscreen draw. Everything else is
a named settings field or a `constexpr` next to the code that uses it, with a comment when the value
was tuned rather than derived.

## Function size

- One function does one thing that fits on a screen, about sixty lines. Frame building, culling,
  pass recording and resource creation are separate functions even when only called once.
- Two exceptions: leaf code that is hot and deliberately dense (the SIMD kernels in
  `assets/kernels.cpp`, where the loop body is the point), and a top-level sequence whose whole
  purpose is ordering (`Renderer::draw` listing the passes).
- Cyclomatic complexity is kept down by early returns, by tables instead of `if` chains (bookmark
  and tour views, material sources, LOD thresholds), and by small context structs (`RockCullContext`,
  `ViewVolume`) that turn a nested loop body into a call.

## Names and scopes

- Types `CamelCase`; functions, variables, namespaces and files `snake_case`; private members with a
  trailing underscore; constants `snake_case` like any other value.
- A local never reuses the name of a member or of an outer-scope constant. `/W4` reports the
  shadowing and the fix is a more specific name (`view_depth`, `min_length`), not a suppression.
- A type forward-declared at namespace scope must be defined at that same scope. Defining it inside
  an anonymous namespace silently creates a second, unrelated type; keep TU-local types in the
  anonymous namespace only when nothing outside the TU names them.
- Project includes are rooted at `src/` (`#include "scene/system.hpp"`), never relative (`../`).
  The formatter keeps `<windows.h>` first, then other Win32 headers, then project headers, then
  third-party, then the standard library.

## Layout

- `src/core`: logging, panic, files, math, byte aliases, `SmallVec`. Depends on nothing else.
- `src/scene`: deterministic system generation and geometry. Depends on `core`.
- `src/assets`: image I/O, pixel kernels, materials. Depends on `core`.
- `src/platform`: OS-agnostic interfaces with one implementation directory per OS. Depends on `core`.
  See "Platform layering" for the rules.
- `src/render`: the Vulkan renderer. Depends on `core`, `scene` and `assets`; the HUD image and the
  native window handle are passed in, so it needs neither `app` nor `platform`. Split by
  responsibility: `renderer_impl.hpp` holds the private `Impl`, the slot and mode enumerations, the
  heap layout, target sizes and quality tiers; `renderer_resources.cpp` creates the device, meshes, materials, pipelines and
  targets; `renderer_frame.cpp` builds frame data, culls, records passes and captures.
- `src/app`: options, camera, HUD layout, frame loop, `main`. Depends on everything, calls no OS API.
- Each directory is one CMake target with the same name prefix (`orbital_core`, `orbital_scene`,
  `orbital_assets`, `orbital_platform`, `orbital`), and the target link graph mirrors the include
  graph above. A new include direction is a new link edge, and the reverse is a review finding.
- Vendored code under `third_party/` keeps its own style and is never edited to match ours. It is
  compiled from exactly one translation unit with warnings suppressed around the include (MSVC
  `warning(push, 0)`, GCC `diagnostic push`), and pinned to a commit recorded in
  `third_party/README.md`.

## Concurrency

- CPU work that is embarrassingly parallel (decoding and filtering materials) runs on a bounded pool
  of `std::async` workers pulling indices from a shared `std::atomic`, capped at
  `min(8, cores - 1)`. There is no general thread pool and no task graph; the demo has one such job.
- Workers never throw. A failure inside a worker is a panic, which terminates the process from that
  thread by design.
- Each worker writes only to its own slot of a preallocated result vector; the join is
  `future::get()` on every worker before the results are read.

## Optimised code

- Measure first. The material pipeline was instrumented per stage before any kernel was written,
  and the numbers, not intuition, picked decode, transform, filter and upload as the targets.
- A hand-written SIMD kernel comes with a scalar reference in the same file and a test that
  compares them on odd, even and 1-wide inputs. Integer kernels must match exactly; single-precision
  kernels may differ by one code from a differently ordered scalar evaluation, and the test says so.
- Instruction-set dispatch happens once at startup from CPUID (`kernels::backend()` reports the
  choice); the baseline is SSE2 on x64 and scalar elsewhere. AVX2 paths carry the target attribute
  on GCC/Clang and rely on MSVC allowing the intrinsics without `/arch`.
- GPU uploads share one bounded staging heap because host-visible heaps on this backend live in the
  256 MiB BAR window; an allocation sized to the whole asset set fails. Budgets like this are
  settings fields with the reason in the comment.

## Tests

- Executable-assertion tests (`assert` stays enabled through `/UNDEBUG`), one executable per module,
  run through CTest. Tests exercise public headers only.
- `static_assert` for anything `constexpr` (vector arithmetic, `Range`, `Extent2D`), `assert` for
  the rest. Never put a braced initializer inside an `assert` argument: the preprocessor splits it
  on the commas. Bind the value to a named `constexpr` local first.
- Optimised paths are compared against their scalar references; hot kernels print timings so a
  regression is visible in the CI log.
- Negative paths are tested through the `try_` and `bool`-returning variants. Panics are not tested.
- Test files use root-relative includes, `std::printf`, and `core/file` for fixtures.

## Refactoring discipline

- Change one module per commit. After each stage: full build, every CTest suite, and a pixel
  comparison of fixed-time captures (belt, Earth, tour) against the previous stage. A refactor that
  changes pixels must say so in its commit message and explain why (the float culling did).
- Keep the deterministic entry points (`--time`, `--bookmark`, `--frames`, `--no-hud`) working; they
  are the regression harness. Compare with identical flags, or the comparison measures the HUD.
- When a rewrite touches a call site the formatter later reflows, anchor edits on a stable token,
  not on a whole line; several stage scripts failed on a reflowed line and had to be rerun.
