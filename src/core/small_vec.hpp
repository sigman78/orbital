#pragma once
#include <cstddef>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace space {

// Sequence with room for N elements inline and a heap fallback beyond that.
// For the small, bounded collections that appear in startup and per-frame
// code (a handful of uploads, workers, resolved bodies) where a std::vector
// would mean a heap allocation for three items. Move-only: a copy of one of
// these is never what the caller wants.
template <class T, std::size_t N> class SmallVec {
public:
    SmallVec() = default;
    SmallVec(const SmallVec&) = delete;
    SmallVec& operator=(const SmallVec&) = delete;
    SmallVec(SmallVec&& other) noexcept { take(std::move(other)); }
    SmallVec& operator=(SmallVec&& other) noexcept {
        if (this != &other) {
            clear();
            take(std::move(other));
        }
        return *this;
    }
    ~SmallVec() { clear(); }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    static constexpr std::size_t inline_capacity() { return N; }

    T* data() { return spilled_ ? heap_.data() : inline_data(); }
    const T* data() const { return spilled_ ? heap_.data() : inline_data(); }
    T& operator[](std::size_t index) { return data()[index]; }
    const T& operator[](std::size_t index) const { return data()[index]; }
    T& back() { return data()[size_ - 1]; }
    T* begin() { return data(); }
    T* end() { return data() + size_; }
    const T* begin() const { return data(); }
    const T* end() const { return data() + size_; }
    operator std::span<T>() { return {data(), size_}; }
    operator std::span<const T>() const { return {data(), size_}; }

    template <class... Args> T& emplace_back(Args&&... args) {
        if (!spilled_ && size_ == N)
            spill();
        T* slot;
        if (spilled_) {
            slot = &heap_.emplace_back(std::forward<Args>(args)...);
        } else {
            slot = ::new (static_cast<void*>(inline_data() + size_)) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *slot;
    }
    void push_back(const T& value) { emplace_back(value); }
    void push_back(T&& value) { emplace_back(std::move(value)); }

    void clear() {
        if (spilled_) {
            heap_.clear();
        } else {
            for (std::size_t i = 0; i < size_; ++i)
                inline_data()[i].~T();
        }
        size_ = 0;
    }

private:
    alignas(T) std::byte storage_[N * sizeof(T)];
    std::vector<T> heap_;
    std::size_t size_ = 0;
    bool spilled_ = false;

    T* inline_data() { return std::launder(reinterpret_cast<T*>(storage_)); }
    const T* inline_data() const { return std::launder(reinterpret_cast<const T*>(storage_)); }

    // Moves the inline elements onto the heap once the inline capacity is used up.
    void spill() {
        heap_.reserve(N * 2);
        for (std::size_t i = 0; i < size_; ++i) {
            heap_.push_back(std::move(inline_data()[i]));
            inline_data()[i].~T();
        }
        spilled_ = true;
    }

    void take(SmallVec&& other) {
        if (other.spilled_) {
            heap_ = std::move(other.heap_);
            spilled_ = true;
        } else {
            for (std::size_t i = 0; i < other.size_; ++i)
                ::new (static_cast<void*>(inline_data() + i)) T(std::move(other.inline_data()[i]));
        }
        size_ = other.size_;
        other.clear();
        other.spilled_ = false;
    }
};

} // namespace space
