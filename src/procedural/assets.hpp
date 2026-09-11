#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace space::assets {

struct Image {
    std::uint32_t width = 0, height = 0;
    std::vector<std::uint8_t> pixels; // Tightly packed RGBA8, linear-light values.
};

struct PlanetAssets {
    std::vector<Image> surface_mips;
    std::vector<Image> cloud_mips;
};

// Width must be even and in [2, 4096]; height is width / 2. Surface alpha is
// ocean coverage for terrestrial worlds and band density for gas giants.
// Cloud alpha is coverage/density in [0, .8]. Every chain ends at 1x1.
PlanetAssets generate_planet(std::uint64_t seed, bool gas, std::uint32_t width);
PlanetAssets load_or_generate(const std::filesystem::path& path, std::uint64_t seed, bool gas, std::uint32_t width);
void save_ppm_preview(const std::filesystem::path& path, const Image& image);

} // namespace space::assets
