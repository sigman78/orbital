#include "planet/allocator.hpp"

#include <cstdint>
#include <cstdlib>

namespace planet {

namespace {

// malloc with any power-of-two alignment: the block starts one pointer past the alignment
// padding, and that pointer slot holds the address malloc returned.
void* default_alloc(void*, std::size_t bytes, std::size_t alignment, Tag) {
    if (alignment < alignof(void*))
        alignment = alignof(void*);
    const std::size_t overhead = alignment + sizeof(void*);
    if (bytes > SIZE_MAX - overhead)
        return nullptr;
    void* raw = std::malloc(bytes + overhead);
    if (!raw)
        return nullptr;
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(raw) + sizeof(void*);
    const std::uintptr_t aligned = (start + alignment - 1) & ~(std::uintptr_t(alignment) - 1);
    reinterpret_cast<void**>(aligned)[-1] = raw;
    return reinterpret_cast<void*>(aligned);
}

void default_free(void*, void* block, std::size_t, Tag) {
    if (block)
        std::free(static_cast<void**>(block)[-1]);
}

constexpr Allocator default_instance{.alloc = default_alloc, .free = default_free, .user = nullptr};

} // namespace

const Allocator* default_allocator() {
    return &default_instance;
}

} // namespace planet
