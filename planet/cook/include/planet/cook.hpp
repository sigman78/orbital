#pragma once
#include "planet/allocator.hpp"
#include "planet/tile.hpp"

#include <cstdint>

namespace planet {

// Bumped whenever the same seed produces different tile bytes, so stores cooked before are redone.
inline constexpr std::uint32_t cook_version = 1;

struct CookOptions {
    const char* directory = nullptr; // the body directory, created if missing
    std::uint64_t seed = 0;          // recorded in the store; the reader matches it against its body
    unsigned level_min = 0, level_max = 5;
    unsigned face_mask = 0x3f;
    unsigned threads = 0; // 0: every core
    bool force = false;   // rewrite shards whose header already matches
    bool check = false;   // after cooking, read every tile back and compare with the provider
    bool quiet = false;   // no per-level progress lines
    const Allocator* allocator = nullptr;
};

struct CookReport {
    unsigned shards_cooked = 0, shards_skipped = 0;
    unsigned tiles_written = 0, tiles_checked = 0, tiles_mismatched = 0;
    std::uint64_t bytes_written = 0;
    double seconds = 0;
};

enum class CookStatus : unsigned char {
    ok,
    invalid_options, // no directory, level range outside the table, empty face mask
    conflict,        // the directory holds a store for another seed or cooker version (use force)
    io_error,
    mismatch, // check found tiles that differ from the provider
};
const char* cook_status_name(CookStatus status);

// Writes the provider's tiles for the levels and faces asked into the store directory: one shard
// per face and level, tiles in key order, bytes independent of the thread count. Shards already
// there with a matching header are skipped unless forced.
CookStatus cook(const CookOptions& options, TileProvider& provider, CookReport& report);

// What the command line cannot know by itself: how a showcase seed becomes the terrain's seed.
struct CookHost {
    // False when the showcase has no such body; null: --seed is the terrain's own.
    bool (*terrain_seed)(void* user, std::uint64_t showcase_seed, std::uint64_t& out) = nullptr;
    void* user = nullptr;
};

// The planet-forge command line: parses argv, builds the crater terrain's provider for the seed,
// cooks, checks or explains, and returns the process exit code (0 ok, 1 usage, 2 I/O, 3 mismatch
// or conflict).
int cook_main(int argc, char** argv, const CookHost& host = {});

} // namespace planet
