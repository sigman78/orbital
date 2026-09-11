#include "../src/assets/materials.hpp"

#include <array>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string_view>

int main() {
    using namespace space::assets;
    struct MaterialCase {
        const char* name;
        MaterialEncoding encoding;
        bool mask;
        bool normal;
    };
    constexpr std::array cases{
        MaterialCase{"earth_albedo.png", MaterialEncoding::SRGB, false, false},
        MaterialCase{"earth_clouds.png", MaterialEncoding::Linear, true, false},
        MaterialCase{"earth_night.png", MaterialEncoding::SRGB, false, false},
        MaterialCase{"earth_normal.png", MaterialEncoding::Linear, false, true},
        MaterialCase{"earth_specular.png", MaterialEncoding::Linear, false, false},
        MaterialCase{"gas_albedo.png", MaterialEncoding::SRGB, false, false},
        MaterialCase{"moon_albedo.png", MaterialEncoding::SRGB, false, false},
        MaterialCase{"rock_albedo.png", MaterialEncoding::SRGB, false, false},
        MaterialCase{"rock_normal.png", MaterialEncoding::Linear, false, true},
        MaterialCase{"rock_roughness.png", MaterialEncoding::Linear, false, false},
    };
    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path() / "assets" / "materials";
    for (const auto& item : cases) {
        const auto mips = load_material(root / item.name, item.encoding, item.mask, item.normal);
        assert(!mips.empty());
        assert(mips.front().width > 0 && mips.front().height > 0);
        assert(mips.back().width == 1 && mips.back().height == 1);
        for (std::size_t level = 0; level < mips.size(); ++level) {
            const auto& image = mips[level];
            assert(image.pixels.size() == std::size_t(image.width) * image.height * 4);
            if (level) {
                assert(image.width == std::max(1u, mips[level - 1].width / 2));
                assert(image.height == std::max(1u, mips[level - 1].height / 2));
            }
        }
        if (item.mask) {
            bool varied_alpha = false;
            const auto& pixels = mips.front().pixels;
            for (std::size_t q = 0; q < pixels.size(); q += 4) {
                assert(pixels[q] == 255 && pixels[q + 1] == 255 && pixels[q + 2] == 255);
                varied_alpha |= pixels[q + 3] != pixels[3];
            }
            assert(varied_alpha);
        }
        if (item.normal) {
            // Every generated level (the authored base is preserved) contains
            // unit-length vectors within RGBA8 quantization tolerance.
            for (std::size_t level = 1; level < mips.size(); ++level) {
                const auto& pixels = mips[level].pixels;
                for (std::size_t q = 0; q < pixels.size(); q += 4) {
                    const double nx = pixels[q] / 127.5 - 1.0;
                    const double ny = pixels[q + 1] / 127.5 - 1.0;
                    const double nz = pixels[q + 2] / 127.5 - 1.0;
                    const double length2 = nx * nx + ny * ny + nz * nz;
                    assert(length2 > .97 && length2 < 1.03);
                }
            }
        }
        const std::string_view name(item.name);
        if (name.find("roughness") != std::string_view::npos || name.find("specular") != std::string_view::npos) {
            std::uint8_t minimum = 255, maximum = 0;
            const auto& pixels = mips.front().pixels;
            for (std::size_t q = 0; q < pixels.size(); q += 4) {
                assert(pixels[q] == pixels[q + 1] && pixels[q] == pixels[q + 2]);
                minimum = std::min(minimum, pixels[q]);
                maximum = std::max(maximum, pixels[q]);
            }
            assert(minimum < maximum); // Map retained a meaningful shader range.
        }
        std::cout << item.name << ' ' << mips.front().width << 'x' << mips.front().height << '\n';
    }
}
