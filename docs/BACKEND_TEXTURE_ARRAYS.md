# Texture arrays in the GPU layer: a second sampled-image binding of `Texture2DArray`

Design and task list for adding texture-array descriptors to the vendored GPU layer and its use in the
renderer. Written for handoff: a coding agent should be able to implement it from this document plus the
code it names. `docs/DYNAMIC_TERRAIN.md` depends on it.

## Goal

Shaders can sample `Texture2DArray<float4>` textures through the conventional descriptor backend, bound by
the renderer into a small, fixed table of array slots beside the existing table of 2D textures. The renderer
can create a layered image, upload one layer's region from a staging heap, and bind the image to an array
slot. Nothing about the existing 2D table, its slots, or any shader that uses it changes.

## What exists today (read these first)

The fork is `third_party/NoGraphicsAPI` (header `include/NoGraphicsAPI/NoGraphicsAPI.hpp`, source
`src/NoGraphicsAPI.cpp`), at upstream commit `8e414bd0a8010b9f721d06d470860e27aa69c071`, with every local
change recorded in `third_party/NoGraphicsAPI-compat.patch`. The demo requires its "conventional
descriptor backend" (`caps.conventional_descriptor_backend`, asserted in `src/render/renderer_resources.cpp`).

Already there, unused by the app:

- `TextureType::two_d_array` and `cube_array` (`NoGraphicsAPI.hpp` ~257). `TextureDesc.layer_count`
  (~486). `create_texture` fills `VkImageCreateInfo.arrayLayers` from it (`NoGraphicsAPI.cpp` ~2837).
- `write_texture_descriptor` (`NoGraphicsAPI.cpp` ~2978) in the conventional branch creates the sampled view
  with `to_vk_view(texture->type)`, so a `two_d_array` texture gets a `VK_IMAGE_VIEW_TYPE_2D_ARRAY` view, and
  honours `TextureDescriptorDesc.base_layer/layer_count`. It writes binding 0 of the one descriptor set at
  the index derived from the CPU descriptor address, and calls `vkUpdateDescriptorSets` at once.
- `copy_memory_to_texture` takes `TextureCopyDesc.base_slice/slice_count/offset/extent` and maps them to
  `VkBufferImageCopy` (~4054). Region uploads into one layer already work.
- Images stay in `VK_IMAGE_LAYOUT_GENERAL` for life after a one-time transition at creation (~3301); there
  is no per-subresource layout tracking, and the app synchronises with global barriers
  (`src/render/gpu_sync.hpp`, `synchronize`). Nothing here needs to change for arrays.

What blocks arrays:

- The conventional descriptor set has one sampled-image binding, binding 0, of
  `conventional_texture_descriptor_count` (56) entries (`NoGraphicsAPI.cpp` ~50 and ~2092), and binding 1 of
  4 samplers. The shaders declare it as `[[vk::binding(0, 0)]] Texture2D<float4> textures[ORBITAL_TEXTURE_COUNT]`
  (`shaders/scene/bindings.slang`). Vulkan allows a 2D-array view in a sampled-image binding, but Slang
  needs a declared type per binding, so array textures need their own binding.
- `src/render/gpu_image.hpp`'s `ImageDesc` has no type or layer count; `GpuImage::create` always makes a
  `two_d` texture. `Renderer::Impl::bind(Slot, const GpuImage&)` (`renderer_resources.cpp` ~87) writes the
  2D table only.
- `tools/check-shaders.py` holds `ORBITAL_TEXTURE_COUNT` (`shaders/resource_slots.h`) equal to the fork's
  constant, by regex on both integer literals.

## Design

- **Binding 2 of the conventional set**: `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE` with
  `conventional_texture_array_descriptor_count` (8) entries, `VK_SHADER_STAGE_ALL`, in the same set layout
  and pool. Bindings 0 and 1 unchanged. The set stays one set; `vkCmdBindDescriptorSets` is unchanged.
- **Device state**: `conventional_array_views[8]` and `conventional_array_view_textures[8]` beside the
  existing arrays; `destroy_texture` clears entries that reference the freed texture as it does for the 2D
  table; device destruction destroys the views.
- **API**: one new function in the header, near `write_texture_descriptor`:

  ```cpp
  // Conventional backend: binds a two_d_array or cube_array texture into the array table at index.
  void write_texture_array_descriptor(Device* device, uint32 index, const Texture* texture,
                                      const TextureDescriptorDesc& desc = {}) noexcept;
  ```

  It asserts the conventional backend, `index < conventional_texture_array_descriptor_count`, and
  `texture->type` is `two_d_array` or `cube_array`; creates the view as `write_texture_descriptor` does
  (destroying the previous one), and writes binding 2 at `index` with `vkUpdateDescriptorSets`. The
  non-conventional backend is out of scope: the function asserts there.
- **Caps**: `DeviceCaps` gains `texture_array_descriptor_count` (8 on the conventional backend, 0 otherwise)
  so the renderer can assert the table size it was built for.
