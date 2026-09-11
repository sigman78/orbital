#include "../src/procedural/assets.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
int main() {
    using namespace space::assets;
    auto a = generate_planet(7, false, 32), b = generate_planet(7, false, 32), other = generate_planet(8, false, 32),
         g = generate_planet(7, true, 32);
    assert(a.surface_mips.size() > 1 && a.surface_mips[0].height == 16);
    assert(a.surface_mips[0].pixels == b.surface_mips[0].pixels);
    assert(a.surface_mips[0].pixels != other.surface_mips[0].pixels);
    assert(a.surface_mips[0].pixels != g.surface_mips[0].pixels);
    for (std::size_t k = 0; k < a.surface_mips.size(); ++k) {
        auto& i = a.surface_mips[k];
        assert(i.pixels.size() == std::size_t(i.width) * i.height * 4);
        assert(a.cloud_mips[k].pixels.size() == i.pixels.size());
    }
    bool ocean = false, land = false, cloud = false;
    for (std::size_t q = 3; q < a.surface_mips[0].pixels.size(); q += 4) {
        ocean |= a.surface_mips[0].pixels[q] > 220;
        land |= a.surface_mips[0].pixels[q] < 30;
        cloud |= a.cloud_mips[0].pixels[q] > 30 && a.cloud_mips[0].pixels[q] <= 204;
    }
    assert(ocean && land && cloud);
    auto p = std::filesystem::temp_directory_path() / "space_assets_test.bin";
    auto expected = generate_planet(8, true, 16);
    auto c = load_or_generate(p, 8, true, 16), d = load_or_generate(p, 8, true, 16);
    assert(c.surface_mips[0].pixels == d.surface_mips[0].pixels);
    {
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        f.write("SPASSET1", 8);
    }
    auto repaired = load_or_generate(p, 8, true, 16);
    assert(repaired.surface_mips[0].pixels == expected.surface_mips[0].pixels);
    assert(std::filesystem::file_size(p) > 8);
    {
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        char h[29] = {};
        std::copy_n("SPASSET1", 8, h);
        f.write(h, sizeof h);
        std::uint32_t bogus = 0xffffffffu;
        f.write(reinterpret_cast<const char*>(&bogus), 4);
    }
    repaired = load_or_generate(p, 8, true, 16);
    assert(repaired.surface_mips[0].pixels == expected.surface_mips[0].pixels);
    std::filesystem::remove(p);
    return 0;
}
