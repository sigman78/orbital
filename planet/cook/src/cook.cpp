#include "planet/cook.hpp"

#include "core/panic.hpp"
#include "core/panic_if.hpp"
#include "core/parallel.hpp"
#include "planet/store_format.hpp"
#include "planet/tile_store.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace planet {

using namespace space; // the host's worker pool

const char* cook_status_name(CookStatus status) {
    switch (status) {
    case CookStatus::ok: return "ok";
    case CookStatus::invalid_options: return "invalid options";
    case CookStatus::conflict: return "the directory holds a store for another seed or cooker version";
    case CookStatus::io_error: return "I/O error";
    case CookStatus::mismatch: return "stored tiles differ from the provider";
    }
    return "unknown";
}

namespace {

namespace fs = std::filesystem;

constexpr unsigned chunk_tiles = 256;
constexpr std::size_t buffer_alignment = 16;

// Paths are ASCII (the store's names are), so string() is lossless.
std::string path_of(const char* directory, const char* name) {
    return (fs::path(directory) / name).string();
}

void* allocate(const Allocator* allocator, std::size_t bytes) {
    void* block = allocator->alloc(allocator->user, bytes, buffer_alignment, Tag::cook);
    panic_if(!block, "cook allocation failed");
    return block;
}

void release(const Allocator* allocator, void* block, std::size_t bytes) {
    allocator->free(allocator->user, block, bytes, Tag::cook);
}

template <class T> bool read_file_header(const std::string& path, T& out) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return false;
    const bool ok = std::fread(&out, sizeof out, 1, file) == 1;
    std::fclose(file);
    return ok;
}

bool write_all(std::FILE* file, const void* data, std::size_t bytes) {
    return bytes == 0 || std::fwrite(data, 1, bytes, file) == bytes;
}

// std::rename does not replace an existing file on Windows, so go through the filesystem.
bool replace_file(const std::string& tmp, const std::string& final_path) {
    std::error_code ec;
    fs::rename(tmp, final_path, ec);
    return !ec;
}

// A shard is current when its header is the one this cook would write and the file is whole.
bool shard_is_current(const std::string& path, const store::ShardHeader& want) {
    store::ShardHeader have;
    if (!read_file_header(path, have))
        return false;
    std::error_code ec;
    const std::uint64_t size = fs::file_size(path, ec);
    return !ec && have.magic == want.magic && have.version == want.version && have.cooker == want.cooker &&
           have.face == want.face && have.level == want.level && have.seed == want.seed &&
           have.tile_count == want.tile_count && have.tile_bytes == want.tile_bytes &&
           have.payload_offset == want.payload_offset && have.index_offset == want.index_offset &&
           size == want.index_offset + std::uint64_t(want.tile_count) * sizeof(store::TileEntry);
}

TileOutput output_for(const TileLayout& layout, std::byte* slot) {
    TileOutput out;
    for (unsigned p = 0; p < layout.plane_count; p++)
        out.planes[p] = {slot + layout.offset(p), layout.planes[p].bytes()};
    return out;
}

PatchKey key_of(unsigned face, unsigned level, unsigned index) {
    const unsigned side = 1u << level;
    return {.face = std::uint8_t(face),
            .level = std::uint8_t(level),
            .x = std::uint16_t(index % side),
            .y = std::uint16_t(index / side)};
}

