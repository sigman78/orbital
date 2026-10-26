#include "planet/crater_terrain.hpp"
#include "planet/patch.hpp"
#include "planet/tile_sampling.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
constexpr std::uint64_t fnv_offset = 14695981039346656037ull;
constexpr std::uint64_t fnv_prime = 1099511628211ull;
constexpr unsigned repeats_target_ms = 75;

void hash_byte(std::uint64_t& hash, std::uint8_t byte) {
    hash = (hash ^ byte) * fnv_prime;
}

template <typename T> void hash_values(std::uint64_t& hash, std::span<const T> values) {
    for (const T value : values) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        for (std::size_t i = 0; i < sizeof(T); ++i)
            hash_byte(hash, bytes[i]);
    }
}

std::array<planet::PatchKey, 4> keys_for(unsigned level) {
    // Four nearby cells centred around s=.6, t=.3 on +Z (face 4).
    const unsigned cells = 1u << level;
    const unsigned x = unsigned(std::floor((.6 + 1.) * .5 * cells));
    const unsigned y = unsigned(std::floor((.3 + 1.) * .5 * cells));
    return {{{4, std::uint8_t(level), std::uint16_t(x), std::uint16_t(y)},
             {4, std::uint8_t(level), std::uint16_t(x + 1), std::uint16_t(y)},
             {4, std::uint8_t(level), std::uint16_t(x), std::uint16_t(y + 1)},
             {4, std::uint8_t(level), std::uint16_t(x + 1), std::uint16_t(y + 1)}}};
}

void emit_row(const char* operation, unsigned level, std::uint64_t seed, unsigned target_ms,
              const planet::MinorPlanetTerrain& terrain) {
    constexpr std::size_t key_count = 4;
    const auto keys = keys_for(level);
    std::array<std::vector<float>, key_count> heights;
    std::array<std::vector<std::uint8_t>, key_count> albedo;
    std::array<std::vector<std::uint16_t>, key_count> slopes;
    const bool colour = std::string_view(operation) == "colour";
    for (std::size_t i = 0; i < key_count; ++i) {
        if (colour) {
            albedo[i].resize(planet::tile_colour_side * planet::tile_colour_side * 4);
            slopes[i].resize(planet::tile_colour_side * planet::tile_colour_side * 2);
        } else {
            heights[i].resize(planet::tile_side * planet::tile_side);
        }
    }

    const auto generate = [&] {
        for (std::size_t i = 0; i < key_count; ++i) {
            if (colour)
                planet::generate_colour_tiles(terrain, keys[i], albedo[i], slopes[i]);
            else
                planet::generate_height_tile(terrain, keys[i], heights[i]);
        }
    };
    generate(); // warmup; also supplies the deterministic output checksum
    std::uint64_t checksum = fnv_offset;
    for (std::size_t i = 0; i < key_count; ++i) {
        if (colour) {
            hash_values(checksum, std::span<const std::uint8_t>(albedo[i]));
            hash_values(checksum, std::span<const std::uint16_t>(slopes[i]));
        } else {
            hash_values(checksum, std::span<const float>(heights[i]));
        }
    }

    unsigned rounds = 0;
    auto start = Clock::now();
    do {
        generate();
        ++rounds;
    } while (std::chrono::duration<double, std::milli>(Clock::now() - start).count() < target_ms);
    const double elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    const double tiles = double(rounds) * key_count;
    const std::size_t texels = colour ? std::size_t(planet::tile_colour_side) * planet::tile_colour_side
                                      : std::size_t(planet::tile_side) * planet::tile_side;
    const std::size_t cpu_bytes = colour ? texels * 8 : texels * sizeof(float);
    // RGBA8 albedo plus RG16 tangent slopes are both four bytes per colour texel.
    const std::size_t gpu_bytes = colour ? texels * 8 : texels * sizeof(std::uint16_t);
    constexpr unsigned quads = planet::tile_side - 1;
    constexpr unsigned triangles_per_tile = (quads * quads + 4 * quads) * 2;
    // Count terrain calls separately; colour evaluates a bordered height/detail field,
    // then albedo once per output texel. The un-cached stencil path asks for five directions
    // per output texel in addition to the bordered height/detail lattice directions.
    const std::size_t bordered_texels = std::size_t(planet::tile_colour_side + 2) * (planet::tile_colour_side + 2);
    const std::size_t height_evals = colour ? bordered_texels : texels;
    const std::size_t detail_evals = colour ? bordered_texels : 0;
    const std::size_t albedo_evals = colour ? texels : 0;
    const std::size_t direction_evals = colour ? bordered_texels + texels * 5 : texels;
    std::cout << operation << ',' << level << ',' << seed << ',' << key_count << ',' << rounds << ',' << std::fixed
              << std::setprecision(4) << elapsed_ms / tiles << ',' << (tiles * 1000. / elapsed_ms) << ',' << texels
              << ',' << cpu_bytes << ',' << gpu_bytes << ',' << triangles_per_tile << ',' << height_evals << ','
              << detail_evals << ',' << albedo_evals << ',' << direction_evals << ",0x" << std::hex << checksum
              << std::dec << '\n';
}

} // namespace

int main(int argc, char** argv) {
    std::uint64_t seed = 1007;
    unsigned target_ms = repeats_target_ms;
    if (argc > 1)
        seed = std::strtoull(argv[1], nullptr, 10);
    if (argc > 2)
        target_ms = unsigned(std::strtoul(argv[2], nullptr, 10));
    std::cout << "operation,level,seed,keys,repeats,ms_per_tile,tiles_per_second,output_texels_per_tile,output_bytes_"
                 "cpu_per_tile,output_bytes_gpu_per_tile,triangles_per_tile,height_evals_per_tile,detail_evals_per_"
                 "tile,albedo_evals_per_tile,direction_evals_per_tile,checksum_fnv1a64\n";
    const planet::MinorPlanetTerrain terrain(seed);
    for (const unsigned level : {3u, 5u, 7u, 9u, 11u}) {
        emit_row("height", level, seed, target_ms, terrain);
        emit_row("colour", level, seed, target_ms, terrain);
    }
}
