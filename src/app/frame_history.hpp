#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace space::app {

// The last few seconds of raw frame times, for the panel's graph and the
// percentile in the title: a fixed ring, plotted in place through the plot's
// offset, sorted only when a percentile is asked for.
class FrameHistory {
public:
    static constexpr std::size_t capacity = 240; // four seconds at 60 Hz, about the graph's width in pixels

    void push(float ms) {
        values_[next_] = ms;
        next_ = (next_ + 1) % capacity;
        count_ = std::min(count_ + 1, capacity);
    }
    // The samples as stored; oldest first once the ring has wrapped, from offset().
    std::span<const float> values() const { return {values_.data(), count_}; }
    int offset() const { return count_ < capacity ? 0 : int(next_); }
    float peak() const { return count_ ? *std::max_element(values_.begin(), values_.begin() + count_) : 0.f; }
    // The frame time at the given rank, 0.95 for the 95th percentile; zero while empty.
    float percentile(float rank) const {
        if (!count_)
            return 0;
        std::vector<float> sorted(values_.begin(), values_.begin() + count_);
        const auto nth = sorted.begin() + std::min(count_ - 1, std::size_t(float(count_) * rank));
        std::nth_element(sorted.begin(), nth, sorted.end());
        return *nth;
    }

private:
    std::array<float, capacity> values_{};
    std::size_t next_ = 0, count_ = 0;
};

} // namespace space::app
