#pragma once
#include "core/extent.hpp"
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>
#include <cstdint>
#include <utility>

namespace space::render {

struct ImageDesc {
    Extent2D extent{1, 1};
    gpu::Format format = gpu::Format::rgba8_unorm;
    gpu::TextureUsage usage = gpu::TextureUsage::sampled;
    unsigned mips = 1;
};

// Owns the attachment view, texture and allocation, in that destruction order.
// The caller must complete GPU work before reset, replacement or destruction.
// Handle accessors return borrowed aliases; they never transfer ownership.
class GpuImage {
public:
    constexpr GpuImage() = default;
    GpuImage(const GpuImage&) = delete;
    GpuImage& operator=(const GpuImage&) = delete;
    constexpr GpuImage(GpuImage&& other) noexcept
        : heap_(std::exchange(other.heap_, {})), texture_(std::exchange(other.texture_, nullptr)),
          view_(std::exchange(other.view_, nullptr)), bytes_(std::exchange(other.bytes_, 0)) {}
    GpuImage& operator=(GpuImage&& other) noexcept;
    ~GpuImage();

    static GpuImage create(gpu::Device* device, const ImageDesc& desc);
    constexpr gpu::Texture* texture() const { return texture_; }
    constexpr gpu::RenderView* view() const { return view_; }
    constexpr std::uint64_t bytes() const { return bytes_; } // the allocation, 0 when empty
    void reset() noexcept;

private:
    gpu::TextureHeap heap_{};
    gpu::Texture* texture_ = nullptr;
    gpu::RenderView* view_ = nullptr;
    std::uint64_t bytes_ = 0;
};

// Replaced together on window resize, after a single GPU wait.
struct FrameTargets {
    GpuImage hdr, depth, sun_visibility, bloom_a, bloom_b, flare, final_image, ldr;
    GpuImage history[2], splat_mask, smaa_edges, smaa_weights, belt_dust, galaxy;
};

// Independent of the window size; released before the device at shutdown.
struct FixedTargets {
    GpuImage shadow_map, belt_light, belt_light_blur, belt_disc_light, belt_disc_rocks;
};

} // namespace space::render
