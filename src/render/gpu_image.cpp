#include "render/gpu_image.hpp"
#include "core/panic_if.hpp"

namespace space::render {

GpuImage::~GpuImage() {
    reset();
}

GpuImage& GpuImage::operator=(GpuImage&& other) noexcept {
    if (this != &other) {
        reset();
        heap_ = std::exchange(other.heap_, {});
        texture_ = std::exchange(other.texture_, nullptr);
        view_ = std::exchange(other.view_, nullptr);
        bytes_ = std::exchange(other.bytes_, 0);
    }
    return *this;
}

void GpuImage::reset() noexcept {
    gpu::destroy_render_view(std::exchange(view_, nullptr));
    gpu::destroy_texture(std::exchange(texture_, nullptr));
    gpu::destroy_texture_heap(std::exchange(heap_, {}));
    bytes_ = 0;
}

GpuImage GpuImage::create(gpu::Device* device, const ImageDesc& desc) {
    const gpu::TextureDesc texture_desc{.extent = {desc.extent.width, desc.extent.height, 1},
                                        .mip_levels = desc.mips,
                                        .format = desc.format,
                                        .usage = desc.usage};
    const auto size = gpu::get_texture_size_align(device, texture_desc);
    GpuImage result;
    result.heap_ = gpu::create_texture_heap(device, size.size);
    result.bytes_ = size.size;
    result.texture_ = gpu::create_texture(device, texture_desc, result.heap_, 0);
    panic_if(!result.texture_, "texture allocation failed ({}x{}, {} mips)", desc.extent.width, desc.extent.height,
             desc.mips);
    const auto attachment_bits = static_cast<unsigned>(gpu::TextureUsage::color_attachment |
                                                       gpu::TextureUsage::depth_stencil_attachment);
    if (static_cast<unsigned>(desc.usage) & attachment_bits) {
        result.view_ = gpu::create_render_view(result.texture_);
        panic_if(!result.view_, "attachment view allocation failed ({}x{})", desc.extent.width, desc.extent.height);
    }
    return result;
}

} // namespace space::render
