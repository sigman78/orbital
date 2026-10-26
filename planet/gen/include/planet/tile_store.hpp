#pragma once
#include "planet/allocator.hpp"
#include "planet/store_format.hpp"
#include "planet/tile.hpp"

namespace planet {

enum class StoreStatus : unsigned char {
    ok,
    missing,    // no store.planet in the directory
    io_error,   // a file could not be opened or read
    corrupt,    // a header or index does not describe its file
    version,    // written by another format version
    incomplete, // a shard the store header promises is missing
};
const char* store_status_name(StoreStatus status);

// The read side of a body's tile store: the header and every shard's index in memory, the tiles
// read from the shard files on demand. Reads are serialized per shard by a lock, so a worker pool
// may call read() from any thread once open() has returned.
class TileStore {
public:
    explicit TileStore(const Allocator* allocator = nullptr);
    ~TileStore();
    TileStore(const TileStore&) = delete;
    TileStore& operator=(const TileStore&) = delete;

    // Opens the directory's store and its shards; anything but ok leaves the store closed.
    StoreStatus open(const char* directory);
    void close();
    bool is_open() const;
    const store::StoreHeader& header() const; // valid while open

    // Whether the store holds the level at every face it covers.
    bool contains(unsigned level) const;
    // Fills the output planes from the tile's bytes, plane by plane in the level's layout, and its
    // height range; false when the key is not stored or the read failed.
    bool read(PatchKey key, TileOutput& out) const;
    // The index entry of a key, for tools; false when the key is not stored.
    bool entry(PatchKey key, store::TileEntry& out) const;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace planet
