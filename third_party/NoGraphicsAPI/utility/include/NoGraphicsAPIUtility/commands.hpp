#pragma once
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>
#include <assert.h>

namespace gpu
{

class [[nodiscard]] RenderPassScope
{
public:
    RenderPassScope(CommandBuffer* commands, const RenderingDesc& desc) noexcept : commands_(commands)
    {
        begin_render_pass(commands_, desc);
    }
    ~RenderPassScope() noexcept
    {
        end_render_pass(commands_);
    }
    RenderPassScope(const RenderPassScope&) = delete;
    RenderPassScope& operator=(const RenderPassScope&) = delete;
    RenderPassScope(RenderPassScope&&) = delete;
    RenderPassScope& operator=(RenderPassScope&&) = delete;

private:
    CommandBuffer* commands_;
};

// Submission is explicit; destruction never submits or waits. Complete all uses
// before reset/destruction, and destroy this owner before its device.
class SubmissionTimeline
{
public:
    constexpr SubmissionTimeline() noexcept = default;
    ~SubmissionTimeline() noexcept
    {
        reset();
    }
    SubmissionTimeline(const SubmissionTimeline&) = delete;
    SubmissionTimeline& operator=(const SubmissionTimeline&) = delete;
    SubmissionTimeline(SubmissionTimeline&&) = delete;
    SubmissionTimeline& operator=(SubmissionTimeline&&) = delete;

    void initialize(Device* device) noexcept
    {
        assert(!timeline_);
        timeline_ = create_timeline_semaphore(device);
    }
    void reset() noexcept
    {
        destroy_timeline_semaphore(timeline_);
        timeline_ = nullptr;
        serial_ = 0;
    }
    constexpr TimelinePoint last_submission() const noexcept
    {
        return {timeline_, serial_};
    }
    [[nodiscard]] TimelinePoint submit(Span<CommandBuffer* const> commands) noexcept
    {
        assert(timeline_);
        ++serial_;
        gpu::submit(commands, last_submission());
        return last_submission();
    }
    void submit_and_present(Device* device, Span<CommandBuffer* const> commands) noexcept
    {
        assert(timeline_);
        ++serial_;
        gpu::submit_and_present(device, commands, last_submission());
    }
    void wait(TimelinePoint point) const noexcept
    {
        assert(point.semaphore == timeline_ && point.value <= serial_);
        wait_timeline(point);
    }
    void wait_last() const noexcept
    {
        wait(last_submission());
    }
    void submit_and_wait(Span<CommandBuffer* const> commands) noexcept
    {
        wait(submit(commands));
    }

private:
    TimelineSemaphore* timeline_ = nullptr;
    uint64 serial_ = 0;
};

} // namespace gpu
