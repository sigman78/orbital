#include "assets/materials.hpp"

#include "assets/kernels.hpp"
#include "core/panic.hpp"

#include <algorithm>

namespace space::assets {

std::vector<Image> load_material(const std::filesystem::path& path, const MaterialDesc& desc) {
    ORBITAL_ASSERT(!(desc.normal_map && desc.luminance_to_alpha));
    Image base = load_png(path);
    const std::size_t pixel_count = static_cast<std::size_t>(base.width) * base.height;
    if (desc.luminance_to_alpha)
        kernels::luminance_to_alpha(base.pixels.data(), pixel_count);
    else if (desc.encoding == MaterialEncoding::SRGB)
        kernels::srgb_to_linear(base.pixels.data(), pixel_count);

    std::vector<Image> mips;
    std::size_t levels = 1;
    for (std::uint32_t extent = std::max(base.width, base.height); extent > 1; extent /= 2)
        ++levels;
    mips.reserve(levels);
    mips.push_back(std::move(base));
    while (mips.back().width > 1 || mips.back().height > 1) {
        const Image& previous = mips.back();
        Image next{kernels::half_extent(previous.width), kernels::half_extent(previous.height), {}};
        next.pixels.resize(static_cast<std::size_t>(next.width) * next.height * 4);
        (desc.normal_map ? kernels::downsample_normals : kernels::downsample_rgba8)(
            previous.pixels.data(), previous.width, previous.height, next.pixels.data());
        mips.push_back(std::move(next));
    }
    return mips;
}

} // namespace space::assets