- **Shader ABI**: `shaders/resource_slots.h` gains `ORBITAL_TEXTURE_ARRAY_COUNT 8` and the array slots
  (`TEX_ARRAY_TERRAIN_HEIGHT 0`, `TEX_ARRAY_TERRAIN_ALBEDO 1`, `TEX_ARRAY_TERRAIN_NORMAL 2`, the rest spare).
  `shaders/scene/bindings.slang` gains
  `[[vk::binding(2, 0)]] Texture2DArray<float4> textureArrays[ORBITAL_TEXTURE_ARRAY_COUNT];`.
  `tools/check-shaders.py` holds `ORBITAL_TEXTURE_ARRAY_COUNT` equal to the fork's new constant the way it
  holds the 2D count.
- **Renderer**: `ImageDesc` gains `unsigned layers = 1`; `GpuImage::create` sets `type = two_d_array` and
  `layer_count = layers` when `layers > 1` (attachment views are not created for layered images; assert
  the usage has no attachment bit). `enum class ArraySlot : unsigned { terrain_height = TEX_ARRAY_TERRAIN_HEIGHT, ...,
  count = ORBITAL_TEXTURE_ARRAY_COUNT }` in `renderer_impl.hpp` and
  `void bind(ArraySlot slot, const GpuImage& image)` calling the new function. A layer upload is
  `copy_memory_to_texture(cmd, source, image.texture(), {.base_slice = layer, .slice_count = 1, .extent = {w, h, 1}})`
  from a host-visible heap the caller owns, followed by the caller's `synchronize` for the reading stage.
- **Unbound slots**: every array slot must hold a valid descriptor before the first draw, as the 2D table
  does ("unused descriptor slots point at the first map so every binding is valid"). The renderer binds a
  1x1x1 placeholder array image to every array slot at start-up.
- **Compat patch**: regenerated by the procedure in `third_party/README.md` (clean checkout at the pinned
  commit, copy the tracked fork files over it, `git diff --binary`, verify with `git apply --check` in a
  second checkout). `docs/FOUNDATION.md`'s description of the set layout (binding 0 and 1) gains binding 2.

## Acceptance criteria

1. `tools/check-shaders.py` passes: both counts match the fork's constants, all shaders compile and
   validate.
2. A self-test in the app, `orbital --texture-array-test` (headless, exits after it): the renderer creates a
   4-layer 8x8 `r16_unorm` array image, uploads distinct values into each layer by region copy, binds it to
   an array slot, dispatches a tiny compute shader (`shaders/tests/array_probe.slang`) that reads
   `textureArrays[slot].Load(int4(x, y, layer, 0))` for each layer into a readback heap, and the app checks
   the values and returns non-zero on a mismatch. Runs clean under the Vulkan validation layer (the layer
   the check tool's `--validate` enables). The existing `gpu_scopes_tests` is a CPU-only double of the API
   and cannot host this; a real device needs the app's window-less path (`--headless`).
3. `tools/check.py --all --validate` is unchanged for every shot (the 2D table and every existing shader
   are untouched), and the validation layer reports nothing new.
4. `git apply --check third_party/NoGraphicsAPI-compat.patch` succeeds on a clean checkout of the pinned
   upstream commit and reproduces the vendored tree.
5. `docs/FOUNDATION.md` and `third_party/README.md` describe binding 2; a `docs/DECISIONS.md` entry records
   the change and why (one typed binding per Slang texture type; the array table sized 8 for the terrain's
   three arrays and spares).

## Tasks

1. **Fork**: the constant, the third binding and pool size in the set layout creation (~2092), the two
   device arrays, `write_texture_array_descriptor` (declare in the header beside `write_texture_descriptor`,
   implement beside it), `destroy_texture` and device teardown cleanup, `DeviceCaps.texture_array_descriptor_count`.
   Keep the upstream style (the fork's formatting, `require_vk`, `assert`).
2. **Shader ABI**: `resource_slots.h` count and slots, `bindings.slang` declaration, `check-shaders.py`
   second count check.
3. **Renderer**: `ImageDesc.layers`, `GpuImage::create`, `ArraySlot`, `bind(ArraySlot, ...)`, the placeholder
   array bound at start-up in `Renderer::Impl::init` after the material uploads, a layer-upload helper if the
   terrain work wants one (`upload_layer(cmd, GpuRange source, GpuImage&, unsigned layer, Extent2D)`).
4. **Self-test**: acceptance 2. Model the option on `--headless` and the shot machinery in
   `src/app/main.cpp` and `src/app/options.cpp`; the probe shader joins the shader build like the others
   (see `shaders/README.md`); the readback heap and its host-read barrier follow `buffers.cull_readback`.
5. **Patch and docs**: regenerate the compat patch; update `third_party/README.md`, `docs/FOUNDATION.md`,
   `docs/DECISIONS.md`.

Order: 1, then 2 and 3 in parallel, then 4, then 5. About half a day.

## Out of scope

- Bindless or descriptor-indexing changes; the non-conventional backend.
- Cube arrays in the renderer (the fork supports the type; nothing binds one).
- Per-subresource layout tracking or per-layer barriers: the fork keeps every image in the general layout
  and the app's global barriers cover layer uploads as they cover everything else.
