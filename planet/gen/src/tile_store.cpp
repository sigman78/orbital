#include "planet/tile_store.hpp"

#include "core/panic.hpp"
#include "core/panic_if.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <mutex>
#include <new>

namespace planet {

using namespace space;

namespace {

constexpr unsigned max_plane_side = 4096; // past any array a host builds; bounds a corrupt layout

struct Shard {
    std::FILE* file = nullptr;
    std::mutex lock;
    store::TileEntry* entries = nullptr;
    unsigned count = 0;
};

bool seek_to(std::FILE* file, std::uint64_t offset) {
    if (offset > std::uint64_t(std::numeric_limits<long>::max()))
        return false;
    return std::fseek(file, long(offset), SEEK_SET) == 0;
}

} // namespace

struct TileStore::Impl {
    const Allocator* allocator = nullptr; // what this Impl was allocated from
    store::StoreHeader header;
    bool open = false;
    Shard shards[store::face_count][store::max_levels];

    void release() {
        for (auto& face : shards)
            for (Shard& shard : face) {
                if (shard.file)
                    std::fclose(shard.file);
                if (shard.entries)
                    allocator->free(allocator->user, shard.entries, shard.count * sizeof(store::TileEntry), Tag::store);
                shard.file = nullptr;
                shard.entries = nullptr;
                shard.count = 0;
            }
        open = false;
    }

    Shard* find(PatchKey key) {
        if (!open || key.face >= store::face_count || key.level >= store::max_levels)
            return nullptr;
        Shard& shard = shards[key.face][key.level];
        return shard.file ? &shard : nullptr;
    }

    static const store::TileEntry* lookup(const Shard& shard, PatchKey key) {
        const std::uint32_t packed = key.packed();
        const store::TileEntry* begin = shard.entries;
        const store::TileEntry* end = begin + shard.count;
        const store::TileEntry* it = std::lower_bound(
            begin, end, packed, [](const store::TileEntry& entry, std::uint32_t value) { return entry.key < value; });
        return it != end && it->key == packed ? it : nullptr;
    }

    StoreStatus open_shard(const std::filesystem::path& directory, unsigned face, unsigned level) {
        char name[store::shard_name_size];
        store::shard_name(face, level, name);
        const std::filesystem::path path = directory / name;
        Shard& shard = shards[face][level];
        shard.file = std::fopen(path.string().c_str(), "rb"); // ASCII paths
        if (!shard.file)
            return StoreStatus::incomplete;
        store::ShardHeader head;
        if (std::fread(&head, sizeof head, 1, shard.file) != 1)
            return StoreStatus::corrupt;
        if (head.magic != store::shard_magic)
            return StoreStatus::corrupt;
        if (head.version != store::format_version)
            return StoreStatus::version;
        if (head.face != face || head.level != level || head.seed != header.seed || head.cooker != header.cooker ||
            head.tile_bytes != header.layouts[level].bytes() || head.tile_count != 1u << (2 * level))
            return StoreStatus::corrupt;
        std::error_code error;
        const std::uint64_t size = std::filesystem::file_size(path, error);
        if (error)
            return StoreStatus::io_error;
        const std::uint64_t index_bytes = std::uint64_t(head.tile_count) * sizeof(store::TileEntry);
        if (head.index_offset > size || index_bytes > size - head.index_offset ||
            head.payload_offset > head.index_offset)
            return StoreStatus::corrupt;
        if (head.tile_count) {
            void* block = allocator->alloc(allocator->user, index_bytes, alignof(store::TileEntry), Tag::store);
            panic_if(!block, "tile store allocation failed");
            shard.entries = static_cast<store::TileEntry*>(block);
            shard.count = head.tile_count;
            if (!seek_to(shard.file, head.index_offset))
                return StoreStatus::corrupt;
            if (std::fread(shard.entries, sizeof(store::TileEntry), head.tile_count, shard.file) != head.tile_count)
                return StoreStatus::io_error;
        }
        for (unsigned i = 0; i < shard.count; i++) {
            const store::TileEntry& entry = shard.entries[i];
            if (i && shard.entries[i - 1].key >= entry.key)
                return StoreStatus::corrupt;
            if (entry.bytes != head.tile_bytes || entry.offset < head.payload_offset ||
                entry.offset > head.index_offset || entry.bytes > head.index_offset - entry.offset)
                return StoreStatus::corrupt;
        }
        return StoreStatus::ok;
    }

