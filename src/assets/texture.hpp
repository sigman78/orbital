#pragma once
#include "assets/materials.hpp"
#include <array>
#include <optional>

namespace space::assets {

enum class TextureFormat : std::uint32_t { RGBA8 = 0, ASTC = 1, BC7 = 2 };
struct TextureMip {
    Extent2D extent;
    Bytes bytes;
};
struct TextureData {
    TextureFormat format = TextureFormat::RGBA8;
    std::vector<TextureMip> mips;
    unsigned block_x = 4, block_y = 4;
};

struct TextureSupport {
    bool bc7 = false;
    std::array<bool, 13> astc_blocks{};
};

TextureData texture_from_images(MipChain images);
std::uint64_t texture_hash(ByteView bytes);
std::uint32_t material_flags(const MaterialDesc& desc);
std::filesystem::path texture_cache_path(const std::filesystem::path& source);
std::filesystem::path texture_cache_path(const std::filesystem::path& source, TextureFormat format);
// Strictly validates our complete 2D texture cache, including source identity and payload checksum.
std::optional<TextureData> read_texture_cache(ByteView bytes, const MaterialDesc& desc,
                                              std::optional<std::uint64_t> source_hash);
Bytes write_texture_cache(const TextureData& texture, const MaterialDesc& desc, std::uint64_t source_hash);
std::optional<TextureData> load_texture_cache(const std::filesystem::path& source, const MaterialDesc& desc,
                                              const TextureSupport& support = {});

} // namespace space::assets
