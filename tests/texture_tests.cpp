#include "assets/texture.hpp"
#include "assets/image_io.hpp"
#include "assets/material_catalog.hpp"
#include "core/file.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>

int main() {
    using namespace space;
    using namespace space::assets;
    const auto directory = std::filesystem::temp_directory_path() /
                           ("orbital-texture-" +
                            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto source = directory / "earth_normal.png";
    Rgba8Image image{{7, 5}, Bytes(7 * 5 * 4)};
    for (std::size_t i = 0; i < image.pixel_count(); ++i) {
        image.pixels[i * 4] = 128;
        image.pixels[i * 4 + 1] = 128;
        image.pixels[i * 4 + 2] = 255;
        image.pixels[i * 4 + 3] = std::uint8_t(i * 7);
    }
    assert(save_png(source, image.view()));
    const auto desc = material_description("earth_normal.png");
    assert(desc.normal_map);
    assert(material_description("earth_albedo.png").encoding == MaterialEncoding::SRGB);
    assert(material_description("earth_clouds.png").luminance_to_alpha);
    constexpr std::uint64_t hash = 123; // opaque build provenance, ignored by runtime
    const auto mips = load_material(source, desc);
    const auto rgba = texture_from_images(mips);
    assert(valid_texture(rgba));
    auto single_image = image;
    const auto* pixels = single_image.pixels.data();
    const auto single = texture_from_image(std::move(single_image));
    assert(valid_texture(single) && single.format == TextureFormat::RGBA8 && single.mips.size() == 1);
    assert(single.mips.front().extent == image.extent && single.mips.front().bytes == image.pixels);
    assert(single.mips.front().bytes.data() == pixels); // transfers the pixel allocation without copying
    auto malformed = rgba;
    malformed.mips.front().bytes.pop_back();
    assert(!valid_texture(malformed) && write_texture_cache(malformed, desc).empty());
    malformed = rgba;
    malformed.mips[1].extent.width++;
    assert(!valid_texture(malformed) && write_texture_cache(malformed, desc).empty());
    malformed = rgba;
    malformed.format = TextureFormat(99);
    assert(!valid_texture(malformed) && write_texture_cache(malformed, desc).empty());
    malformed = rgba;
    malformed.block_x = 5;
    assert(!valid_texture(malformed) && write_texture_cache(malformed, desc).empty());
    malformed = rgba;
    malformed.mips.push_back(malformed.mips.back());
    assert(!valid_texture(malformed));
    auto partial = rgba;
    partial.mips.resize(1);
    assert(valid_texture(partial)); // valid upload, incomplete serialized cache
    assert(write_texture_cache(partial, desc).empty());
    // Synthetic block payload exercises cache parsing independently of the optional encoder.
    TextureData encoded{.format = TextureFormat::ASTC, .mips = {}};
    for (const auto& mip : mips)
        encoded.mips.push_back(
            {mip.extent, Bytes(std::size_t((mip.extent.width + 3) / 4) * ((mip.extent.height + 3) / 4) * 16, 0xfc)});
    auto bc7 = encoded;
    bc7.format = TextureFormat::BC7;
    TextureSupport support{.bc7 = true};
    support.astc_blocks[4] = true;
    for (const auto& texture : {rgba, encoded, bc7}) {
        const auto bytes = write_texture_cache(texture, desc);
        const auto decoded = read_texture_cache(bytes, desc);
        assert(decoded && decoded->format == texture.format);
        for (std::size_t i = 0; i < texture.mips.size(); ++i) {
            assert(decoded->mips[i].extent == texture.mips[i].extent);
            assert(decoded->mips[i].bytes == texture.mips[i].bytes);
        }
        assert(!read_texture_cache(bytes, {}));
        for (const auto offset : {0u, 4u, 8u, 12u, 16u, 20u, 24u, 28u, 32u}) {
            auto bad = bytes;
            bad[offset] ^= 0xff;
            assert(!read_texture_cache(bad, desc));
        }
        auto legacy = bytes;
        for (const auto offset : {36u, 40u, 44u, 48u, 64u})
            legacy[offset] ^= 0xff;
        assert(read_texture_cache(legacy, desc)); // provenance, checksum and pixel changes are not structural errors
        for (const auto size : {std::size_t(0), std::size_t(63), bytes.size() - 1})
            assert(!read_texture_cache(ByteView(bytes).first(size), desc));
        auto trailing = bytes;
        trailing.push_back(0);
        assert(!read_texture_cache(trailing, desc));
    }
    assert(!load_texture_cache(source, desc, support)); // source-only install
    const auto cache = texture_cache_path(source);
    assert(file::write(cache, write_texture_cache(encoded, desc)));
    assert(load_texture_cache(source, desc, support));
    const auto bc7_path = texture_cache_path(source, TextureFormat::BC7);
    const auto astc_path = texture_cache_path(source, TextureFormat::ASTC);
    const auto rgba_path = texture_cache_path(source, TextureFormat::RGBA8);
    assert(file::write(bc7_path, write_texture_cache(bc7, desc)));
    assert(file::write(astc_path, write_texture_cache(encoded, desc)));
    assert(file::write(rgba_path, write_texture_cache(rgba, desc)));
    assert(load_texture_cache(source, desc, support)->format == TextureFormat::BC7);
    support.bc7 = false;
    assert(load_texture_cache(source, desc, support)->format == TextureFormat::ASTC);
    assert(load_texture_cache(source, desc)->format == TextureFormat::RGBA8);
    support.bc7 = true;
    assert(file::write_text(bc7_path, "corrupt"));
    assert(load_texture_cache(source, desc, support)->format == TextureFormat::ASTC);
    assert(file::write(bc7_path, write_texture_cache(bc7, desc, hash + 1)));
    assert(load_texture_cache(source, desc, support)->format ==
           TextureFormat::BC7);                                     // provenance does not affect priority
    assert(file::write(bc7_path, write_texture_cache(rgba, desc))); // mislabeled file
    assert(load_texture_cache(source, desc, support)->format == TextureFormat::ASTC);
    bc7.block_x = bc7.block_y = 6;
    assert(!read_texture_cache(write_texture_cache(bc7, desc), desc));
    for (const auto& path : {bc7_path, astc_path, rgba_path})
        std::filesystem::remove(path);
    assert(!load_texture_cache(source, desc)); // legacy ASTC unsupported
    image.pixels[0] = 42;
    assert(save_png(source, image.view()));
    assert(load_texture_cache(source, desc, support)); // PNG edits do not override prepared assets
    std::filesystem::remove(source);
    assert(load_texture_cache(source, desc, support)); // cache-only distribution
    assert(file::write_text(cache, "truncated cache"));
    assert(!load_texture_cache(source, desc, support));
    std::filesystem::remove(cache);
    std::filesystem::remove(directory);
}
