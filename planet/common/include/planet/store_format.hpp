#pragma once
#include "planet/body.hpp"
#include "planet/tile.hpp"

#include <bit>
#include <cstdint>
#include <type_traits>

// The tile store on disk: one body directory holding `store.planet` and a shard per face and
// level, `f{face}_l{level:02}.tiles`. Headers are written as these structs, little-endian, every
// byte of them set by the writer, so the sizes below are the format.
namespace planet::store {

static_assert(std::endian::native == std::endian::little, "the store is little-endian on disk");

constexpr std::uint32_t tag(const char (&text)[5]) {
    return std::uint32_t(text[0]) | std::uint32_t(text[1]) << 8 | std::uint32_t(text[2]) << 16 |
           std::uint32_t(text[3]) << 24;
}
inline constexpr std::uint32_t store_magic = tag("PLST");
inline constexpr std::uint32_t shard_magic = tag("PLSH");
inline constexpr std::uint32_t format_version = 1;
inline constexpr unsigned max_levels = LevelTable::count;
inline constexpr unsigned face_count = 6;

// store.planet: the body the tiles were sampled from and what the directory holds.
struct StoreHeader {
    std::uint32_t magic = store_magic;
    std::uint32_t version = format_version;
    std::uint32_t cooker = 0;    // the cooker version that wrote it; bumps when the same seed changes bytes
    std::uint32_t level_min = 0; // inclusive level range every face in face_mask has a shard for
    std::uint32_t level_max = 0;
    std::uint32_t face_mask = 0; // bit f: face f is present
    std::uint64_t seed = 0;      // the terrain's seed, which the reader matches against its body
    BodyShape shape;
    LevelTable levels;
    TileLayout layouts[max_levels]; // indexed by level; levels outside the range are zero
    std::uint32_t reserved = 0;     // the tail padding seed's alignment would add, made explicit
};
static_assert(std::is_trivially_copyable_v<StoreHeader>);
static_assert(sizeof(StoreHeader) == 616);

// Shard file: this header, the tiles' bytes in index order, then the index.
struct ShardHeader {
    std::uint32_t magic = shard_magic;
    std::uint32_t version = format_version;
    std::uint32_t cooker = 0;
    std::uint32_t face = 0;
    std::uint32_t level = 0;
    std::uint32_t tile_count = 0; // 4^level when the shard is complete
    std::uint32_t tile_bytes = 0; // TileLayout::bytes() of the level; every tile the same
    std::uint32_t reserved = 0;
    std::uint64_t seed = 0;
    std::uint64_t payload_offset = 0; // first tile's bytes
    std::uint64_t index_offset = 0;   // tile_count TileEntry, sorted by key
};
static_assert(std::is_trivially_copyable_v<ShardHeader>);
static_assert(sizeof(ShardHeader) == 56);

struct TileEntry {
    std::uint32_t key = 0;   // PatchKey::packed()
    std::uint32_t bytes = 0; // equals the shard's tile_bytes until tiles are compressed
    std::uint64_t offset = 0;
    float height_min = 0, height_max = 0; // what the tile's heights span, for the CLOD's bounds
};
static_assert(std::is_trivially_copyable_v<TileEntry>);
static_assert(sizeof(TileEntry) == 24);

// The shard's file name within the body directory, "f3_l07.tiles": 13 characters and the null.
inline constexpr unsigned shard_name_size = 14;
constexpr void shard_name(unsigned face, unsigned level, char (&out)[shard_name_size]) {
    const char text[] = {
        'f', char('0' + face), '_', 'l', char('0' + level / 10), char('0' + level % 10), '.', 't', 'i', 'l', 'e', 's',
        '\0'};
    for (unsigned i = 0; i < sizeof text; i++)
        out[i] = text[i];
}
inline constexpr const char store_name[] = "store.planet";

} // namespace planet::store
