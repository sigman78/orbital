#pragma once
#include "core/types.hpp"

// Build provenance and historical profiling only; runtime texture loading does not hash.
namespace space::tooling {

inline std::uint64_t texture_hash(ByteView bytes) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace space::tooling