// Cooks one shard into <name>.tmp and renames it over the final file once it is complete.
CookStatus cook_shard(TileProvider& provider, const Allocator* allocator, const store::ShardHeader& header,
                      const std::string& path, unsigned workers, CookReport& report) {
    const TileLayout& layout = provider.layout(header.level);
    const std::size_t tile_bytes = header.tile_bytes;
    const std::size_t entries_bytes = sizeof(store::TileEntry) * header.tile_count;
    const std::size_t chunk_bytes = tile_bytes * chunk_tiles;
    // The reader seeks with std::fseek, whose reach is a long: 2 GiB on Windows (phase 3 debt).
    if (header.index_offset + entries_bytes > std::uint64_t(std::numeric_limits<long>::max()))
        return CookStatus::io_error;

    auto* chunk = static_cast<std::byte*>(allocate(allocator, chunk_bytes));
    auto* entries = static_cast<store::TileEntry*>(allocate(allocator, entries_bytes));
    std::memset(entries, 0, entries_bytes);
    std::vector<char> failed(chunk_tiles);

    const std::string tmp = path + ".tmp";
    std::FILE* file = std::fopen(tmp.c_str(), "wb");
    store::ShardHeader zero;
    std::memset(&zero, 0, sizeof zero);
    bool ok = file && write_all(file, &zero, sizeof zero); // the real header follows once the tiles are in

    for (unsigned first = 0; ok && first < header.tile_count; first += chunk_tiles) {
        const unsigned count = std::min(chunk_tiles, header.tile_count - first);
        std::fill(failed.begin(), failed.begin() + count, char(0));
        parallel_for(
            count,
            [&](unsigned i) {
                const unsigned index = first + i;
                const PatchKey key = key_of(header.face, header.level, index);
                std::byte* slot = chunk + std::size_t(i) * tile_bytes;
                std::memset(slot, 0, tile_bytes);
                TileOutput out = output_for(layout, slot);
                if (!provider.produce({.key = key, .detail = 1}, out)) {
                    failed[i] = 1;
                    return;
                }
                entries[index] = {.key = key.packed(),
                                  .bytes = header.tile_bytes,
                                  .offset = header.payload_offset + std::uint64_t(index) * tile_bytes,
                                  .height_min = out.heights.min,
                                  .height_max = out.heights.max};
            },
            workers);
        ok = std::find(failed.begin(), failed.begin() + count, char(1)) == failed.begin() + count &&
             write_all(file, chunk, tile_bytes * count);
    }

    ok = ok && write_all(file, entries, entries_bytes);
    ok = ok && std::fseek(file, 0, SEEK_SET) == 0 && write_all(file, &header, sizeof header);
    if (file)
        ok = (std::fclose(file) == 0) && ok;
    ok = ok && replace_file(tmp, path);
    if (!ok) {
        std::error_code ec;
        fs::remove(tmp, ec);
    }
    release(allocator, entries, entries_bytes);
    release(allocator, chunk, chunk_bytes);
    if (!ok)
        return CookStatus::io_error;

    report.shards_cooked++;
    report.tiles_written += header.tile_count;
    report.bytes_written += header.index_offset + entries_bytes;
    return CookStatus::ok;
}

bool planes_equal(const TileLayout& layout, const std::byte* a, const std::byte* b) {
    for (unsigned p = 0; p < layout.plane_count; p++)
        if (std::memcmp(a + layout.offset(p), b + layout.offset(p), layout.planes[p].bytes()) != 0)
            return false;
    return true;
}

// Reads the tiles of the range back from the store on disk and compares them with the provider.
CookStatus check_store(const CookOptions& options, TileProvider& provider, const Allocator* allocator, unsigned workers,
                       CookReport& report) {
    TileStore store(allocator);
    if (store.open(options.directory) != StoreStatus::ok)
        return CookStatus::io_error;
    std::atomic<unsigned> bad = 0;
    for (unsigned level = options.level_min; level <= options.level_max; level++) {
        const TileLayout& layout = provider.layout(level);
        const std::size_t bytes = layout.bytes();
        const unsigned count = 1u << (2 * level);
        for (unsigned face = 0; face < store::face_count; face++) {
            if (!(options.face_mask >> face & 1))
                continue;
            parallel_for(
                count,
                [&](unsigned index) {
                    const PatchKey key = key_of(face, level, index);
                    auto* made = static_cast<std::byte*>(allocate(allocator, bytes));
                    auto* read = static_cast<std::byte*>(allocate(allocator, bytes));
                    std::memset(made, 0, bytes);
                    std::memset(read, 0, bytes);
                    TileOutput want = output_for(layout, made), have = output_for(layout, read);
                    if (!provider.produce({.key = key, .detail = 1}, want) || !store.read(key, have) ||
                        !planes_equal(layout, made, read) || want.heights.min != have.heights.min ||
                        want.heights.max != have.heights.max)
                        bad++;
                    release(allocator, read, bytes);
                    release(allocator, made, bytes);
                },
                workers);
            report.tiles_checked += count;
        }
    }
    report.tiles_mismatched = bad;
    return bad ? CookStatus::mismatch : CookStatus::ok;
}

} // namespace

