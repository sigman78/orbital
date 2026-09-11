#include "materials.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace space::assets {
namespace {

std::uint8_t linear_byte(std::uint8_t value) {
    static const auto table = [] {
        std::array<std::uint8_t, 256> result{};
        for (unsigned i = 0; i < result.size(); ++i) {
            const double s = i / 255.0;
            const double linear = s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
            result[i] = static_cast<std::uint8_t>(std::lround(std::clamp(linear, 0.0, 1.0) * 255.0));
        }
        return result;
    }();
    return table[value];
}

Image downsample(const Image& source, bool normal_map) {
    if (source.width == 1 && source.height == 1)
        return source;
    Image out{std::max(1u, source.width / 2), std::max(1u, source.height / 2), {}};
    out.pixels.resize(static_cast<std::size_t>(out.width) * out.height * 4);
    for (std::uint32_t y = 0; y < out.height; ++y) {
        for (std::uint32_t x = 0; x < out.width; ++x) {
            unsigned sums[4]{};
            double nx = 0, ny = 0, nz = 0;
            for (unsigned dy = 0; dy < 2; ++dy) {
                const auto sy = std::min(source.height - 1, y * 2 + dy);
                for (unsigned dx = 0; dx < 2; ++dx) {
                    // Horizontal wrap makes equirectangular planetary textures seam-safe.
                    const auto sx = (x * 2 + dx) % source.width;
                    const auto q = (static_cast<std::size_t>(sy) * source.width + sx) * 4;
                    for (unsigned c = 0; c < 4; ++c)
                        sums[c] += source.pixels[q + c];
                    if (normal_map) {
                        nx += source.pixels[q] / 127.5 - 1.0;
                        ny += source.pixels[q + 1] / 127.5 - 1.0;
                        nz += source.pixels[q + 2] / 127.5 - 1.0;
                    }
                }
            }
            const auto q = (static_cast<std::size_t>(y) * out.width + x) * 4;
            if (normal_map) {
                const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (length > 1e-10) {
                    nx /= length;
                    ny /= length;
                    nz /= length;
                } else {
                    nx = ny = 0;
                    nz = 1;
                }
                out.pixels[q] = static_cast<std::uint8_t>(std::lround((nx * .5 + .5) * 255));
                out.pixels[q + 1] = static_cast<std::uint8_t>(std::lround((ny * .5 + .5) * 255));
                out.pixels[q + 2] = static_cast<std::uint8_t>(std::lround((nz * .5 + .5) * 255));
            } else {
                for (unsigned c = 0; c < 3; ++c)
                    out.pixels[q + c] = static_cast<std::uint8_t>((sums[c] + 2) / 4);
            }
            out.pixels[q + 3] = static_cast<std::uint8_t>((sums[3] + 2) / 4);
        }
    }
    return out;
}

} // namespace

std::vector<Image> load_material(const std::filesystem::path& path, MaterialEncoding encoding, bool luminance_to_alpha,
                                 bool normal_map) {
    if (normal_map && luminance_to_alpha)
        throw std::invalid_argument("material cannot be both a normal map and luminance-to-alpha mask");
    Image base = load_png(path);
    for (std::size_t q = 0; q < base.pixels.size(); q += 4) {
        if (luminance_to_alpha) {
            // Rec.709 luma from encoded source bytes is used as authored mask coverage.
            const unsigned luma = 54u * base.pixels[q] + 183u * base.pixels[q + 1] + 19u * base.pixels[q + 2];
            base.pixels[q] = base.pixels[q + 1] = base.pixels[q + 2] = 255;
            base.pixels[q + 3] = static_cast<std::uint8_t>((luma + 128) >> 8);
        } else if (encoding == MaterialEncoding::SRGB) {
            base.pixels[q] = linear_byte(base.pixels[q]);
            base.pixels[q + 1] = linear_byte(base.pixels[q + 1]);
            base.pixels[q + 2] = linear_byte(base.pixels[q + 2]);
        }
    }
    std::vector<Image> mips;
    mips.push_back(std::move(base));
    while (mips.back().width > 1 || mips.back().height > 1)
        mips.push_back(downsample(mips.back(), normal_map));
    return mips;
}

} // namespace space::assets
