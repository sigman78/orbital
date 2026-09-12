#pragma once

#include <cstdint>

namespace space {

// The interval must be shorter than one wrap of the GPU's timestamp counter.
constexpr std::uint64_t timestamp_ticks(std::uint64_t begin, std::uint64_t end, unsigned valid_bits) {
    const auto mask = valid_bits >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << valid_bits) - 1;
    return (end - begin) & mask;
}

} // namespace space
