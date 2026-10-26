#include "planet/streamer.hpp"

#include "core/panic.hpp"
#include "core/panic_if.hpp"
#include "core/parallel.hpp"

#include <algorithm>
#include <chrono>
#include <new>
#include <thread>

namespace planet {

using namespace space; // the host's worker pool

struct TileStreamer::Impl {
    struct Result {
        TileRequest request;
        TileStaging staging;
        Range<float> heights;
        float milliseconds = 0;
        bool produced = false;
    };
    TileProvider& provider;
    TileSink& sink;
    unsigned workers;
    WorkerPool<Result> pool;
    const Allocator* allocator = nullptr; // what this Impl was allocated from

    Impl(TileProvider& provider, TileSink& sink, unsigned workers)
        : provider(provider), sink(sink), workers(workers), pool(workers) {}
};

namespace {

unsigned worker_count(unsigned requested) {
    if (requested)
        return requested;
    const unsigned cores = std::thread::hardware_concurrency();
    return std::max(1u, cores > 2 ? cores - 2 : 1u);
}

} // namespace

TileStreamer::TileStreamer(const Config& config) {
    ORBITAL_ASSERT(config.provider && config.sink);
    const Allocator* allocator = allocator_or_default(config.allocator);
    void* block = allocator->alloc(allocator->user, sizeof(Impl), alignof(Impl), Tag::tiles);
    panic_if(!block, "tile streamer allocation failed");
    impl_ = new (block) Impl(*config.provider, *config.sink, worker_count(config.workers));
    impl_->allocator = allocator;
}

TileStreamer::~TileStreamer() {
    const Allocator* allocator = impl_->allocator;
    impl_->~Impl();
    allocator->free(allocator->user, impl_, sizeof(Impl), Tag::tiles);
}

void TileStreamer::pump(Clod& clod) {
    for (const Impl::Result& result : impl_->pool.poll()) {
        if (result.produced) {
            impl_->sink.complete(result.request, result.staging, result.heights, result.milliseconds);
        } else {
            impl_->sink.abandon(result.staging);
            clod.release(result.request.slot, result.request.key, result.request.stamp);
        }
    }
}

void TileStreamer::dispatch(Clod& clod, float detail) {
    for (const Clod::Generation& generation : clod.generate()) {
        const TileLayout* layout = &impl_->provider.layout(generation.key.level); // constant per level
        TileStaging staging;
        if (!impl_->sink.acquire(layout->bytes(), staging)) {
            // Nothing else may hold the slot: visit() re-requests only keys with none, and eviction
            // passes over anything not resident, so an unserved slot would keep its key forever.
            clod.release(generation.slot, generation.key, generation.stamp);
            continue;
        }
        ORBITAL_ASSERT(staging.bytes.size() >= layout->bytes());
        const TileRequest request{
            .key = generation.key, .slot = generation.slot, .stamp = generation.stamp, .detail = detail};
        TileProvider* provider = &impl_->provider;
        impl_->pool.submit([provider, request, staging, layout]() -> Impl::Result {
            const auto start = std::chrono::steady_clock::now();
            TileOutput out;
            for (unsigned i = 0; i < layout->plane_count; i++)
                out.planes[i] = staging.bytes.subspan(layout->offset(i), layout->planes[i].bytes());
            const bool produced = provider->produce(request, out);
            return {.request = request,
                    .staging = staging,
                    .heights = out.heights,
                    .milliseconds =
                        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count(),
                    .produced = produced};
        });
    }
}

unsigned TileStreamer::in_flight() const {
    return impl_->pool.in_flight();
}

unsigned TileStreamer::workers() const {
    return impl_->workers;
}

} // namespace planet
