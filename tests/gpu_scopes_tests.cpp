#include "render/gpu_commands.hpp"
#include "render/gpu_owners.hpp"
#include "render/gpu_timing.hpp"
#include <array>
#include <cassert>
#include <type_traits>

namespace {
unsigned begun = 0, ended = 0, heaps_destroyed = 0, psos_destroyed = 0, waits = 0, submissions = 0;
unsigned timelines_destroyed = 0;
gpu::uint64 signaled = 0, waited = 0;
gpu::uint64 clock_ticks = 230;
unsigned timestamp_writes = 0;
gpu::CommandBuffer* timestamp_commands = nullptr;
std::array<gpu::uint64, space::render::GpuTimings::timestamp_count> timestamp_storage{};
} // namespace

// CPU-only API doubles check wrapper sequencing without a device or driver.
namespace gpu {
void write_timestamp(CommandBuffer* commands, uint64* destination, Stage) noexcept {
    assert(commands == timestamp_commands);
    clock_ticks = (clock_ticks + 10) & 255;
    *destination = clock_ticks;
    ++timestamp_writes;
}

void begin_render_pass(CommandBuffer*, const RenderingDesc&) noexcept {
    ++begun;
}
void end_render_pass(CommandBuffer*) noexcept {
    ++ended;
}
const DeviceCaps& get_device_caps(const Device*) noexcept {
    static const auto caps = [] {
        DeviceCaps result{};
        result.timestamp_period_ns = 1e6f;
        result.timestamp_valid_bits = 8;
        return result;
    }();
    return caps;
}
GpuHeap create_gpu_heap(Device*, uint64 size, MemoryType) noexcept {
    return {.range = {.cpu = reinterpret_cast<byte*>(timestamp_storage.data()),
                      .gpu = reinterpret_cast<byte*>(timestamp_storage.data()),
                      .size = size}};
}
void destroy_gpu_heap(const GpuHeap& heap) noexcept {
    if (heap.range.size)
        ++heaps_destroyed;
}
void destroy_pso(PSO* pso) noexcept {
    if (pso)
        ++psos_destroyed;
}
TimelineSemaphore* create_timeline_semaphore(Device*, uint64) noexcept {
    return reinterpret_cast<TimelineSemaphore*>(1);
}
void destroy_timeline_semaphore(TimelineSemaphore* timeline) noexcept {
    if (timeline)
        ++timelines_destroyed;
}
void submit(Span<CommandBuffer* const>, TimelinePoint point) noexcept {
    ++submissions;
    signaled = point.value;
}
void submit_and_present(Device*, Span<CommandBuffer* const> commands, TimelinePoint point) noexcept {
    submit(commands, point);
}
void wait_timeline(TimelinePoint point) noexcept {
    ++waits;
    waited = point.value;
}
} // namespace gpu

static_assert(!std::is_copy_constructible_v<space::render::RenderPassScope>);
static_assert(!std::is_move_constructible_v<space::render::RenderPassScope>);
static_assert(!std::is_copy_constructible_v<space::render::UniqueGpuHeap>);
static_assert(std::is_nothrow_move_constructible_v<space::render::UniqueGpuHeap>);
static_assert(std::is_nothrow_move_constructible_v<space::render::UniquePso>);

void early_return() {
    space::render::RenderPassScope pass(nullptr, {});
    assert(begun == 1 && ended == 0);
    return;
}

void timed_early_return(space::render::GpuTimings& timings) {
    space::render::GpuTimingScope timing(timings, space::render::GpuPass::BodyShadows);
    return;
}

void timing_scopes() {
    using namespace space::render;
    static_assert(!std::is_copy_constructible_v<GpuTimingScope>);
    static_assert(!std::is_move_constructible_v<GpuTimingScope>);
    timestamp_commands = reinterpret_cast<gpu::CommandBuffer*>(1);
    GpuTimings timings;
    timings.initialize(nullptr);
    {
        GpuTimingFrame frame(timings, timestamp_commands);
        assert(!timings.recorded(GpuPass::Frame));
        timed_early_return(timings);
        assert(timings.recorded(GpuPass::BodyShadows));
    }
    assert(timestamp_writes == 4);
    assert(timings.milliseconds(GpuPass::Frame).value() == 30.f);
    assert(timings.milliseconds(GpuPass::BodyShadows).value() == 10.f);
    assert(!timings.milliseconds(GpuPass::BeltLight));
    // Reusing the readback allocation must not expose a skipped pass's old result.
    {
        GpuTimingFrame frame(timings, timestamp_commands);
        GpuTimingScope post(timings, GpuPass::Post);
    }
    assert(timestamp_writes == 8);
    assert(!timings.recorded(GpuPass::BodyShadows));
    assert(!timings.milliseconds(GpuPass::BodyShadows));
    assert(timings.milliseconds(GpuPass::Post).value() == 10.f);
    assert(waits == 3 && submissions == 3);
}

int main() {
    early_return();
    assert(ended == 1);
    {
        auto first = space::render::UniqueGpuHeap::create(nullptr, 16);
        auto moved = static_cast<space::render::UniqueGpuHeap&&>(first);
        assert(first.range().size == 0 && moved.range().size == 16 && heaps_destroyed == 0);
        auto replacement = space::render::UniqueGpuHeap::create(nullptr, 32);
        replacement = static_cast<space::render::UniqueGpuHeap&&>(moved);
        assert(moved.range().size == 0 && replacement.range().size == 16 && heaps_destroyed == 1);
        auto& same = replacement;
        replacement = static_cast<space::render::UniqueGpuHeap&&>(same);
        assert(heaps_destroyed == 1);
        replacement.reset();
        replacement.reset();
    }
    assert(heaps_destroyed == 2 && waits == 0);
    {
        space::render::UniquePso first(reinterpret_cast<gpu::PSO*>(1));
        space::render::UniquePso moved(static_cast<space::render::UniquePso&&>(first));
        space::render::UniquePso replacement(reinterpret_cast<gpu::PSO*>(2));
        replacement = static_cast<space::render::UniquePso&&>(moved);
        assert(!first.get() && !moved.get() && replacement.get() && psos_destroyed == 1);
        auto& same = replacement;
        replacement = static_cast<space::render::UniquePso&&>(same);
        replacement.reset();
        replacement.reset();
    }
    assert(psos_destroyed == 2 && waits == 0);
    {
        space::render::SubmissionTimeline timeline;
        timeline.initialize(nullptr);
        const auto first = timeline.submit({});
        assert(first.value == 1 && submissions == 1 && signaled == 1 && waits == 0);
        timeline.wait(first);
        assert(waited == 1);
        timeline.submit_and_present(nullptr, {});
        assert(signaled == 2 && waits == 1);
        timeline.submit_and_wait({});
        assert(signaled == 3 && waited == 3 && waits == 2);
        timeline.wait_last();
        assert(waited == 3 && waits == 3);
        timeline.reset();
        timeline.reset();
    }
    assert(timelines_destroyed == 1 && waits == 3 && submissions == 3);
    timing_scopes();
}