    StoreStatus open_all(const char* directory_name) {
        const std::filesystem::path directory(directory_name);
        std::FILE* file = std::fopen((directory / store::store_name).string().c_str(), "rb"); // ASCII paths
        if (!file)
            return StoreStatus::missing;
        const bool read = std::fread(&header, sizeof header, 1, file) == 1;
        std::fclose(file);
        if (!read)
            return StoreStatus::corrupt;
        if (header.magic != store::store_magic)
            return StoreStatus::corrupt;
        if (header.version != store::format_version)
            return StoreStatus::version;
        if (header.level_min > header.level_max || header.level_max >= store::max_levels || header.face_mask == 0 ||
            header.face_mask > 0x3f)
            return StoreStatus::corrupt;
        for (unsigned level = header.level_min; level <= header.level_max; level++) {
            const TileLayout& layout = header.layouts[level];
            if (layout.plane_count == 0 || layout.plane_count > TileLayout::max_planes)
                return StoreStatus::corrupt;
            for (unsigned p = 0; p < layout.plane_count; p++) {
                const PlaneLayout& plane = layout.planes[p];
                if (plane.id > PlaneId::slope || plane.format > TileFormat::rgba8_unorm || plane.side == 0 ||
                    plane.side > max_plane_side)
                    return StoreStatus::corrupt;
            }
        }
        for (unsigned face = 0; face < store::face_count; face++) {
            if (!(header.face_mask >> face & 1))
                continue;
            for (unsigned level = header.level_min; level <= header.level_max; level++) {
                const StoreStatus status = open_shard(directory, face, level);
                if (status != StoreStatus::ok)
                    return status;
            }
        }
        return StoreStatus::ok;
    }
};

const char* store_status_name(StoreStatus status) {
    switch (status) {
    case StoreStatus::ok: return "ok";
    case StoreStatus::missing: return "missing";
    case StoreStatus::io_error: return "io_error";
    case StoreStatus::corrupt: return "corrupt";
    case StoreStatus::version: return "version";
    case StoreStatus::incomplete: return "incomplete";
    }
    return "unknown";
}

TileStore::TileStore(const Allocator* allocator) {
    allocator = allocator_or_default(allocator);
    void* block = allocator->alloc(allocator->user, sizeof(Impl), alignof(Impl), Tag::store);
    panic_if(!block, "tile store allocation failed");
    impl_ = new (block) Impl;
    impl_->allocator = allocator;
}

TileStore::~TileStore() {
    close();
    const Allocator* allocator = impl_->allocator;
    impl_->~Impl();
    allocator->free(allocator->user, impl_, sizeof(Impl), Tag::store);
}

StoreStatus TileStore::open(const char* directory) {
    ORBITAL_ASSERT(directory);
    close();
    const StoreStatus status = impl_->open_all(directory);
    if (status == StoreStatus::ok)
        impl_->open = true;
    else
        impl_->release();
    return status;
}

void TileStore::close() {
    impl_->release();
}

bool TileStore::is_open() const {
    return impl_->open;
}

const store::StoreHeader& TileStore::header() const {
    ORBITAL_ASSERT(is_open());
    return impl_->header;
}

bool TileStore::contains(unsigned level) const {
    return impl_->open && level >= impl_->header.level_min && level <= impl_->header.level_max;
}

bool TileStore::entry(PatchKey key, store::TileEntry& out) const {
    const Shard* shard = impl_->find(key);
    if (!shard)
        return false;
    const store::TileEntry* found = Impl::lookup(*shard, key);
    if (!found)
        return false;
    out = *found;
    return true;
}

bool TileStore::read(PatchKey key, TileOutput& out) const {
    Shard* shard = impl_->find(key);
    if (!shard)
        return false;
    const store::TileEntry* tile = Impl::lookup(*shard, key);
    if (!tile)
        return false;
    const TileLayout& layout = impl_->header.layouts[key.level];
    std::lock_guard guard(shard->lock);
    for (unsigned p = 0; p < layout.plane_count; p++) {
        const unsigned bytes = layout.planes[p].bytes();
        ORBITAL_ASSERT(out.planes[p].size() == bytes);
        if (!seek_to(shard->file, tile->offset + layout.offset(p)) ||
            std::fread(out.planes[p].data(), 1, bytes, shard->file) != bytes)
            return false;
    }
    out.heights = {tile->height_min, tile->height_max};
    return true;
}

} // namespace planet
