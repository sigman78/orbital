#include "assets/image.hpp"
#include "core/file.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <vector>

int main() {
    using namespace space::assets;
    const std::uint32_t width = 7, height = 5;
    std::vector<std::uint8_t> rgba(width * height * 4);
    for (std::uint32_t i = 0; i < width * height; ++i) {
        rgba[i * 4] = static_cast<std::uint8_t>(i * 3);
        rgba[i * 4 + 1] = static_cast<std::uint8_t>(255 - i);
        rgba[i * 4 + 2] = static_cast<std::uint8_t>(i * 7);
        rgba[i * 4 + 3] = i % 2 ? 255 : 64;
    }
    const auto path = std::filesystem::temp_directory_path() / "orbital_image_test.png";

    // RGBA round trip is lossless.
    assert(save_png(path, width, height, 4, rgba.data()));
    const auto loaded = load_png(path);
    assert(loaded.width == width && loaded.height == height);
    assert(loaded.pixels == rgba);

    // RGB input is expanded to opaque RGBA on load.
    std::vector<std::uint8_t> rgb(width * height * 3);
    for (std::uint32_t i = 0; i < width * height; ++i)
        for (unsigned c = 0; c < 3; ++c)
            rgb[i * 3 + c] = rgba[i * 4 + c];
    assert(save_png(path, width, height, 3, rgb.data()));
    const auto expanded = load_png(path);
    assert(expanded.width == width && expanded.height == height);
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
