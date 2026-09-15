# Scene, renderer and asset API review

Reviewed revision: `bc8db88`, 2026-09-13. Findings and line references below describe that baseline.

Follow-up: C1-C5 have been addressed in the working tree, along with duplicate galaxy resizing, UI font/index contract enforcement and checked file completion. Broader interface and ownership work remains planned. See [REFACTOR_PLAN.md](REFACTOR_PLAN.md) for exact scope, validation and the updated implementation sequence; the original findings below are retained as review evidence.

Three delegated reviews used GPT-5.6 Sol for assets and renderer, and GPT-5.6 Luna for scene/app orchestration. The primary review reconciled findings against the source, measured complexity, inspected the resource destruction backend and ran small C++ probes. Focus is `src/scene`, `src/render`, `src/assets` and their application boundaries. Platform/core received a boundary and hotspot scan, not a comprehensive backend audit. Previously documented TAA algorithm issues are not repeated here.

The strongest theme is implicit contracts: image layout, scene roles, resource ownership and settings propagation depend on conventions that types do not express. Splitting files helped navigation, but several responsibilities still share one unrestricted state object. The right next step is stronger boundaries around existing concepts, followed by focused decomposition.

## Correctness findings first

### C1 — Dust controls are discarded when preparing the frame

**High confidence; current user-visible bug.** [main.cpp:433](../src/app/main.cpp#L433) initializes the full `FrameInput::belt_dust` aggregate from `app.belt_dust.enabled`, a bool. Only its first member is initialized from UI state; all the remaining fields receive defaults. [ui.cpp:129](../src/app/ui.cpp#L129) updates density, brightness, far brightness, tint and saturation, and frame packing reads those fields, but the values never reach it.

A standalone C++ probe reproducing the same aggregate initialization printed `density 7.0 -> 1.0; tint 0.2 -> 1.0`. The enable toggle works, which conceals the rest of the broken controls.

Pass the complete settings value. Extract a small app-to-render frame-input builder and test it with deliberately non-default values in every settings group. This is a better regression test than testing each slider implementation. It also illustrates the risk of terse aggregate construction across a large settings boundary.

### C2 — The final galaxy render target is omitted from destruction

**High confidence; resource lifetime bug established from source.** `galaxy` is allocated by [resize/resize_galaxy](../src/render/renderer_resources.cpp#L257), but the destruction inventory at [renderer_resources.cpp:27](../src/render/renderer_resources.cpp#L27) omits it. Resize destroys previous instances correctly; final renderer shutdown does not explicitly release the final texture, attachment view and heap. The backend's device destructor cleans its own internal objects, not the application's texture wrappers.

Add the missing cleanup immediately, then remove the need to keep independent owning-resource inventories synchronized. This is a shutdown lifetime violation, not evidence of an accumulating per-frame leak or the earlier FPS concern. A GPU validation/lifetime run was not performed for this review.

### C3 — CLI bookmarks leave the Orbit target at Earth

**High confidence; current behavior bug.** [initial_state](../src/app/main.cpp#L341) changes the camera for `--bookmark`, but leaves `selected_body` at zero. Starting with `--bookmark 3` frames Mars; pressing `O` subsequently orbits Earth. The keyboard and panel bookmark handlers correctly update both fields ([main.cpp:238](../src/app/main.cpp#L238), [ui.cpp:291](../src/app/ui.cpp#L291)).

Centralize the semantic operation `select_bookmark(AppState&, index)` and use it from startup, keys and buttons. Share transitions, not arbitrary UI callbacks. Test that CLI and interactive selection establish the same selected body and camera state.

### C4 — Scene validation accepts values that produce invalid render data

**High confidence; reproduced public-API defect, not an observed default-scene failure.** [validate_system](../src/scene/system.cpp#L158) omits finite checks for rotation phase, axial tilt and rotation period. Its belt check compares `thickness < 0` without checking finiteness. NaN thickness therefore passes. [evaluate_system](../src/scene/system.cpp#L194) can return a non-finite rotation despite having accepted the scene as valid.

Compiled probes using the current scene library produced:

```text
NaN belt thickness validation errors: 0
NaN rotation phase validation errors: 0; states returned: 6; rotation finite: 0
```

Complete finite validation for consumed fields and define the intended zero/negative rotation-period policy. Preserve intentional stationary bodies if period zero is supported; do not arbitrarily reject them. Add parameterized invalid-input tests, then distinguish validated scene construction from per-frame evaluation.

### C5 — Public image/texture aggregates bypass assumed invariants

**High confidence; unsafe API precondition, current decoder callers normally satisfy it.** [Image](../src/assets/image.hpp#L12) permits an extent unrelated to its byte vector. [prepare_material](../src/assets/materials.cpp#L14) sends its pointer and extent-derived count directly to conversion/downsampling kernels without checking storage. A 4×4 image with empty pixels can cause out-of-bounds access. The kernel header says sizes have already been validated; this boundary does not enforce that assertion.

Validate nonempty dimensions and expected RGBA8 byte length before calling kernels. An owning type with controlled construction or a checked view can enforce this once. Do not add bounds checks inside every SIMD loop.

The same family of invariant gap exists in [write_texture_cache](../src/assets/texture.cpp#L102) and [upload_images](../src/render/renderer_resources.cpp#L85): arbitrary `TextureData` can reach serialization/upload with inconsistent format, mip dimensions or payload lengths. The cache reader performs strong checks; preserve them and share layout validation with construction/writer/upload boundaries where useful.

## Interface and responsibility findings

### D1 — Image interpretation, storage and ownership need separate names

The user's example is well chosen. There is one useful nuance: current `Image` is documented as **tightly packed, non-premultiplied RGBA8**, and PNG decoding deliberately normalizes input to that representation ([image.hpp](../src/assets/image.hpp#L11), [image.cpp:73](../src/assets/image.cpp#L73)). It is not intended to hold arbitrary channel layouts. The problem is that its generic name and freely mutable fields do not express or enforce this specialization.

Renaming it `RawImage` or `ImageData` alone would leave interpretation ambiguous. Two reasonable designs are an explicitly specialized owner, `Rgba8Image`, or a general image descriptor with validated format metadata. For this codebase, start with a specialized material-processing owner and a small borrowed image view at module boundaries:

```cpp
enum class PixelLayout { Gray8, GrayAlpha8, Rgb8, Rgba8 };
struct ImageView {
    Extent2D extent;
    PixelLayout layout;
    std::size_t row_stride;
    ByteView pixels;
};
bool save_png(const std::filesystem::path& path, ImageView image);
```

This is an interface sketch, not a complete validated implementation. Channel count and minimum row size should be derived from layout; do not allow contradictory format/channel fields. Two-channel PNG means grayscale plus alpha, not an arbitrary red/green image. Add stride only with well-defined range validation and a tightly packed convenience constructor. Compressed BC7/ASTC blocks should not pretend to be directly PNG-encodable pixels. Color encoding and alpha interpretation must remain explicit at conversion boundaries; storage layout alone does not tell whether bytes are linear or sRGB.

[save_png](../src/assets/image.cpp#L127) currently takes four correlated arguments. Its encoder does not need ownership, so an image **view** is the appropriate input, with an owning-image convenience overload if useful. The current RGB screenshot explains why simply taking `const Image&` would be too restrictive. The same view concept can remove the raw pointer/width/height bundle in [Ui::FontAtlas](../src/app/ui.hpp#L25) and [Renderer::set_ui_font](../src/render/renderer.hpp#L63). Keep the view in a lower shared layer if platform and assets both need it; do not create a new sideways dependency to share it.

### D2 — Mip layout should not force one owner per level

`MipChain = vector<Image>` and `TextureData::mips = vector<TextureMip>` duplicate the shape of an owning mip chain ([image.hpp:21](../src/assets/image.hpp#L21), [texture.hpp:9](../src/assets/texture.hpp#L9)). `texture_from_images` **moves** the pixel buffers, so the conversion is not itself a full pixel copy. Cache loading, however, copies payload slices into separately allocated mip vectors ([texture.cpp:89](../src/assets/texture.cpp#L89)). The comment calling Image the only owning pixel container is already inaccurate.

Separate ownership from layout now so consumers accept views. A later contiguous owner can contain one `Bytes` slab plus `{extent, offset, length}` mip records and return views on demand. Store offsets rather than self-referential spans: copying/moving/reallocating a slab owner must not leave dangling views stored inside it. A borrowed cache view also needs its file buffer to outlive upload. Preserve that contract explicitly.

This permits both today's vector-backed storage and single-allocation compressed payloads without rewriting algorithms. Whether to consolidate allocations immediately is a profiling decision; introducing a view boundary is independently worthwhile. Avoid a polymorphic image hierarchy or adding virtual allocation interfaces for these two storage choices.

### D3 — Renderer depends upward on application policy

[renderer.hpp:2](../src/render/renderer.hpp#L2) includes `app/camera.hpp`; [renderer_resources.cpp:3](../src/render/renderer_resources.cpp#L3) includes `app/hud.hpp` and generates the app's HUD during GPU initialization. Both contradict the layer direction described in `CODE_STYLE.md`.

The renderer needs camera pose, projection parameters and a cut identifier, not bookmarks, tour controls or navigation methods. Supply a render-facing camera value or move the shared camera-view value below app, while app retains the controller. App should create HUD content and supply an image view to the renderer. A pure `make_frame_input` adapter then provides a testable place to catch C1 without coupling renderer tests to navigation/UI.

### D4 — A generic-looking scene API conceals a showcase-specific contract

[Renderer::init](../src/render/renderer_resources.cpp#L206) requires terrestrial, gas-giant and desert roles, always uses the first belt, and treats that belt as attached to the giant. Additional validated belts disappear; a first belt parented elsewhere is still positioned relative to the giant. The API's `SystemDescription` suggests more flexibility than exists. `FrameInput` says “at least the three major bodies,” while [draw](../src/render/renderer_frame.cpp#L30) requires exact count equality. IDs exist, but render state is paired by index without verifying ID/order correspondence. Bookmarks also contain positional body indices.

Prefer an explicit validated `ShowcaseBindings`/render-scene description: resolved role indices, belt parent and supported cardinality. Validate unsupported combinations before GPU initialization. Keep contiguous indices in the hot path; ID-to-index resolution belongs at scene setup. A full ECS, dynamic scene graph or hash map per lookup is not justified by the current eight-body bound.

Related misleading fields: `scale_policy`, `atmosphere_scale`, and star intensity/temperature are exposed by the scene description but are not consumed by the renderer as their names suggest. Some may intentionally be descriptive metadata; distinguish that from render parameters rather than silently promising an effect. Do not remove `material_seed` wholesale: it is consumed for moonlet geometry.

### D5 — File decomposition has not established ownership boundaries

`renderer_impl.hpp` is 421 lines and every renderer implementation file can mutate the same resource, scene, upload, history, exposure and statistics state. The pass-family split improves navigation, but it is still one broad implementation object ([renderer_impl.hpp:239](../src/render/renderer_impl.hpp#L239)).

Extract cohesive owners where the lifetime is already real: render targets, loaded scene assets, and temporal/exposure state. Make owning GPU images move-only, with borrowed handles distinguished from owners. Keep synchronization explicit: RAII alone does not make destruction safe while the GPU is using a resource. Current wait-before-destroy behavior must survive the change.

A shared target inventory or small target-owner aggregate should govern creation, resize and destruction. C2 is concrete evidence that three independent lists drift. [resize](../src/render/renderer_resources.cpp#L271) followed by [resize_galaxy](../src/render/renderer_resources.cpp#L257) also rebuilds the galaxy twice if extent and divisor change together. Reconcile both desired values in one target-update operation.

Do not replace the readable top-level frame sequence with a generic render graph just to shorten `Impl`. Keep pass recorders explicit and give them the small contexts they actually use. Also move the reusable fullscreen recorder out of the post-specific conceptual boundary if scene/belt passes continue to use it.

### D6 — Smaller lifetime/configuration gaps should be made explicit

- [set_ui_font](../src/render/renderer_overlay.cpp#L85) uploads and retains another GPU image on every call, while replacing the same descriptor. The header says one-time upload. Enforce one-shot use or give the font its own replaceable owner before supporting atlas/DPI rebuilds.
- [record_ui](../src/render/renderer_overlay.cpp#L73) always uses 16-bit GPU indices but allocates/copies according to `sizeof(ImDrawIdx)`. The default build uses 16-bit indices; a 32-bit ImGui configuration would render incorrectly. Pin the supported configuration with a static assertion or select the index type at compile time.
- [file::write](../src/core/file.cpp#L51) returns success before `File`'s destructor calls `fclose`, and ignores close/flush errors. A buffered final write failure can therefore be reported as successful by `save_png` or benchmark/cache writers. This is source-established error-path behavior, not a reproduced storage failure. Provide an explicit checked finish operation where a bool promises complete write success.

## Complexity and terseness

Measured with Lizard 1.24.0 over `src/**/*.cpp`. CCN is the tool's lexical cyclomatic-complexity estimate, including conditional expressions/boolean decisions; it is not runtime cost or a bug score. NLOC excludes blank/comment-only lines. Raw results are in `.scratch/code-quality-metrics.json`.

| Function | Location | CCN | NLOC | Assessment |
| --- | --- | ---: | ---: | --- |
| `build_frame` | renderer_frame_data.cpp:171 | 49 | 147 | Highest-value decomposition: camera/history, belt geometry/LOD, lighting and shader-field packing |
| `parse_options` | app/main.cpp:114 | 47 | 86 | Many repetitive option branches; use grouped parsing helpers or modest descriptor tables |
| `draw_panel` | app/ui.cpp:80 | 37 | 239 | Independent settings sections; extract section functions and share semantic actions |
| `validate_system` | scene/system.cpp:158 | 31 | 35 | High number largely comes from guards; complete validation before reducing branch count |
| `frame_loop` | app/main.cpp:381 | 27 | 95 | Preserve ordering; extract event actions, frame-input assembly and measurement/capture bookkeeping |
| `read_texture_cache` | assets/texture.cpp:58 | 27 | 43 | Defensive parsing is valuable; factor format/layout rules, not every guard |
| `handle_key` | app/main.cpp:201 | 24 | 44 | Share transitions with UI/startup; simple toggles may use a table |
| `load_materials` | renderer_assets.cpp:115 | 15 | 69 | Separate decode/cache/fallback jobs from GPU upload and capability discovery |
| `record_ui` | renderer_overlay.cpp:28 | 13 | 56 | Mostly necessary command iteration; clarify layout/configuration contracts |
| `Renderer::draw` | renderer_frame.cpp:27 | 11 | 100 | Long because it states ordering; not a high-priority split by length alone |

Terseness is most harmful when it conceals a domain decision: `Frame.quality.x`, `.belt_dust = enabled`, or an implicit first-belt/giant association. Short mathematical names inside a small projection helper and raw pointers inside SIMD kernels are often appropriate. Expand names at boundaries, keep local mathematical code compact, and move packing into a clearly labeled ABI adapter. A thousand-line replacement using tiny one-line wrappers would not improve this code.

## Repetition worth factoring, and what to retain

| Pattern | Focused extraction | Avoid |
| --- | --- | --- |
| Bookmark selection in startup, keys, UI | One semantic state transition; shared bookmark names/role descriptors | Separate slightly different handlers |
| Target creation/binding/destruction | Cohesive target owners or one inventory, desired extent/divisor reconciliation | A whole render graph for a fixed pass order |
| Image dimensions/layout/bytes across PNG, fonts, uploads | Checked image/texture views; storage ownership separate | One class pretending compressed blocks are RGBA pixels |
| `build_frame` decisions mixed with float4 packing | Pure camera/belt/light calculations, then ABI packing | Exporting shader slot conventions into scene/app |
| Material cache/fallback/decode/upload | CPU preparation returning typed results; GPU upload consumes them | A generic task graph for one bounded startup batch |
| Per-frame scene validation and fresh state vector | Validate immutable topology at setup; evaluate into reusable output | Replacing tiny bounded linear searches with infrastructure |

Preserve deterministic scene evaluation, the small bounded parent-resolution algorithm, normalized RGBA material kernels, scalar SIMD reference tests, rigorous cache-reader validation, enum-backed shader slots/layout assertions, bounded staging uploads, and the existing explicit frame order. `fullscreen_pass` already factors a genuinely repeated pattern. Shadow, alpha-blended scene, depth-tested splat metadata and UI passes differ enough that forcing them through one configurable mega-helper would reintroduce complexity.

## Suggested commit sequence

1. Fix dust settings propagation and bookmark selection; add app-state/frame-adapter tests with non-default settings.
2. Fix galaxy cleanup, pin UI index configuration and define font replacement semantics. Validate create/destroy balance and shutdown with GPU validation enabled.
3. Close scene/image/texture validation holes with negative tests. Separate generic scene validity from the showcase renderer's requirements.
4. Introduce checked image views and an explicit RGBA owner name; adapt PNG/font boundaries. Keep current mip allocation strategy initially. Add grayscale, gray-alpha, RGB, RGBA and stride-validation tests according to supported formats.
5. Introduce a render-facing camera snapshot and app-supplied HUD; remove renderer-to-app includes. Extract frame-input mapping and shared bookmark actions.
6. Consolidate render-target ownership/update, then decompose frame calculations and settings sections. Keep source changes per concern and commits reviewable.
7. Consider a contiguous mip owner and additional pass abstractions only where measurements or new consumers justify them.

For behavior-preserving renderer refactors, use the existing fixed-time Earth/belt/tour captures and the performance suite against both the prior milestone and original baseline. Bugs that intentionally change pixels or controls need explicit before/after expectations. Do not trade correctness checks for lower CCN. The original documentation-only review used independently compiled C++ probes; subsequent fix validation is recorded in the refactor plan.
