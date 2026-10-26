#include "planet/baked_provider.hpp"
#include "planet/cook.hpp"
#include "planet/mixed_provider.hpp"
#include "planet/procedural_provider.hpp"
#include "planet/store_format.hpp"
#include "planet/tile_store.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

using namespace planet;

namespace {

namespace fs = std::filesystem;

int failures = 0;

bool check(bool ok, const char* test, const char* what) {
    if (!ok) {
        std::printf("FAIL %s: %s\n", test, what);
        failures++;
    }
    return ok;
}

constexpr std::uint64_t seed_a = 0x1007, seed_b = 0x2008;
constexpr unsigned level_count = 3; // levels 0 .. 2
constexpr unsigned tile_total = 6 * (1 + 4 + 16);
constexpr unsigned shard_total = 6 * level_count;

// Unique per process, so concurrent runs do not share a directory.
const fs::path& root() {
    static const fs::path path = [] {
        std::error_code ec;
        return fs::temp_directory_path(ec) /
               std::format("planet_store_tests_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    }();
    return path;
}

std::string dir(const char* name) {
    return (root() / name).string();
}

void remove_dir(const std::string& path) {
    std::error_code ec;
    fs::remove_all(path, ec);
}

CookOptions options_for(const std::string& path, std::uint64_t seed) {
    CookOptions options;
    options.directory = path.c_str();
    options.seed = seed;
    options.level_min = 0;
    options.level_max = 2;
    options.threads = 0;
    options.quiet = true;
    return options;
}

std::vector<char> read_file(const std::string& path) {
    std::vector<char> bytes;
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return bytes;
    char buffer[4096];
    for (std::size_t got; (got = std::fread(buffer, 1, sizeof buffer, file)) > 0;)
        bytes.insert(bytes.end(), buffer, buffer + got);
    std::fclose(file);
    return bytes;
}

std::string shard_path(const std::string& path, unsigned face, unsigned level) {
    char name[store::shard_name_size];
    store::shard_name(face, level, name);
    return (fs::path(path) / name).string();
}

TileOutput output_for(const TileLayout& layout, std::vector<std::byte>& bytes) {
    bytes.assign(layout.bytes(), std::byte{});
    TileOutput out;
    for (unsigned p = 0; p < layout.plane_count; p++)
        out.planes[p] = {bytes.data() + layout.offset(p), layout.planes[p].bytes()};
    return out;
}

bool layouts_equal(const TileLayout& a, const TileLayout& b) {
    if (a.plane_count != b.plane_count)
        return false;
    for (unsigned p = 0; p < a.plane_count; p++)
        if (a.planes[p].id != b.planes[p].id || a.planes[p].format != b.planes[p].format ||
            a.planes[p].side != b.planes[p].side)
            return false;
    return true;
}

bool planes_equal(const TileLayout& layout, const std::vector<std::byte>& a, const std::vector<std::byte>& b) {
    for (unsigned p = 0; p < layout.plane_count; p++)
        if (std::memcmp(a.data() + layout.offset(p), b.data() + layout.offset(p), layout.planes[p].bytes()) != 0)
            return false;
    return true;
}

PatchKey key_of(unsigned face, unsigned level, unsigned x, unsigned y) {
    return {.face = std::uint8_t(face), .level = std::uint8_t(level), .x = std::uint16_t(x), .y = std::uint16_t(y)};
}

// The shared store most tests read: seed_a, levels 0..2, every face.
struct Fixture {
    MinorPlanetTerrain terrain{seed_a};
    ProceduralProvider procedural{terrain};
    std::string path = dir("main");
};

void cook_writes_store(Fixture& fx) {
    const char* test = "cook_writes_store";
    CookReport report;
    const CookOptions options = options_for(fx.path, fx.terrain.seed());
    check(cook(options, fx.procedural, report) == CookStatus::ok, test, "cook ok");
    check(report.shards_cooked == shard_total, test, "18 shards cooked");
    check(report.tiles_written == tile_total, test, "126 tiles written");
    check(report.shards_skipped == 0, test, "none skipped");

    std::error_code ec;
    check(fs::exists(fs::path(fx.path) / store::store_name, ec), test, "store.planet exists");
    bool all = true;
    for (unsigned face = 0; face < 6; face++)
        for (unsigned level = 0; level < level_count; level++)
            all = all && fs::exists(shard_path(fx.path, face, level), ec);
    check(all, test, "every shard exists");

    const std::uint64_t size = fs::file_size(shard_path(fx.path, 3, 2), ec);
    const std::uint64_t want = sizeof(store::ShardHeader) +
                               16 * std::uint64_t(ProceduralProvider::tile_layout.bytes()) +
                               16 * sizeof(store::TileEntry);
    check(!ec && size == want, test, "level 2 shard size");
}

void header_contents(Fixture& fx) {
    const char* test = "header_contents";
    TileStore store;
    if (!check(store.open(fx.path.c_str()) == StoreStatus::ok, test, "open ok"))
        return;
    const store::StoreHeader& header = store.header();
    check(header.seed == fx.terrain.seed(), test, "seed");
    check(header.cooker == cook_version, test, "cooker");
    check(header.level_min == 0 && header.level_max == 2, test, "level range");
    check(header.face_mask == 0x3f, test, "face mask");
    check(header.shape.heights.min == MinorPlanetTerrain::shape.heights.min &&
              header.shape.heights.max == MinorPlanetTerrain::shape.heights.max,
          test, "shape heights");
    bool levels = true;
    for (unsigned i = 0; i < LevelTable::count; i++)
        levels = levels && header.levels.error[i] == MinorPlanetTerrain::levels.error[i] &&
                 header.levels.relief[i] == MinorPlanetTerrain::levels.relief[i];
    check(levels, test, "level table");
    for (unsigned level = 0; level < level_count; level++)
        check(layouts_equal(header.layouts[level], ProceduralProvider::tile_layout), test, "cooked layout");
    check(header.layouts[3].plane_count == 0, test, "level 3 layout empty");
    check(store.contains(2), test, "contains 2");
    check(!store.contains(3), test, "not contains 3");
}

void baked_equals_procedural(Fixture& fx) {
    const char* test = "baked_equals_procedural";
    TileStore store;
    if (!check(store.open(fx.path.c_str()) == StoreStatus::ok, test, "open ok"))
        return;
    BakedProvider baked(store);
    const TileLayout& layout = ProceduralProvider::tile_layout;
    std::vector<std::byte> a, b;
    unsigned bad_produce = 0, bad_bytes = 0, bad_heights = 0, bad_entry = 0;
    for (unsigned level = 0; level < level_count; level++)
        for (unsigned face = 0; face < 6; face++)
            for (unsigned y = 0; y < (1u << level); y++)
                for (unsigned x = 0; x < (1u << level); x++) {
                    const PatchKey key = key_of(face, level, x, y);
                    TileOutput from_store = output_for(layout, a), from_terrain = output_for(layout, b);
                    const bool got = baked.produce({.key = key}, from_store);
                    const bool made = fx.procedural.produce({.key = key}, from_terrain);
                    bad_produce += !(got && made);
                    bad_bytes += !planes_equal(layout, a, b);
                    bad_heights += from_store.heights.min != from_terrain.heights.min ||
                                   from_store.heights.max != from_terrain.heights.max;
                    store::TileEntry entry;
                    bad_entry += !store.entry(key, entry) || entry.height_min != from_terrain.heights.min ||
                                 entry.height_max != from_terrain.heights.max || entry.bytes != layout.bytes();
                }
    check(bad_produce == 0, test, "every produce returns true");
    check(bad_bytes == 0, test, "plane bytes equal");
    check(bad_heights == 0, test, "height ranges equal");
    check(bad_entry == 0, test, "index entries match");
}

void baked_missing_level(Fixture& fx) {
    const char* test = "baked_missing_level";
    TileStore store;
    if (!check(store.open(fx.path.c_str()) == StoreStatus::ok, test, "open ok"))
        return;
    BakedProvider baked(store);
    TileOutput out; // the level has no planes
    check(!baked.produce({.key = key_of(0, 3, 1, 1)}, out), test, "level 3 produce fails");
    check(baked.layout(3).plane_count == 0, test, "level 3 layout empty");
    check(!store.read(key_of(0, 3, 0, 0), out), test, "read level 3 fails");
    check(!store.read(key_of(7, 1, 0, 0), out), test, "read face 7 fails");
    check(!store.read(key_of(0, 200, 0, 0), out), test, "read level 200 fails");
    check(!store.read(key_of(0, 1, 5, 5), out), test, "read cell outside the level fails");
}

void mixed_routes_by_level(Fixture& fx) {
    const char* test = "mixed_routes_by_level";
    TileStore store;
    if (!check(store.open(fx.path.c_str()) == StoreStatus::ok, test, "open ok"))
        return;
    BakedProvider baked(store);
    MixedProvider mixed(baked, fx.procedural, 1);
    check(mixed.split_level() == 1, test, "split level");
    check(layouts_equal(mixed.layout(0), ProceduralProvider::tile_layout), test, "layout 0");
    check(layouts_equal(mixed.layout(2), ProceduralProvider::tile_layout), test, "layout 2");

    const TileLayout& layout = ProceduralProvider::tile_layout;
    std::vector<std::byte> a, b;
    for (const PatchKey key : {key_of(2, 1, 1, 0), key_of(4, 2, 3, 2)}) {
        TileOutput from_mixed = output_for(layout, a), from_terrain = output_for(layout, b);
        check(mixed.produce({.key = key}, from_mixed), test, "mixed produce");
        check(fx.procedural.produce({.key = key}, from_terrain), test, "procedural produce");
        check(planes_equal(layout, a, b), test, "mixed bytes equal procedural");
    }
    // The store lacks level 3, so only the procedural source can answer.
    TileOutput deep = output_for(layout, a);
    check(mixed.produce({.key = key_of(1, 3, 5, 6)}, deep), test, "level 3 through procedural");
}

void recook_skips(Fixture& fx) {
    const char* test = "recook_skips";
    std::vector<std::vector<char>> before;
    for (unsigned face = 0; face < 6; face++)
        before.push_back(read_file(shard_path(fx.path, face, 2)));
    const std::vector<char> store_before = read_file((fs::path(fx.path) / store::store_name).string());

    CookOptions options = options_for(fx.path, fx.terrain.seed());
    CookReport report;
    check(cook(options, fx.procedural, report) == CookStatus::ok, test, "recook ok");
    check(report.shards_skipped == shard_total && report.shards_cooked == 0, test, "all skipped");

    options.force = true;
    check(cook(options, fx.procedural, report) == CookStatus::ok, test, "forced cook ok");
    check(report.shards_cooked == shard_total && report.shards_skipped == 0, test, "all cooked");
    bool same = true;
    for (unsigned face = 0; face < 6; face++) {
        const std::vector<char> after = read_file(shard_path(fx.path, face, 2));
        same = same && !after.empty() && after == before[face];
    }
    check(same, test, "shard bytes identical after a forced cook");
    check(read_file((fs::path(fx.path) / store::store_name).string()) == store_before, test, "store header identical");
}

void seed_conflict(Fixture& fx) {
    const char* test = "seed_conflict";
    const MinorPlanetTerrain other(seed_b);
    ProceduralProvider provider(other);
    CookReport report;
    const CookOptions options = options_for(fx.path, other.seed());
    check(cook(options, provider, report) == CookStatus::conflict, test, "other seed conflicts");
    TileStore store;
    if (!check(store.open(fx.path.c_str()) == StoreStatus::ok, test, "store still opens"))
        return;
    check(store.header().seed == fx.terrain.seed(), test, "seed unchanged");
}

void check_passes(Fixture& fx) {
    const char* test = "check_passes";
    CookOptions options = options_for(fx.path, fx.terrain.seed());
    options.check = true;
    CookReport report;
    check(cook(options, fx.procedural, report) == CookStatus::ok, test, "cook with check ok");
    check(report.tiles_checked == tile_total, test, "126 tiles checked");
    check(report.tiles_mismatched == 0, test, "no mismatches");
}

void corrupt_shard(Fixture& fx) {
    const char* test = "corrupt_shard";
    std::error_code ec;
    const std::string bad = dir("corrupt"), gone = dir("incomplete"), empty = dir("empty");
    remove_dir(bad);
    remove_dir(gone);
    remove_dir(empty);
    fs::copy(fx.path, bad, fs::copy_options::recursive, ec);
    check(!ec, test, "copy for corrupt");
    fs::copy(fx.path, gone, fs::copy_options::recursive, ec);
    check(!ec, test, "copy for incomplete");
    fs::create_directories(empty, ec);

    std::FILE* file = std::fopen(shard_path(bad, 2, 1).c_str(), "r+b");
    if (check(file != nullptr, test, "open shard for writing")) {
        const unsigned char zeros[4] = {};
        check(std::fwrite(zeros, 1, 4, file) == 4, test, "zero the magic");
        std::fclose(file);
    }
    TileStore store;
    check(store.open(bad.c_str()) == StoreStatus::corrupt, test, "zeroed magic is corrupt");
    check(!store.is_open(), test, "closed after corrupt");

    fs::remove(shard_path(gone, 4, 2), ec);
    check(store.open(gone.c_str()) == StoreStatus::incomplete, test, "missing shard is incomplete");
    check(!store.is_open(), test, "closed after incomplete");

    check(store.open(empty.c_str()) == StoreStatus::missing, test, "no store.planet is missing");
    check(!store.is_open(), test, "closed after missing");
}

void invalid_options(Fixture& fx) {
    const char* test = "invalid_options";
    const std::string path = dir("invalid");
    CookReport report;
    CookOptions options = options_for(path, fx.terrain.seed());
    options.directory = nullptr;
    check(cook(options, fx.procedural, report) == CookStatus::invalid_options, test, "null directory");
    options = options_for(path, fx.terrain.seed());
    options.level_max = LevelTable::count;
    check(cook(options, fx.procedural, report) == CookStatus::invalid_options, test, "level past the table");
    options = options_for(path, fx.terrain.seed());
    options.face_mask = 0;
    check(cook(options, fx.procedural, report) == CookStatus::invalid_options, test, "empty face mask");
    options = options_for(path, fx.terrain.seed());
    options.level_min = 2;
    options.level_max = 1;
    check(cook(options, fx.procedural, report) == CookStatus::invalid_options, test, "inverted level range");
    options = options_for(path, fx.terrain.seed());
    options.face_mask = 0x40;
    check(cook(options, fx.procedural, report) == CookStatus::invalid_options, test, "face mask past six faces");
}

void determinism_across_threads(Fixture& fx) {
    const char* test = "determinism_across_threads";
    const std::string other = dir("one_thread");
    CookOptions options = options_for(other, fx.terrain.seed());
    options.threads = 1;
    CookReport report;
    check(cook(options, fx.procedural, report) == CookStatus::ok, test, "single-thread cook ok");
    const std::vector<char> head = read_file((fs::path(fx.path) / store::store_name).string());
    check(!head.empty() && head == read_file((fs::path(other) / store::store_name).string()), test,
          "store header identical");
    unsigned differ = 0;
    for (unsigned face = 0; face < 6; face++)
        for (unsigned level = 0; level < level_count; level++) {
            const std::vector<char> a = read_file(shard_path(fx.path, face, level));
            differ += a.empty() || a != read_file(shard_path(other, face, level));
        }
    check(differ == 0, test, "every shard identical");
}

void check_detects_corruption(Fixture& fx) {
    const char* test = "check_detects_corruption";
    const std::string bad = dir("flipped");
    std::error_code ec;
    fs::copy(fx.path, bad, fs::copy_options::recursive, ec);
    check(!ec, test, "copy");
    std::FILE* file = std::fopen(shard_path(bad, 1, 2).c_str(), "r+b");
    if (!check(file != nullptr, test, "open shard"))
        return;
    const long at = long(sizeof(store::ShardHeader)) + 100;
    unsigned char byte = 0;
    const bool read = std::fseek(file, at, SEEK_SET) == 0 && std::fread(&byte, 1, 1, file) == 1;
    byte ^= 0xff;
    const bool wrote = read && std::fseek(file, at, SEEK_SET) == 0 && std::fwrite(&byte, 1, 1, file) == 1;
    std::fclose(file);
    check(wrote, test, "flip a payload byte");

    CookOptions options = options_for(bad, fx.terrain.seed());
    options.check = true;
    CookReport report;
    check(cook(options, fx.procedural, report) == CookStatus::mismatch, test, "mismatch reported");
    check(report.tiles_mismatched >= 1, test, "mismatched tile counted");
    check(report.shards_skipped == shard_total, test, "nothing recooked");
}

void truncated_shard(Fixture& fx) {
    const char* test = "truncated_shard";
    std::error_code ec;
    const std::string cut = dir("truncated"), count = dir("tile_count");
    fs::copy(fx.path, cut, fs::copy_options::recursive, ec);
    fs::copy(fx.path, count, fs::copy_options::recursive, ec);
    const std::string shard = shard_path(cut, 0, 1);
    fs::resize_file(shard, fs::file_size(shard, ec) / 2, ec);
    check(!ec, test, "truncate");
    TileStore store;
    check(store.open(cut.c_str()) == StoreStatus::corrupt, test, "truncated shard is corrupt");

    std::FILE* file = std::fopen(shard_path(count, 0, 2).c_str(), "r+b");
    if (check(file != nullptr, test, "open shard")) {
        const std::uint32_t three = 3;
        check(std::fseek(file, 5 * 4, SEEK_SET) == 0 && std::fwrite(&three, 4, 1, file) == 1, test, "set tile_count");
        std::fclose(file);
    }
    check(store.open(count.c_str()) == StoreStatus::corrupt, test, "wrong tile_count is corrupt");
}

} // namespace

int main() {
    remove_dir(root().string());
    {
        Fixture fx;
        cook_writes_store(fx);
        header_contents(fx);
        baked_equals_procedural(fx);
        baked_missing_level(fx);
        mixed_routes_by_level(fx);
        recook_skips(fx);
        seed_conflict(fx);
        check_passes(fx);
        corrupt_shard(fx);
        invalid_options(fx);
        determinism_across_threads(fx);
        check_detects_corruption(fx);
        truncated_shard(fx);
    }
    remove_dir(root().string());
    std::printf("store tests: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
