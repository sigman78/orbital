#pragma once
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>

namespace space::render {

// Immediate destruction: complete recorded/submitted GPU uses before replacing
// or destroying an owner. Borrowed handles/ranges do not extend its lifetime.
class UniqueGpuHeap {
public:
    constexpr UniqueGpuHeap() noexcept = default;
    explicit constexpr UniqueGpuHeap(gpu::GpuHeap heap) noexcept : heap_(heap) {}
    ~UniqueGpuHeap() noexcept { reset(); }
    UniqueGpuHeap(const UniqueGpuHeap&) = delete;
    UniqueGpuHeap& operator=(const UniqueGpuHeap&) = delete;
    constexpr UniqueGpuHeap(UniqueGpuHeap&& other) noexcept : heap_(other.heap_) { other.heap_ = {}; }
    UniqueGpuHeap& operator=(UniqueGpuHeap&& other) noexcept {
        if (this != &other) {
            reset();
            heap_ = other.heap_;
            other.heap_ = {};
        }
        return *this;
    }
    [[nodiscard]] static UniqueGpuHeap create(gpu::Device* device, gpu::uint64 bytes,
                                              gpu::MemoryType memory = gpu::MemoryType::cpu_visible) noexcept {
        return UniqueGpuHeap(gpu::create_gpu_heap(device, bytes, memory));
    }
    constexpr gpu::GpuHeap get() const noexcept { return heap_; }
    constexpr gpu::GpuCpuRange<gpu::byte> range() const noexcept { return heap_.range; }
    void reset() noexcept {
        gpu::destroy_gpu_heap(heap_);
        heap_ = {};
    }

private:
    gpu::GpuHeap heap_{};
};

class UniquePso {
public:
    constexpr UniquePso() noexcept = default;
    explicit constexpr UniquePso(gpu::PSO* pipeline) noexcept : pipeline_(pipeline) {}
    ~UniquePso() noexcept { reset(); }
    UniquePso(const UniquePso&) = delete;
    UniquePso& operator=(const UniquePso&) = delete;
    constexpr UniquePso(UniquePso&& other) noexcept : pipeline_(other.pipeline_) { other.pipeline_ = nullptr; }
    UniquePso& operator=(UniquePso&& other) noexcept {
        if (this != &other) {
            reset();
            pipeline_ = other.pipeline_;
            other.pipeline_ = nullptr;
        }
        return *this;
    }
    constexpr gpu::PSO* get() const noexcept { return pipeline_; }
    void reset() noexcept {
        gpu::destroy_pso(pipeline_);
        pipeline_ = nullptr;
    }

private:
    gpu::PSO* pipeline_ = nullptr;
};

} // namespace space::render
