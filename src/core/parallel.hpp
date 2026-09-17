#pragma once
#include "core/small_vec.hpp"

#include <algorithm>
#include <future>
#include <thread>

namespace space {

inline constexpr unsigned parallel_workers_max = 16;

// Runs body(i) for every i in [0, count) across the cores, this thread
// included, each worker taking every worker-th index, and returns when all are
// done. For items of about equal cost; body must be safe to run concurrently.
template <class Body> void parallel_for(unsigned count, Body&& body, unsigned max_workers = parallel_workers_max) {
    const unsigned workers = std::clamp(std::min(std::thread::hardware_concurrency(), count), 1u, max_workers);
    SmallVec<std::future<void>, parallel_workers_max> pool;
    for (unsigned w = 1; w < workers; w++)
        pool.push_back(std::async(std::launch::async, [&, w] {
            for (unsigned i = w; i < count; i += workers)
                body(i);
        }));
    for (unsigned i = 0; i < count; i += workers)
        body(i);
    for (auto& worker : pool)
        worker.get();
}

} // namespace space
