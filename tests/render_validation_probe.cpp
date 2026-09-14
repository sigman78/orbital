#include "core/panic.hpp"
#include "render/gpu_image.hpp"
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>
#include <cstring>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<space::render::GpuImage>);
static_assert(std::is_nothrow_move_constructible_v<space::render::GpuImage>);
static_assert(std::is_nothrow_move_assignable_v<space::render::FrameTargets>);

// Negative control for the validation runner, not a normal CTest test. Two
// identical writes deliberately omit their dependency with --missing-barrier.
int main(int argc, char** argv) {
    const bool missing = argc == 2 && std::strcmp(argv[1], "--missing-barrier") == 0;
    auto init = gpu::create_device();
    ORBITAL_ASSERT(init.device);
    using space::render::GpuImage;
    const space::render::ImageDesc image_desc{
        .extent = {16, 16}, .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment};
    auto first = GpuImage::create(init.device, image_desc);
    const auto* texture = first.texture();
    const auto* view = first.view();
    auto moved = std::move(first);
    ORBITAL_ASSERT(!first.texture() && !first.view());
    ORBITAL_ASSERT(moved.texture() == texture && moved.view() == view);
    auto replacement = GpuImage::create(init.device, image_desc);
    auto source = gpu::create_gpu_heap(init.device, 16);
    auto target = gpu::create_gpu_heap(init.device, 16, gpu::MemoryType::readback);
    ORBITAL_ASSERT(source.range.cpu && target.range.cpu);
    std::memset(source.range.cpu, 42, 16);
    auto* cmd = gpu::begin_commands(init.device);
    gpu::copy_memory(cmd, gpu::gpu_range(source), gpu::gpu_range(target));
    if (!missing)
        gpu::barrier(cmd, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::transfer,
                     gpu::Access::transfer_write);
    gpu::copy_memory(cmd, gpu::gpu_range(source), gpu::gpu_range(target));
    gpu::barrier(cmd, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::host, gpu::Access::host_read);
    auto* timeline = gpu::create_timeline_semaphore(init.device);
    gpu::submit({cmd}, {timeline, 1});
    gpu::wait_timeline({timeline, 1});
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
    gpu::destroy_gpu_heap(target);
    gpu::destroy_gpu_heap(source);
    gpu::destroy_timeline_semaphore(timeline);
    gpu::destroy_device(init.device);
}
