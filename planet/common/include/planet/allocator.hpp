#pragma once
#include <cstddef>

namespace planet {

// What an allocation is for, so a host can account for the library's memory by purpose.
enum class Tag : unsigned char { general, nodes, slots, tiles, store, cook, count };

// The host's allocator: every allocation the library makes goes through one of these. A null
// Allocator* anywhere in the API means default_allocator(), which forwards to malloc and free.
// alignment is a power of two; free receives the size it was given.
struct Allocator {
    void* (*alloc)(void* user, std::size_t bytes, std::size_t alignment, Tag tag) = nullptr;
    void (*free)(void* user, void* block, std::size_t bytes, Tag tag) = nullptr;
    void* user = nullptr;
};

const Allocator* default_allocator();
inline const Allocator* allocator_or_default(const Allocator* allocator) {
    return allocator ? allocator : default_allocator();
}

} // namespace planet