CookStatus cook(const CookOptions& options, TileProvider& provider, CookReport& report) {
    const auto start = std::chrono::steady_clock::now();
    report = {};
    if (!options.directory || options.level_min > options.level_max || options.level_max >= store::max_levels ||
        options.face_mask == 0 || options.face_mask > 0x3f)
        return CookStatus::invalid_options;
    const Allocator* allocator = allocator_or_default(options.allocator);
    const unsigned workers = options.threads ? options.threads : parallel_workers_max;

    std::error_code ec;
    fs::create_directories(options.directory, ec);
    if (ec)
        return CookStatus::io_error;

    store::StoreHeader header;
    std::memset(&header, 0, sizeof header);
    header.magic = store::store_magic;
    header.version = store::format_version;
    header.cooker = cook_version;
    header.level_min = options.level_min;
    header.level_max = options.level_max;
    header.face_mask = options.face_mask;
    header.seed = options.seed;
    header.shape = provider.shape();
    header.levels = provider.levels();
    for (unsigned level = options.level_min; level <= options.level_max; level++) {
        const TileLayout& layout = provider.layout(level); // field by field: its padding is not the store's
        header.layouts[level].plane_count = layout.plane_count;
        for (unsigned p = 0; p < layout.plane_count; p++) {
            header.layouts[level].planes[p].id = layout.planes[p].id;
            header.layouts[level].planes[p].format = layout.planes[p].format;
            header.layouts[level].planes[p].side = layout.planes[p].side;
        }
    }

    const std::string store_path = path_of(options.directory, store::store_name);
    if (!options.force && fs::exists(store_path, ec)) {
        store::StoreHeader have;
        if (!read_file_header(store_path, have) || have.magic != store::store_magic ||
            have.version != store::format_version || have.seed != options.seed || have.cooker != cook_version)
            return CookStatus::conflict;
    }

    // The new header covers exactly the asked levels and faces; shards of an earlier, wider store
    // outside them stay on disk untouched and unreferenced.
    for (unsigned level = options.level_min; level <= options.level_max; level++) {
        const auto level_start = std::chrono::steady_clock::now();
        const TileLayout& layout = provider.layout(level);
        unsigned cooked = 0, skipped = 0;
        std::uint64_t bytes = 0;
        for (unsigned face = 0; face < store::face_count; face++) {
            if (!(options.face_mask >> face & 1))
                continue;
            store::ShardHeader shard;
            std::memset(&shard, 0, sizeof shard);
            shard.magic = store::shard_magic;
            shard.version = store::format_version;
            shard.cooker = cook_version;
            shard.face = face;
            shard.level = level;
            shard.tile_count = 1u << (2 * level);
            shard.tile_bytes = layout.bytes();
            shard.seed = options.seed;
            shard.payload_offset = sizeof(store::ShardHeader);
            shard.index_offset = shard.payload_offset + std::uint64_t(shard.tile_count) * shard.tile_bytes;

            char name[store::shard_name_size];
            store::shard_name(face, level, name);
            const std::string path = path_of(options.directory, name);
            if (!options.force && shard_is_current(path, shard)) {
                report.shards_skipped++;
                skipped++;
                continue;
            }
            const std::uint64_t before = report.bytes_written;
            const CookStatus status = cook_shard(provider, allocator, shard, path, workers, report);
            if (status != CookStatus::ok)
                return status;
            cooked++;
            bytes += report.bytes_written - before;
        }
        if (!options.quiet) {
            const double seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - level_start).count();
            if (cooked)
                std::printf("level %u: %u shards, %u tiles, %.1f s, %.1f MiB%s", level, cooked,
                            cooked * (1u << (2 * level)), seconds, double(bytes) / (1024 * 1024),
                            skipped ? ", " : "\n");
            else
                std::printf("level %u: ", level);
            if (skipped)
                std::printf("%u shards skipped\n", skipped);
        }
    }

    // Last, so a store header never promises shards that are not there.
    const std::string tmp = store_path + ".tmp";
    std::FILE* file = std::fopen(tmp.c_str(), "wb");
    bool ok = file && write_all(file, &header, sizeof header);
    if (file)
        ok = (std::fclose(file) == 0) && ok;
    ok = ok && replace_file(tmp, store_path);
    if (!ok) {
        fs::remove(tmp, ec);
        return CookStatus::io_error;
    }

    CookStatus status = CookStatus::ok;
    if (options.check)
        status = check_store(options, provider, allocator, workers, report);
    report.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return status;
}

} // namespace planet
