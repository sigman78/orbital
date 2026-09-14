#pragma once
#include "core/panic.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

namespace space {

// Fixed-capacity sequence stored entirely inline: N slots of T and a size
// counter no wider than N needs, no heap, no pointer. Exceeding the capacity
// is a programming error and panics; use try_push_back where dropping the
// overflow is the intended behaviour (input queues), and std::vector where
// the count is not bounded by a compile-time fact or validated input.
template <class T, std::size_t N> class SmallVec {
    static_assert(N > 0);

public:
    using size_type = std::conditional_t<(N <= UINT8_MAX), std::uint8_t,
                                         std::conditional_t<(N <= UINT16_MAX), std::uint16_t, std::uint32_t>>;

    SmallVec() = default;
    SmallVec(const SmallVec& other) { copy_from(other); }
    SmallVec(SmallVec&& other) noexcept { move_from(std::move(other)); }
    SmallVec& operator=(const SmallVec& other) {
        if (this != &other) {
            clear();
            copy_from(other);
        }
        return *this;
    }
    SmallVec& operator=(SmallVec&& other) noexcept {
        if (this != &other) {
            clear();
            move_from(std::move(other));
        }
        return *this;
    }
    ~SmallVec() { clear(); }

    static constexpr std::size_t capacity() { return N; }
    constexpr std::size_t size() const { return size_; }
    constexpr bool empty() const { return size_ == 0; }
    constexpr bool full() const { return size_ == N; }

    T* data() { return std::launder(reinterpret_cast<T*>(storage_)); }
    const T* data() const { return std::launder(reinterpret_cast<const T*>(storage_)); }
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
        ORBITAL_ASSERT(size_ < N && "SmallVec capacity exceeded");
        T* slot = ::new (static_cast<void*>(data() + size_)) T(std::forward<Args>(args)...);
        ++size_;
        return *slot;
    }
    void push_back(const T& value) { emplace_back(value); }
    void push_back(T&& value) { emplace_back(std::move(value)); }

    // False, with nothing changed, when the sequence is already full.
    bool try_push_back(const T& value) {
        if (full())
            return false;
        emplace_back(value);
        return true;
    }

    void clear() {
        for (std::size_t i = 0; i < size_; ++i)
            data()[i].~T();
        size_ = 0;
    }

private:
    alignas(T) std::byte storage_[N * sizeof(T)];
    size_type size_ = 0;

    void copy_from(const SmallVec& other) {
        for (std::size_t i = 0; i < other.size_; ++i)
            ::new (static_cast<void*>(data() + i)) T(other.data()[i]);
        size_ = other.size_;
    }

    void move_from(SmallVec&& other) {
        for (std::size_t i = 0; i < other.size_; ++i)
            ::new (static_cast<void*>(data() + i)) T(std::move(other.data()[i]));
        size_ = other.size_;
        other.clear();
    }
};

} // namespace space
