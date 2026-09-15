#pragma once
#include "core/timing.hpp"
#include "render/gpu_owners.hpp"
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>
#include <cassert>
#include <cstdint>
#include <optional>

namespace space::render {

enum class GpuPass : unsigned {
    Frame,
    CullAndShadows,
    Culling,
    BodyShadows,
    BeltLight,
    BeltDiscs,
    Surface,
    Atmosphere,
    Post,
    Meter, // the exposure histogram's slice for the frame and the cycle's copies
    // Children of the groups above, each one contiguous run of commands; a group's
    // remainder over its children is the barriers and transitions between them.
    SurfaceSky,    // the galaxy splat pass, the background and the stars
    SurfaceBodies, // the planet surfaces
    SurfaceRocks,  // the pooled rock multi-draw, meshes and splats together
    SurfaceClouds, // the cloud shell
    Atmospheres,   // the per-body shell marches
    BeltDust,      // the dust march, its upsample and the disc blend
    SplatMask,     // splat coverage and depth for TAA and sun visibility
    Temporal,      // the temporal resolve
    MotionStreaks,
    Bloom, // prefilter and both blurs
    SunVisibility,
    Flare,     // the quarter-resolution stack
    Composite, // tone map and the full-resolution lens work
    SpatialAA, // FXAA, or the three SMAA passes
    Present,   // the swapchain copy with the HUD and the panel
    Count
};

class GpuTimingScope;
class GpuTimingFrame;

// One recording at a time; read results only after its submission completes.
class GpuTimings {
public:
    static_assert(unsigned(GpuPass::Count) <= 32); // one bit per pass in recording masks
    static constexpr unsigned timestamp_count = 2 * unsigned(GpuPass::Count);
    constexpr GpuTimings() = default;
    GpuTimings(const GpuTimings&) = delete;
    GpuTimings& operator=(const GpuTimings&) = delete;

    void initialize(gpu::Device* device) {
        assert(!cmd_ && !storage_.range().size);
        const auto& caps = gpu::get_device_caps(device);
        period_ns_ = caps.timestamp_period_ns;
        valid_bits_ = caps.timestamp_valid_bits;
        storage_ = UniqueGpuHeap::create(device, timestamp_count * sizeof(gpu::uint64), gpu::MemoryType::readback);
        assert(storage_.range().cpu && storage_.range().gpu);
    }
    void reset() {
        assert(!cmd_ && !open_);
        storage_.reset();
        recorded_ = 0;
    }
    constexpr bool recorded(GpuPass pass) const { return (recorded_ & bit(pass)) != 0; }
    std::optional<float> milliseconds(GpuPass pass) const {
        assert(!cmd_);
        if (!recorded(pass))
            return std::nullopt;
        const auto* values = reinterpret_cast<const gpu::uint64*>(storage_.range().cpu);
        const auto slot = 2 * unsigned(pass);
        return float(timestamp_ticks(values[slot], values[slot + 1], valid_bits_)) * period_ns_ * 1e-6f;
    }

private:
    friend class GpuTimingScope;
    friend class GpuTimingFrame;
    void begin_frame(gpu::CommandBuffer* cmd) {
        assert(!cmd_ && !open_ && cmd && storage_.range().gpu);
        cmd_ = cmd;
        recorded_ = 0;
        begin(GpuPass::Frame);
    }
    void end_frame() {
        end(GpuPass::Frame);
        assert(!open_);
        cmd_ = nullptr;
    }
    static constexpr std::uint32_t bit(GpuPass pass) {
        assert(pass < GpuPass::Count);
        return std::uint32_t{1} << unsigned(pass);
    }
    void begin(GpuPass pass) {
        const auto mask = bit(pass);
        assert(cmd_ && !((recorded_ | open_) & mask));
        open_ |= mask;
        gpu::write_timestamp(cmd_, reinterpret_cast<gpu::uint64*>(storage_.range().gpu) + 2 * unsigned(pass));
    }
    void end(GpuPass pass) {
        const auto mask = bit(pass);
        assert(cmd_ && (open_ & mask));
        gpu::write_timestamp(cmd_, reinterpret_cast<gpu::uint64*>(storage_.range().gpu) + 2 * unsigned(pass) + 1);
        open_ &= ~mask;
        recorded_ |= mask;
    }
    gpu::CommandBuffer* cmd_ = nullptr;
    UniqueGpuHeap storage_;
    float period_ns_ = 0;
    unsigned valid_bits_ = 0;
    std::uint32_t open_ = 0, recorded_ = 0;
};

// Must close before submission; the frame scope also records the total interval.
class [[nodiscard]] GpuTimingFrame {
public:
    GpuTimingFrame(GpuTimings& timings, gpu::CommandBuffer* cmd) : timings_(timings) { timings_.begin_frame(cmd); }
    ~GpuTimingFrame() { timings_.end_frame(); }
    GpuTimingFrame(const GpuTimingFrame&) = delete;
    GpuTimingFrame& operator=(const GpuTimingFrame&) = delete;
    GpuTimingFrame(GpuTimingFrame&&) = delete;
    GpuTimingFrame& operator=(GpuTimingFrame&&) = delete;

private:
    GpuTimings& timings_;
};

// Records two timestamps; destruction does not submit or wait.
class [[nodiscard]] GpuTimingScope {
public:
    GpuTimingScope(GpuTimings& timings, GpuPass pass) : timings_(timings), pass_(pass) { timings_.begin(pass_); }
    ~GpuTimingScope() { timings_.end(pass_); }
    GpuTimingScope(const GpuTimingScope&) = delete;
    GpuTimingScope& operator=(const GpuTimingScope&) = delete;
    GpuTimingScope(GpuTimingScope&&) = delete;
    GpuTimingScope& operator=(GpuTimingScope&&) = delete;

private:
    GpuTimings& timings_;
    GpuPass pass_;
};

} // namespace space::render
