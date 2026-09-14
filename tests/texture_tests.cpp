#include "assets/texture.hpp"
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
    Image image{{7, 5}, Bytes(7 * 5 * 4)};
    for (std::size_t i = 0; i < image.pixel_count(); ++i) {
        image.pixels[i * 4] = 128;
        image.pixels[i * 4 + 1] = 128;
        image.pixels[i * 4 + 2] = 255;
        image.pixels[i * 4 + 3] = std::uint8_t(i * 7);
    }
    assert(save_png(source, image.extent, 4, image.pixels));
    const auto desc = material_description("earth_normal.png");
    assert(desc.normal_map);
    assert(material_description("earth_albedo.png").encoding == MaterialEncoding::SRGB);
    assert(material_description("earth_clouds.png").luminance_to_alpha);
    const auto png = file::read(source);
    assert(png);
    const auto hash = texture_hash(*png);
    const auto mips = load_material(source, desc);
    const auto rgba = texture_from_images(mips);
    assert(valid_texture(rgba));
    auto malformed = rgba;
    malformed.mips.front().bytes.pop_back();
    assert(!valid_texture(malformed) && write_texture_cache(malformed, desc, hash).empty());
    malformed = rgba;
    malformed.mips[1].extent.width++;
    assert(!valid_texture(malformed) && write_texture_cache(malformed, desc, hash).empty());
    malformed = rgba;
    malformed.format = TextureFormat(99);
    assert(!valid_texture(malformed) && write_texture_cache(malformed, desc, hash).empty());
    malformed = rgba;
    malformed.block_x = 5;
    assert(!valid_texture(malformed) && write_texture_cache(malformed, desc, hash).empty());
    malformed = rgba;
    malformed.mips.push_back(malformed.mips.back());
    assert(!valid_texture(malformed));
    auto partial = rgba;
    partial.mips.resize(1);
    assert(valid_texture(partial)); // valid upload, incomplete serialized cache
    assert(write_texture_cache(partial, desc, hash).empty());
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
        const auto bytes = write_texture_cache(texture, desc, hash);
        const auto decoded = read_texture_cache(bytes, desc, hash);
        assert(decoded && decoded->format == texture.format);
        for (std::size_t i = 0; i < texture.mips.size(); ++i) {
            assert(decoded->mips[i].extent == texture.mips[i].extent);
            assert(decoded->mips[i].bytes == texture.mips[i].bytes);
        }
        assert(!read_texture_cache(bytes, desc, hash + 1));
        assert(!read_texture_cache(bytes, {}, hash));
        for (const auto offset : {0u, 4u, 8u, 12u, 16u, 20u, 24u, 28u, 32u, 36u, 40u, 64u}) {
            auto bad = bytes;
            bad[offset] ^= 0xff;
            assert(!read_texture_cache(bad, desc, hash));
        }
        for (const auto size : {std::size_t(0), std::size_t(63), bytes.size() - 1})
            assert(!read_texture_cache(ByteView(bytes).first(size), desc, hash));
        auto trailing = bytes;
        trailing.push_back(0);
        assert(!read_texture_cache(trailing, desc, hash));
    }
    assert(!load_texture_cache(source, desc, support)); // source-only install
    const auto cache = texture_cache_path(source);
    assert(file::write(cache, write_texture_cache(encoded, desc, hash)));
    assert(load_texture_cache(source, desc, support));
    const auto bc7_path = texture_cache_path(source, TextureFormat::BC7);
    const auto astc_path = texture_cache_path(source, TextureFormat::ASTC);
    const auto rgba_path = texture_cache_path(source, TextureFormat::RGBA8);
    assert(file::write(bc7_path, write_texture_cache(bc7, desc, hash)));
    assert(file::write(astc_path, write_texture_cache(encoded, desc, hash)));
    assert(file::write(rgba_path, write_texture_cache(rgba, desc, hash)));
    assert(load_texture_cache(source, desc, support)->format == TextureFormat::BC7);
    support.bc7 = false;
    assert(load_texture_cache(source, desc, support)->format == TextureFormat::ASTC);
    assert(load_texture_cache(source, desc)->format == TextureFormat::RGBA8);
    support.bc7 = true;
    assert(file::write_text(bc7_path, "corrupt"));
    assert(load_texture_cache(source, desc, support)->format == TextureFormat::ASTC);
    assert(file::write(bc7_path, write_texture_cache(bc7, desc, hash + 1)));
    assert(load_texture_cache(source, desc, support)->format == TextureFormat::ASTC);
    assert(file::write(bc7_path, write_texture_cache(rgba, desc, hash))); // mislabeled file
    assert(load_texture_cache(source, desc, support)->format == TextureFormat::ASTC);
    bc7.block_x = bc7.block_y = 6;
    assert(!read_texture_cache(write_texture_cache(bc7, desc, hash), desc, hash));
    for (const auto& path : {bc7_path, astc_path, rgba_path})
        std::filesystem::remove(path);
    assert(!load_texture_cache(source, desc)); // legacy ASTC unsupported
    image.pixels[0] = 42;
    assert(save_png(source, image.extent, 4, image.pixels));
    assert(!load_texture_cache(source, desc, support)); // stale source
    std::filesystem::remove(source);
    assert(load_texture_cache(source, desc, support)); // cache-only distribution
    assert(file::write_text(cache, "truncated cache"));
    assert(!load_texture_cache(source, desc, support));
    std::filesystem::remove(cache);
    std::filesystem::remove(directory);
}
