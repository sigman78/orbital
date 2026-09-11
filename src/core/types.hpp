#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

// Names for the byte containers that cross module boundaries. Owned data is
// Bytes; borrowed, read-only data is ByteView. Prefer ByteView in signatures
// whenever the callee does not need to keep the data.
namespace space {

using Bytes = std::vector<std::uint8_t>;
using ByteView = std::span<const std::uint8_t>;
using MutableByteView = std::span<std::uint8_t>;

// Reinterprets a contiguous range of trivially copyable values as raw bytes.
template <class T> ByteView bytes_of(std::span<const T> items) {
    static_assert(std::is_trivially_copyable_v<T>);
    return {reinterpret_cast<const std::uint8_t*>(items.data()), items.size_bytes()};
}

template <class T> ByteView bytes_of(const std::vector<T>& items) {
    return bytes_of(std::span<const T>(items));
}

} // namespace space
