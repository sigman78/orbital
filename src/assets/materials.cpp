#include "assets/materials.hpp"
#include "assets/image_io.hpp"

#include "assets/kernels.hpp"
#include "core/panic.hpp"

#include <algorithm>

namespace space::assets {

MipChain load_material(const std::filesystem::path& path, const MaterialDesc& desc) {
    return prepare_material(load_png(path), desc);
}

MipChain prepare_material(Rgba8Image base, const MaterialDesc& desc) {
    ORBITAL_ASSERT(base.valid());
    ORBITAL_ASSERT(!(desc.normal_map && desc.luminance_to_alpha));
    if (desc.luminance_to_alpha)
        kernels::luminance_to_alpha(base.pixels.data(), base.pixel_count());
    else if (desc.encoding == MaterialEncoding::SRGB)
        kernels::srgb_to_linear(base.pixels.data(), base.pixel_count());

    MipChain mips;
    std::size_t levels = 1;
    for (std::uint32_t extent = std::max(base.extent.width, base.extent.height); extent > 1; extent /= 2)
        ++levels;
    mips.reserve(levels);
    mips.push_back(std::move(base));
    while (mips.back().extent.width > 1 || mips.back().extent.height > 1) {
        const Rgba8Image& previous = mips.back();
        Rgba8Image next{{kernels::half_extent(previous.extent.width), kernels::half_extent(previous.extent.height)},
                        {}};
        next.pixels.resize(next.pixel_count() * 4);
        (desc.normal_map ? kernels::downsample_normals : kernels::downsample_rgba8)(
            previous.pixels.data(), previous.extent.width, previous.extent.height, next.pixels.data());
        mips.push_back(std::move(next));
    }
    return mips;
}

} // namespace space::assets
