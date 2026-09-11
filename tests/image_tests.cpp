#include "assets/image.hpp"
#include "core/file.hpp"
#include "core/types.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <vector>

int main() {
    using namespace space::assets;
    const std::uint32_t width = 7, height = 5;
    const space::Extent2D extent{width, height};
    space::Bytes rgba(width * height * 4);
    for (std::uint32_t i = 0; i < width * height; ++i) {
        rgba[i * 4] = static_cast<std::uint8_t>(i * 3);
        rgba[i * 4 + 1] = static_cast<std::uint8_t>(255 - i);
        rgba[i * 4 + 2] = static_cast<std::uint8_t>(i * 7);
        rgba[i * 4 + 3] = i % 2 ? 255 : 64;
    }
    const auto path = std::filesystem::temp_directory_path() / "orbital_image_test.png";

    // RGBA round trip is lossless.
    assert(save_png(path, extent, 4, rgba));
    const auto loaded = load_png(path);
    assert(loaded.extent == extent);
    assert(loaded.pixels == rgba);

    // RGB input is expanded to opaque RGBA on load.
    space::Bytes rgb(width * height * 3);
    for (std::uint32_t i = 0; i < width * height; ++i)
        for (unsigned c = 0; c < 3; ++c)
            rgb[i * 3 + c] = rgba[i * 4 + c];
    assert(save_png(path, extent, 3, rgb));
    const auto expanded = load_png(path);
    assert(expanded.extent == extent);
    for (std::uint32_t i = 0; i < width * height; ++i) {
        for (unsigned c = 0; c < 3; ++c)
            assert(expanded.pixels[i * 4 + c] == rgba[i * 4 + c]);
        assert(expanded.pixels[i * 4 + 3] == 255);
    }

    // A missing file and a non-PNG file are reported, not fatal, on the try_ path.
    std::filesystem::remove(path);
    assert(!try_load_png(path));
    assert(space::file::write_text(path, "not a png"));
    assert(!try_load_png(path));
    std::filesystem::remove(path);
    std::printf("image tests passed\n");
    return 0;
}
