#pragma once
#include "planet/allocator.hpp"
#include "planet/clod.hpp"
#include "planet/tile.hpp"

#include <cstddef>
#include <span>

namespace planet {

// Staging for one tile on its way to the GPU: the bytes the provider fills, four-byte aligned for
// its texel writes, and the host's name for the slot so it can find it again when the tile comes back.
struct TileStaging {
    std::span<std::byte> bytes;
    unsigned token = 0;
};

// The host side of streaming. Every call happens on the host thread.
class TileSink {
public:
    virtual ~TileSink() = default;
    // Staging for a tile of `bytes` bytes, or false when none is free this frame: the generation
    // is then handed back to the Clod, which asks again.
    virtual bool acquire(unsigned bytes, TileStaging& staging) = 0;
    // A produced tile: the host uploads it and marks the slot resident under the request's stamp.
    virtual void complete(const TileRequest& request, const TileStaging& staging, Range<float> heights,
                          float milliseconds) = 0;
    // The provider could not produce the tile: the staging is free again and the slot released.
    virtual void abandon(const TileStaging& staging) = 0;
};

// Runs a provider on worker threads for the generations a Clod asks for, and hands the finished
// tiles to the sink on the host thread. The provider and the sink outlive the streamer.
class TileStreamer {
public:
    struct Config {
        TileProvider* provider = nullptr;
        TileSink* sink = nullptr;
        unsigned workers = 0; // 0: all cores but two, at least one
        const Allocator* allocator = nullptr;
    };
    explicit TileStreamer(const Config& config);
    ~TileStreamer();
    TileStreamer(const TileStreamer&) = delete;
    TileStreamer& operator=(const TileStreamer&) = delete;

    // Before clod.update(): finished tiles reach the sink, failed ones release their slot.
    void pump(Clod& clod);
    // After clod.update(): one job per generation; a generation that finds no staging is released.
    void dispatch(Clod& clod, float detail);
    unsigned in_flight() const; // submitted, not yet pumped
    unsigned workers() const;

private:
    struct Impl;
    Impl* impl_; // allocated through the config's allocator
};

} // namespace planet
