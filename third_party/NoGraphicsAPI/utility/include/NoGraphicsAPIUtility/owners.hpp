#pragma once
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>

namespace gpu {

    // Immediate destruction: complete recorded/submitted GPU uses before replacing
    // or destroying an owner. Borrowed handles/ranges do not extend its lifetime.
    class UniqueGpuHeap
    {
      public:
        constexpr UniqueGpuHeap() noexcept = default;
        explicit constexpr UniqueGpuHeap(GpuHeap heap) noexcept : heap_(heap)
        {
        }
        ~UniqueGpuHeap() noexcept
        {
            reset();
        }
        UniqueGpuHeap(const UniqueGpuHeap&) = delete;
        UniqueGpuHeap& operator=(const UniqueGpuHeap&) = delete;
        constexpr UniqueGpuHeap(UniqueGpuHeap&& other) noexcept : heap_(other.heap_)
        {
            other.heap_ = {};
        }
        UniqueGpuHeap& operator=(UniqueGpuHeap&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                heap_ = other.heap_;
                other.heap_ = {};
            }
            return *this;
        }
        [[nodiscard]] static UniqueGpuHeap create(Device* device, uint64 bytes, MemoryType memory = MemoryType::cpu_visible) noexcept
        {
            return UniqueGpuHeap(create_gpu_heap(device, bytes, memory));
        }
        constexpr GpuHeap get() const noexcept
        {
            return heap_;
        }
        constexpr GpuCpuRange<byte> range() const noexcept
        {
            return heap_.range;
        }
        void reset() noexcept
        {
            destroy_gpu_heap(heap_);
            heap_ = {};
        }

      private:
        GpuHeap heap_{};
    };

    class UniquePso
    {
      public:
        constexpr UniquePso() noexcept = default;
        explicit constexpr UniquePso(PSO* pipeline) noexcept : pipeline_(pipeline)
        {
        }
        ~UniquePso() noexcept
        {
            reset();
        }
        UniquePso(const UniquePso&) = delete;
        UniquePso& operator=(const UniquePso&) = delete;
        constexpr UniquePso(UniquePso&& other) noexcept : pipeline_(other.pipeline_)
        {
            other.pipeline_ = nullptr;
        }
        UniquePso& operator=(UniquePso&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                pipeline_ = other.pipeline_;
                other.pipeline_ = nullptr;
            }
            return *this;
        }
        constexpr PSO* get() const noexcept
        {
            return pipeline_;
        }
        void reset() noexcept
        {
            destroy_pso(pipeline_);
            pipeline_ = nullptr;
        }

      private:
        PSO* pipeline_ = nullptr;
    };

} // namespace gpu
