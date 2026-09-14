#include "core/panic.hpp"
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>
#include <cstring>

// Negative control for the validation runner, not a normal CTest test. Two
// identical writes deliberately omit their dependency with --missing-barrier.
int main(int argc, char** argv) {
    const bool missing = argc == 2 && std::strcmp(argv[1], "--missing-barrier") == 0;
    auto init = gpu::create_device();
    ORBITAL_ASSERT(init.device);
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
    gpu::destroy_gpu_heap(target);
    gpu::destroy_gpu_heap(source);
    gpu::destroy_timeline_semaphore(timeline);
    gpu::destroy_device(init.device);
}
