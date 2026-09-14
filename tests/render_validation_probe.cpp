#include "core/panic.hpp"
#include "render/gpu_commands.hpp"
#include "render/gpu_image.hpp"
#include "render/gpu_owners.hpp"
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>
#include <cstring>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<space::render::GpuImage>);
static_assert(std::is_nothrow_move_constructible_v<space::render::GpuImage>);
static_assert(std::is_nothrow_move_assignable_v<space::render::FrameTargets>);

static void clear_pass(gpu::CommandBuffer* cmd, gpu::RenderView* view) {
    gpu::ColorAttachment color{.render_view = view, .load = gpu::LoadOp::clear, .clear = {1, 0, 0, 1}};
    space::render::RenderPassScope pass(cmd, {.colors = {&color, 1}});
    return; // The subsequent transfer must be outside rendering.
}

// Negative control for the validation runner, not a normal CTest test. Two
// identical writes deliberately omit their dependency with --missing-barrier.
int main(int argc, char** argv) {
    const bool missing = argc == 2 && std::strcmp(argv[1], "--missing-barrier") == 0;
    auto init = gpu::create_device();
    ORBITAL_ASSERT(init.device);
    using space::render::GpuImage;
    const space::render::ImageDesc image_desc{
        .extent = {16, 16},
        .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment | gpu::TextureUsage::transfer_source};
    auto first = GpuImage::create(init.device, image_desc);
    const auto* texture = first.texture();
    const auto* view = first.view();
    auto moved = std::move(first);
    ORBITAL_ASSERT(!first.texture() && !first.view());
    ORBITAL_ASSERT(moved.texture() == texture && moved.view() == view);
    auto replacement = GpuImage::create(init.device, image_desc);
    auto source_owner = space::render::UniqueGpuHeap::create(init.device, 16);
    auto moved_heap = std::move(source_owner);
    ORBITAL_ASSERT(!source_owner.range().gpu);
    auto target_owner = space::render::UniqueGpuHeap::create(init.device, 16, gpu::MemoryType::readback);
    auto image_readback = space::render::UniqueGpuHeap::create(init.device, 16 * 16 * 4, gpu::MemoryType::readback);
    const auto source = moved_heap.get(), target = target_owner.get();
    ORBITAL_ASSERT(source.range.cpu && target.range.cpu);
    std::memset(source.range.cpu, 42, 16);
    auto* cmd = gpu::begin_commands(init.device);
    clear_pass(cmd, moved.view());
    gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::transfer,
                 gpu::Access::transfer_read);
    gpu::copy_texture_to_memory(cmd, moved.texture(), gpu::gpu_range(image_readback.get()));
    gpu::copy_memory(cmd, gpu::gpu_range(source), gpu::gpu_range(target));
    if (!missing)
        gpu::barrier(cmd, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::transfer,
                     gpu::Access::transfer_write);
    gpu::copy_memory(cmd, gpu::gpu_range(source), gpu::gpu_range(target));
    gpu::barrier(cmd, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::host, gpu::Access::host_read);
    space::render::SubmissionTimeline timeline;
    timeline.initialize(init.device);
    timeline.submit_and_wait({cmd});
    ORBITAL_ASSERT(timeline.last_submission().value == 1);
    for (unsigned i = 0; i < 16 * 16; ++i) {
        const auto* pixel = image_readback.range().cpu + i * 4;
        ORBITAL_ASSERT(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255);
    }
    ORBITAL_ASSERT(std::memcmp(source.range.cpu, target.range.cpu, 16) == 0);
    gpu::wait_idle(init.device);
    // Both images' initial layout commands have completed before replacement.
    replacement = std::move(moved);
    ORBITAL_ASSERT(!moved.texture() && !moved.view());
    ORBITAL_ASSERT(replacement.texture() == texture && replacement.view() == view);
    auto& same = replacement;
    replacement = std::move(same);
    ORBITAL_ASSERT(replacement.texture() == texture && replacement.view() == view);
    replacement.reset();
    replacement.reset();
    ORBITAL_ASSERT(!replacement.texture() && !replacement.view());
    target_owner = std::move(moved_heap); // replacement after GPU completion
    target_owner.reset();
    target_owner.reset();
    image_readback.reset();
    timeline.reset();
    gpu::destroy_device(init.device);
}
