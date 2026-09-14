#include "assets/texture.hpp"
#include "core/file.hpp"
#include "core/log.hpp"
#include "core/panic.hpp"
#include <algorithm>
#include <limits>

namespace space::assets {
namespace {
constexpr std::uint32_t cache_magic = 0x5845544f; // OTEX
constexpr std::uint32_t cache_version = 1;
constexpr std::size_t header_size = 64;
std::uint32_t word(ByteView b, std::size_t offset) {
    return std::uint32_t(b[offset]) | (std::uint32_t(b[offset + 1]) << 8) | (std::uint32_t(b[offset + 2]) << 16) |
           (std::uint32_t(b[offset + 3]) << 24);
}
void put(Bytes& b, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        b[offset + i] = std::uint8_t(value >> (8 * i));
}
std::uint64_t mip_size(Extent2D e, TextureFormat format, unsigned bx = 4, unsigned by = 4) {
    return format != TextureFormat::RGBA8 ? std::uint64_t((e.width + bx - 1) / bx) * ((e.height + by - 1) / by) * 16
                                          : std::uint64_t(e.width) * e.height * 4;
}
} // namespace

TextureData texture_from_image(Rgba8Image image) {
    ORBITAL_ASSERT(image.valid());
    TextureData result;
    result.mips.push_back({image.extent, std::move(image.pixels)});
    return result;
}

TextureData texture_from_images(MipChain images) {
    TextureData result;
    result.mips.reserve(images.size());
    for (auto& image : images) {
        ORBITAL_ASSERT(image.valid());
        result.mips.push_back({image.extent, std::move(image.pixels)});
    }
    return result;
}

bool valid_texture(const TextureData& texture) {
    if (texture.mips.empty() || (texture.format != TextureFormat::RGBA8 && texture.format != TextureFormat::BC7 &&
                                 texture.format != TextureFormat::ASTC))
        return false;
    const unsigned block = texture.block_x;
    if (block != texture.block_y || (block != 4 && block != 6 && block != 8 && block != 12) ||
        (texture.format == TextureFormat::BC7 && block != 4))
        return false;
    Extent2D expected = texture.mips.front().extent;
    if (!valid_image_extent(expected))
        return false;
    for (std::size_t level = 0; level < texture.mips.size(); ++level) {
        const auto& mip = texture.mips[level];
        if (mip.extent != expected || mip.bytes.size() != mip_size(expected, texture.format, block, block))
            return false;
        if (expected.width == 1 && expected.height == 1 && level + 1 != texture.mips.size())
            return false;
        expected = {std::max(1u, expected.width / 2), std::max(1u, expected.height / 2)};
    }
    return true;
}
std::uint32_t material_flags(const MaterialDesc& d) {
    return (d.encoding == MaterialEncoding::SRGB ? 1u : 0u) | (d.normal_map ? 2u : 0u) |
           (d.luminance_to_alpha ? 4u : 0u);
}
std::filesystem::path texture_cache_path(const std::filesystem::path& source) {
    auto result = source;
    result.replace_extension(".otex");
    return result;
}

std::filesystem::path texture_cache_path(const std::filesystem::path& source, TextureFormat format) {
    auto result = source;
    result.replace_extension(format == TextureFormat::BC7    ? ".bc7.otex"
                             : format == TextureFormat::ASTC ? ".astc.otex"
                                                             : ".rgba8.otex");
    return result;
}

std::optional<TextureData> read_texture_cache(ByteView b, const MaterialDesc& desc) {
    if (b.size() < header_size || word(b, 0) != cache_magic || word(b, 4) != cache_version ||
        word(b, 32) != material_flags(desc))
        return std::nullopt;
    TextureData result;
    const auto format = word(b, 28);
    if (format != unsigned(TextureFormat::ASTC) && format != unsigned(TextureFormat::RGBA8) &&
        format != unsigned(TextureFormat::BC7))
        return std::nullopt;
    result.format = TextureFormat(format);
    Extent2D extent{word(b, 8), word(b, 12)};
    if (!valid_image_extent(extent))
        return std::nullopt;
    result.block_x = word(b, 20);
    result.block_y = word(b, 24);
    if (result.block_x != result.block_y ||
        (result.block_x != 4 && result.block_x != 6 && result.block_x != 8 && result.block_x != 12))
        return std::nullopt;
    if (result.format == TextureFormat::BC7 && result.block_x != 4)
        return std::nullopt;
    unsigned levels = 1;
    for (auto size = std::max(extent.width, extent.height); size > 1; size /= 2)
        ++levels;
    if (word(b, 16) != levels)
        return std::nullopt;
    std::size_t offset = header_size;
    for (unsigned level = 0; level < levels; ++level) {
        const auto size = mip_size(extent, result.format, result.block_x, result.block_y);
        if (size > b.size() - offset)
            return std::nullopt;
        result.mips.push_back({extent, Bytes(b.begin() + offset, b.begin() + offset + std::size_t(size))});
        offset += std::size_t(size);
        extent = {std::max(1u, extent.width / 2), std::max(1u, extent.height / 2)};
    }
    if (offset != b.size())
        return std::nullopt;
    return result;
}

Bytes write_texture_cache(const TextureData& texture, const MaterialDesc& desc, std::uint64_t source_hash) {
    if (!valid_texture(texture) || texture.mips.back().extent.width != 1 || texture.mips.back().extent.height != 1)
        return {};
    Bytes result(header_size);
    const auto base = texture.mips.front().extent;
    put(result, 0, cache_magic);
    put(result, 4, cache_version);
    put(result, 8, base.width);
    put(result, 12, base.height);
    put(result, 16, unsigned(texture.mips.size()));
    put(result, 20, texture.block_x);
    put(result, 24, texture.block_y);
    put(result, 28, unsigned(texture.format));
    put(result, 32, material_flags(desc));
    put(result, 36, std::uint32_t(source_hash));
    put(result, 40, std::uint32_t(source_hash >> 32));
    for (const auto& mip : texture.mips)
        result.insert(result.end(), mip.bytes.begin(), mip.bytes.end());
    // Legacy checksum fields remain zero; runtime validates structure, not payload identity.
    return result;
}

std::optional<TextureData> load_texture_cache(const std::filesystem::path& source, const MaterialDesc& desc,
                                              const TextureSupport& support) {
    const auto supported = [&](TextureFormat format, unsigned block) {
        return format == TextureFormat::RGBA8 || (format == TextureFormat::BC7 && support.bc7) ||
               (format == TextureFormat::ASTC && block < support.astc_blocks.size() && support.astc_blocks[block]);
    };
    // The last candidate preserves caches generated before format-specific filenames.
    const std::array<std::optional<TextureFormat>, 4> candidates{TextureFormat::BC7, TextureFormat::ASTC,
                                                                 TextureFormat::RGBA8, std::nullopt};
    for (const auto candidate : candidates) {
        if (candidate == TextureFormat::BC7 && !support.bc7)
            continue;
        if (candidate == TextureFormat::ASTC &&
            std::none_of(support.astc_blocks.begin(), support.astc_blocks.end(), [](bool value) { return value; }))
            continue;
        const auto path = candidate ? texture_cache_path(source, *candidate) : texture_cache_path(source);
        const auto bytes = file::read(path);
        if (!bytes)
            continue;
        if (bytes->size() >= header_size) {
            const auto format = TextureFormat(word(*bytes, 28));
            if ((candidate && format != *candidate) || !supported(format, word(*bytes, 20)))
                continue;
        }
        if (auto result = read_texture_cache(*bytes, desc))
            return result;
        log::warn("Ignoring invalid texture cache: {}", path.string());
    }
    return std::nullopt;
}
} // namespace space::assets
