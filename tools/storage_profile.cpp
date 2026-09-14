// Single-threaded C++ allocation probe. Requested bytes, not process RSS or
// allocator overhead; direct malloc allocations are outside this measurement.
#include "assets/material_catalog.hpp"
#include "assets/texture.hpp"
#include "core/file.hpp"
#include "core/panic.hpp"
#include "scene/system.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>

namespace probe {
struct Counters {
    std::size_t allocations = 0, bytes = 0, live = 0, peak = 0;
};
struct Header {
    void* base;
    std::size_t size, generation;
};
thread_local Counters counters;
thread_local std::size_t generation = 0;
thread_local bool active = false;

void* allocate(std::size_t size, std::size_t alignment) {
    alignment = std::max(alignment, alignof(Header));
    const auto overhead = sizeof(Header) + alignment - 1;
    if (size > std::numeric_limits<std::size_t>::max() - overhead)
        throw std::bad_alloc();
    void* base = std::malloc(size + overhead);
    if (!base)
        throw std::bad_alloc();
    const auto start = reinterpret_cast<std::uintptr_t>(base) + sizeof(Header);
    void* result = reinterpret_cast<void*>((start + alignment - 1) & ~(alignment - 1));
    auto* header = static_cast<Header*>(result) - 1;
    *header = {base, size, active ? generation : 0};
    if (active) {
        ++counters.allocations;
        counters.bytes += size;
        counters.live += size;
        counters.peak = std::max(counters.peak, counters.live);
    }
    return result;
}
void release(void* pointer) noexcept {
    if (!pointer)
        return;
    const auto* header = static_cast<Header*>(pointer) - 1;
    if (active && header->generation == generation)
        counters.live -= header->size;
    std::free(header->base);
}
} // namespace probe

void* operator new(std::size_t n) {
    return probe::allocate(n, alignof(std::max_align_t));
}
void* operator new[](std::size_t n) {
    return ::operator new(n);
}
void operator delete(void* p) noexcept {
    probe::release(p);
}
void operator delete[](void* p) noexcept {
    probe::release(p);
}
void operator delete(void* p, std::size_t) noexcept {
    probe::release(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    probe::release(p);
}
void* operator new(std::size_t n, std::align_val_t a) {
    return probe::allocate(n, std::size_t(a));
}
void* operator new[](std::size_t n, std::align_val_t a) {
    return ::operator new(n, a);
}
void operator delete(void* p, std::align_val_t) noexcept {
    probe::release(p);
}
void operator delete[](void* p, std::align_val_t) noexcept {
    probe::release(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    probe::release(p);
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
    probe::release(p);
}

namespace {
using namespace space;
using namespace space::assets;
volatile double sink = 0;

template <class Work> void measure(const char* name, unsigned repeat, unsigned operations, Work work) {
    probe::counters = {};
    ++probe::generation;
    probe::active = true;
    const auto start = std::chrono::steady_clock::now();
    work();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    probe::active = false;
    const auto counts = probe::counters;
    ORBITAL_ASSERT(counts.live == 0); // every scoped result must be released before the snapshot
    std::printf("%s,%u,%u,%.6f,%zu,%zu,%zu\n", name, repeat, operations, ms, counts.allocations, counts.bytes,
                counts.peak);
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: storage_profile <assets/materials directory>\n");
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    const auto system = generate_system(showcase_seed);
    struct Source {
        std::filesystem::path path;
        MaterialDesc desc;
        Bytes cache;
    };
    std::vector<Source> sources;
    for (const auto& entry : material_catalog) {
        const auto path = directory / entry.file;
        auto bytes = file::read(texture_cache_path(path, TextureFormat::BC7));
        if (!bytes)
            bytes = file::read(texture_cache_path(path));
        ORBITAL_ASSERT(bytes && read_texture_cache(*bytes, entry.desc, std::nullopt));
        sources.push_back({path, entry.desc, std::move(*bytes)});
    }
    std::size_t payload = 0, mips = 0;
    for (const auto& source : sources) {
        const auto texture = read_texture_cache(source.cache, source.desc, std::nullopt);
        mips += texture->mips.size();
        for (const auto& mip : texture->mips)
            payload += mip.bytes.size();
    }
    std::fprintf(stderr, "catalog=%zu textures, mips=%zu, payload=%zu bytes, bodies=%zu\n", sources.size(), mips,
                 payload, system.bodies.size());
    std::puts("case,repeat,operations,ms,allocations,allocated_bytes,peak_live_bytes");
    for (unsigned repeat = 0; repeat < 3; ++repeat) {
        measure("allocator-control", repeat, 1, [] {
            void* a = ::operator new(64);
            void* b = ::operator new(128, std::align_val_t(64));
            ORBITAL_ASSERT(probe::counters.allocations == 2 && probe::counters.peak == 192);
            ORBITAL_ASSERT(reinterpret_cast<std::uintptr_t>(b) % 64 == 0);
            ::operator delete(a);
            ::operator delete(b, std::align_val_t(64));
        });
        measure("scene-evaluate", repeat, 100000, [&] {
            for (unsigned i = 0; i < 100000; ++i) {
                const auto states = evaluate_system(system, i / 60.0);
                ORBITAL_ASSERT(states.size() == system.bodies.size());
                sink = states.back().position.x;
            }
        });
        measure("scene-validate", repeat, 100000, [&] {
            for (unsigned i = 0; i < 100000; ++i)
                ORBITAL_ASSERT(validate_system(system).empty());
        });
        measure("cache-hash-only", repeat, unsigned(sources.size()), [&] {
            for (const auto& source : sources)
                sink = double(texture_hash(ByteView(source.cache).subspan(64)));
        });
        measure("cache-parse-retained", repeat, unsigned(sources.size()), [&] {
            std::vector<TextureData> textures;
            textures.reserve(sources.size());
            for (const auto& source : sources) {
                auto texture = read_texture_cache(source.cache, source.desc, std::nullopt);
                ORBITAL_ASSERT(texture);
                textures.push_back(std::move(*texture));
            }
        });
        // Optimistic slab baseline: checksum plus one payload allocation/copy per
        // texture, with no metadata or header validation. This is a lower bound
        // for fresh slab storage, not a replacement cache reader.
        measure("cache-slab-lower-bound", repeat, unsigned(sources.size()), [&] {
            std::vector<Bytes> slabs;
            slabs.reserve(sources.size());
            for (const auto& source : sources) {
                sink = double(texture_hash(ByteView(source.cache).subspan(64)));
                slabs.emplace_back(source.cache.begin() + 64, source.cache.end());
            }
        });
        measure("cache-load-retained", repeat, unsigned(sources.size()), [&] {
            std::vector<TextureData> textures;
            textures.reserve(sources.size());
            for (const auto& source : sources) {
                auto texture = load_texture_cache(source.path, source.desc, {.bc7 = true});
                ORBITAL_ASSERT(texture);
                textures.push_back(std::move(*texture));
            }
        });
        // Representative conversion paths: sRGB, mask, and normals, including PNG decode.
        for (unsigned index : {0u, 2u, 3u})
            measure(material_catalog[index].file.data(), repeat, 1, [&] {
                auto texture = texture_from_images(load_material(sources[index].path, sources[index].desc));
                ORBITAL_ASSERT(valid_texture(texture));
                sink = double(texture.mips.front().bytes.size());
            });
    }
}
