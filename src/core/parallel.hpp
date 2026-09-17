#pragma once
#include "core/small_vec.hpp"

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace space {

inline constexpr unsigned parallel_workers_max = 16;

template <class T> class WorkerPool {
public:
    explicit WorkerPool(unsigned workers) {
        for (unsigned i = 0; i < workers; i++)
            threads_.emplace_back([this] { run(); });
    }
    ~WorkerPool() {
        {
            std::lock_guard lock(job_mutex_);
            stop_ = true;
        }
        job_cv_.notify_all();
        for (auto& t : threads_)
            t.join();
    }
    void submit(std::function<T()> job) {
        {
            std::lock_guard lock(job_mutex_);
            jobs_.push(std::move(job));
        }
        job_cv_.notify_one();
    }
    std::vector<T> poll() {
        std::lock_guard lock(finished_mutex_);
        std::vector<T> out;
        std::swap(out, finished_);
        return out;
    }

private:
    void run() {
        for (;;) {
            std::function<T()> job;
            {
                std::unique_lock lock(job_mutex_);
                job_cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
                if (stop_ && jobs_.empty())
                    return;
                job = std::move(jobs_.front());
                jobs_.pop();
            }
            T result = job();
            {
                std::lock_guard lock(finished_mutex_);
                finished_.push_back(std::move(result));
            }
        }
    }
    std::vector<std::thread> threads_;
    std::queue<std::function<T()>> jobs_;
    std::vector<T> finished_;
    std::mutex job_mutex_, finished_mutex_;
    std::condition_variable job_cv_;
    bool stop_ = false;
};

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
