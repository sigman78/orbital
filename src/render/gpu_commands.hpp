#pragma once
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>
#include <assert.h>

namespace space::render {

class [[nodiscard]] RenderPassScope {
public:
    RenderPassScope(gpu::CommandBuffer* commands, const gpu::RenderingDesc& desc) noexcept : commands_(commands) {
        gpu::begin_render_pass(commands_, desc);
    }
    ~RenderPassScope() noexcept { gpu::end_render_pass(commands_); }
    RenderPassScope(const RenderPassScope&) = delete;
    RenderPassScope& operator=(const RenderPassScope&) = delete;
    RenderPassScope(RenderPassScope&&) = delete;
    RenderPassScope& operator=(RenderPassScope&&) = delete;

private:
    gpu::CommandBuffer* commands_;
};

// Submission is explicit; destruction never submits or waits. Complete all uses
// before reset/destruction, and destroy this owner before its device.
class SubmissionTimeline {
public:
    constexpr SubmissionTimeline() noexcept = default;
    ~SubmissionTimeline() noexcept { reset(); }
    SubmissionTimeline(const SubmissionTimeline&) = delete;
    SubmissionTimeline& operator=(const SubmissionTimeline&) = delete;
    SubmissionTimeline(SubmissionTimeline&&) = delete;
    SubmissionTimeline& operator=(SubmissionTimeline&&) = delete;

    void initialize(gpu::Device* device) noexcept {
        assert(!timeline_);
        timeline_ = gpu::create_timeline_semaphore(device);
    }
    void reset() noexcept {
        gpu::destroy_timeline_semaphore(timeline_);
        timeline_ = nullptr;
        serial_ = 0;
    }
    constexpr gpu::TimelinePoint last_submission() const noexcept { return {timeline_, serial_}; }
    [[nodiscard]] gpu::TimelinePoint submit(gpu::Span<gpu::CommandBuffer* const> commands) noexcept {
        assert(timeline_);
        ++serial_;
        gpu::submit(commands, last_submission());
        return last_submission();
    }
    void submit_and_present(gpu::Device* device, gpu::Span<gpu::CommandBuffer* const> commands) noexcept {
        assert(timeline_);
        ++serial_;
        gpu::submit_and_present(device, commands, last_submission());
    }
    void wait(gpu::TimelinePoint point) const noexcept {
        assert(point.semaphore == timeline_ && point.value <= serial_);
        gpu::wait_timeline(point);
    }
    void wait_last() const noexcept { wait(last_submission()); }
    void submit_and_wait(gpu::Span<gpu::CommandBuffer* const> commands) noexcept { wait(submit(commands)); }

private:
    gpu::TimelineSemaphore* timeline_ = nullptr;
    gpu::uint64 serial_ = 0;
};

} // namespace space::render
