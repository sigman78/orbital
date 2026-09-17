#pragma once
#include "core/log.hpp"
#include "render/renderer.hpp"
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace space::app {

// The benchmark CSV, one row per drawn frame, written as the run goes: rows
// gather in a small buffer that is flushed about once a second and at the
// end, so memory stays flat and an aborted run still leaves a file. The
// columns are read by name (tools/benchmark-suite.py) and follow the pass
// table, so a new GPU scope appears here by itself. Only the draw times are
// kept, for the summary the run ends with.
class BenchmarkLog {
public:
    static constexpr unsigned rows_per_flush = 64;
    static constexpr std::size_t warmup_frames = 60; // dropped before computing percentiles

    explicit BenchmarkLog(const std::filesystem::path& path) : file_(path, std::ios::binary | std::ios::trunc) {
        if (!file_) {
            log::error("cannot write benchmark {}", path.string());
            return;
        }
        buffer_ = "frame,frame_ms,cpu_submit_and_wait_ms,cpu_prepare_ms,cpu_belt_ms";
        for (const auto& pass : render::gpu_pass_info)
            std::format_to(std::back_inserter(buffer_), ",{}", pass.column);
        buffer_ += ",rocks,rock_candidates\n";
    }
    ~BenchmarkLog() { flush(); }
    BenchmarkLog(const BenchmarkLog&) = delete;
    BenchmarkLog& operator=(const BenchmarkLog&) = delete;

    // frame_ms is the loop's frame delta; the draw time and the passes come from the renderer.
    void record(float frame_ms, const render::FrameStats& frame) {
        if (!file_)
            return;
        std::format_to(std::back_inserter(buffer_), "{},{},{},{},{}", rows_++, frame_ms, frame.draw_ms,
                       frame.prepare_ms, frame.belt_ms);
        for (const float ms : frame.gpu.ms)
            std::format_to(std::back_inserter(buffer_), ",{}", ms);
        std::format_to(std::back_inserter(buffer_), ",{},{}\n", frame.visible_asteroids, frame.rock_candidates);
        draw_ms_.push_back(frame.draw_ms);
        if (rows_ % rows_per_flush == 0)
            flush();
    }
    // Writes what is buffered and logs the run's median and 95th percentile draw time.
    void finish() {
        flush();
        auto sorted = draw_ms_;
        if (sorted.size() > warmup_frames)
            sorted.erase(sorted.begin(), sorted.begin() + warmup_frames);
        std::sort(sorted.begin(), sorted.end());
        if (!sorted.empty()) {
            const auto p95 = sorted[std::min(sorted.size() - 1, std::size_t(double(sorted.size()) * .95))];
            log::info("Frame timing median {} ms, p95 {} ms (CPU including GPU wait; not isolated GPU timing).",
                      sorted[sorted.size() / 2], p95);
        }
    }

private:
    void flush() {
        if (file_ && !buffer_.empty()) {
            file_.write(buffer_.data(), std::streamsize(buffer_.size()));
            file_.flush();
        }
        buffer_.clear();
    }
    std::ofstream file_;
    std::string buffer_;
    std::vector<float> draw_ms_;
    unsigned rows_ = 0;
};

} // namespace space::app
