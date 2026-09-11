#include "assets.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace space::assets {
namespace {
constexpr std::uint32_t version = 2;
constexpr double pi = 3.14159265358979323846;
std::uint64_t hash(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}
double fade(double x) {
    return x * x * x * (x * (x * 6 - 15) + 10);
}
double lattice(std::uint64_t s, int x, int y, int z) {
    auto ux = std::uint64_t(std::int64_t(x)), uy = std::uint64_t(std::int64_t(y)), uz = std::uint64_t(std::int64_t(z));
    return double(hash(s ^ hash(ux * 0x632be59bd9b4e019ull) ^ hash(uy * 0x8cb92baa3f4f1b95ull) ^
                       hash(uz * 0x517cc1b727220a95ull)) &
                  0xffffu) /
               32767.5 -
           1;
}
double noise(std::uint64_t s, double x, double y, double z) {
    int ix = int(std::floor(x)), iy = int(std::floor(y)), iz = int(std::floor(z));
    double fx = fade(x - ix), fy = fade(y - iy), fz = fade(z - iz), a[2][2][2];
    for (int k = 0; k < 2; k++)
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < 2; i++)
                a[k][j][i] = lattice(s, ix + i, iy + j, iz + k);
    auto l = [](double p, double q, double t) { return p + (q - p) * t; };
    return l(l(l(a[0][0][0], a[0][0][1], fx), l(a[0][1][0], a[0][1][1], fx), fy),
             l(l(a[1][0][0], a[1][0][1], fx), l(a[1][1][0], a[1][1][1], fx), fy), fz);
}
double fbm(std::uint64_t s, double x, double y, double z, int n = 5) {
    double sum = 0, amp = .5, norm = 0;
    for (int i = 0; i < n; i++) {
        sum += amp * noise(s + std::uint64_t(i) * 101, x, y, z);
        norm += amp;
        x *= 2.03;
        y *= 2.03;
        z *= 2.03;
        amp *= .5;
    }
    return sum / norm;
}
double ridge(std::uint64_t s, double x, double y, double z) {
    return 1 - std::abs(fbm(s, x, y, z, 4));
}
double sat(double x) {
    return std::clamp(x, 0., 1.);
}
std::uint8_t byte(double x) {
    return std::uint8_t(std::lround(sat(x) * 255));
}
using Color = std::array<double, 3>;
Color mix(Color a, Color b, double t) {
    t = sat(t);
    return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t};
}
Image mip(const Image& in) {
    if (in.width <= 1 && in.height <= 1)
        return in;
    Image o{std::max(1u, in.width / 2), std::max(1u, in.height / 2), {}};
    o.pixels.resize(std::size_t(o.width) * o.height * 4);
    for (std::uint32_t y = 0; y < o.height; y++)
        for (std::uint32_t x = 0; x < o.width; x++)
            for (int c = 0; c < 4; c++) {
                unsigned s = 0;
                for (unsigned dy = 0; dy < 2; dy++) {
                    auto sy = std::min(in.height - 1, 2 * y + dy);
                    for (unsigned dx = 0; dx < 2; dx++) {
                        auto sx = (2 * x + dx) % in.width;
                        s += in.pixels[(std::size_t(sy) * in.width + sx) * 4 + c];
                    }
                }
                o.pixels[(std::size_t(y) * o.width + x) * 4 + c] = std::uint8_t((s + 2) / 4);
            }
    return o;
}
void write_image(std::ofstream& f, const Image& i) {
    std::uint64_t n = i.pixels.size();
    f.write((char*)&i.width, 4);
    f.write((char*)&i.height, 4);
    f.write((char*)&n, 8);
    f.write((char*)i.pixels.data(), std::streamsize(n));
}
bool read_image(std::ifstream& f, Image& i, std::uint32_t ew, std::uint32_t eh) {
    std::uint64_t n = 0;
    std::uint32_t w = 0, h = 0;
    if (!f.read((char*)&w, 4) || !f.read((char*)&h, 4) || !f.read((char*)&n, 8))
        return false;
    std::uint64_t expected = std::uint64_t(ew) * eh * 4;
    if (w != ew || h != eh || n != expected || n > std::uint64_t(std::numeric_limits<std::size_t>::max()))
        return false;
    std::vector<std::uint8_t> p;
    p.resize(static_cast<std::size_t>(n));
    if (n && !f.read((char*)p.data(), std::streamsize(n)))
        return false;
    i = {w, h, std::move(p)};
    return true;
}
bool read_cache(const std::filesystem::path& p, std::uint64_t seed, bool gas, std::uint32_t width, PlanetAssets& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f)
        return false;
    char m[8];
    std::uint32_t v = 0, w = 0, n = 0;
    std::uint64_t s = 0;
    std::uint8_t g = 0;
    if (!f.read(m, 8) || std::string(m, 8) != "SPASSET1" || !f.read((char*)&v, 4) || !f.read((char*)&s, 8) ||
        !f.read((char*)&g, 1) || !f.read((char*)&w, 4) || !f.read((char*)&n, 4))
        return false;
    std::uint32_t en = 1, tw = width, th = width / 2;
    while (tw > 1 || th > 1) {
        ++en;
        tw = std::max(1u, tw / 2);
        th = std::max(1u, th / 2);
    }
    if (v != version || s != seed || g != std::uint8_t(gas) || w != width || n != en || n > 16)
        return false;
    PlanetAssets c;
    c.surface_mips.resize(n);
    c.cloud_mips.resize(n);
    tw = width;
    th = width / 2;
    for (auto& i : c.surface_mips) {
        if (!read_image(f, i, tw, th))
            return false;
        tw = std::max(1u, tw / 2);
        th = std::max(1u, th / 2);
    }
    tw = width;
    th = width / 2;
    for (auto& i : c.cloud_mips) {
        if (!read_image(f, i, tw, th))
            return false;
        tw = std::max(1u, tw / 2);
        th = std::max(1u, th / 2);
    }
    if (f.peek() != std::ifstream::traits_type::eof())
        return false;
    out = std::move(c);
    return true;
}
void replace_file(const std::filesystem::path& t, const std::filesystem::path& p) {
#ifdef _WIN32
    if (!MoveFileExW(t.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ec;
        std::filesystem::remove(t, ec);
        throw std::runtime_error("cannot atomically replace asset cache");
    }
#else
    std::error_code ec;
    std::filesystem::rename(t, p, ec);
    if (ec) {
        std::filesystem::remove(t);
        throw std::runtime_error("cannot atomically replace asset cache");
    }
#endif
}
} // namespace
PlanetAssets generate_planet(std::uint64_t seed, bool gas, std::uint32_t width) {
    if (width < 2 || width > 4096 || width % 2)
        throw std::invalid_argument("planet width must be even and in 2..4096");
    std::uint32_t height = width / 2;
    Image surface{width, height, std::vector<std::uint8_t>(std::size_t(width) * height * 4)}, clouds = surface;
    for (std::uint32_t y = 0; y < height; y++) {
        double lat = (.5 - (y + .5) / height) * pi, sl = std::sin(lat), cl = std::cos(lat);
        for (std::uint32_t x = 0; x < width; x++) {
            double lon = ((x + .5) / width * 2 - 1) * pi, px = cl * std::cos(lon), py = sl, pz = cl * std::sin(lon);
            std::size_t q = (std::size_t(y) * width + x) * 4;
            Color color{};
            double alpha;
            if (!gas) {
                double broad = fbm(seed, px * 1.55, py * 1.55, pz * 1.55),
                       warp = fbm(seed + 31, px * 3.1 + broad * .75, py * 3.1, pz * 3.1 - broad * .75),
                       continental = broad * .78 + warp * .22 - .055, coast = sat(continental * 24 + .5),
                       mount = ridge(seed + 73, px * 7, py * 7, pz * 7) * sat((continental - .055) * 5),
                       detail = fbm(seed + 109, px * 15, py * 15, pz * 15, 3), depth = sat(-continental * 8),
                       shelf = sat(continental * 32 + .55);
                Color ocean = mix({.006, .025, .075}, {.015, .12, .22}, 1 - depth);
                ocean = mix(ocean, {.025, .20, .27}, shelf * .35);
                double temp = sat(1.05 - std::abs(py) * 1.15 - mount * .42 +
                                  .1 * fbm(seed + 211, px * 2, py * 2, pz * 2, 3)),
                       wet = sat(.52 + .48 * fbm(seed + 307, px * 3.5, py * 3.5, pz * 3.5, 4) - mount * .22);
                Color land = mix({.34, .25, .10}, {.07, .29, .075}, sat((wet - .28) * 2.2));
                land = mix(land, {.30, .22, .13}, sat((.38 - temp) * 2.5));
                land = mix(land, {.28, .27, .24}, sat(mount * .9 + detail * .08));
                double ice = sat((std::abs(py) - .78) * 8 + (1 - temp) * .28 +
                                 noise(seed + 401, px * 9, py * 9, pz * 9) * .18);
                color = mix(mix(ocean, land, coast), {.78, .87, .91}, ice);
                alpha = 1 - coast;
            } else {
                double shear = fbm(seed + 19, px * 2.2, py * 8, pz * 2.2, 4),
                       fine = fbm(seed + 47, px * 7, py * 25, pz * 7, 3), phase = lat * 28 + shear * 2.8,
                       band = .5 + .5 * std::sin(phase), fil = .5 + .5 * std::sin(phase * 2.15 + fine * 3);
                color = mix({.34, .20, .105}, {.88, .72, .48}, band);
                color = mix(color, {.95, .86, .66}, fil * .28);
                double dx = std::remainder(std::atan2(pz, px) - .42, 2 * pi), dy = lat + .18,
                       rad = std::sqrt((dx * cl / .34) * (dx * cl / .34) + (dy / .16) * (dy / .16)),
                       ang = std::atan2(dy / .16, dx * cl / .34),
                       swirl = std::exp(-rad * rad * 1.7) * (.5 + .5 * std::sin(18 * rad - ang * 3 + fine * 2)),
                       core = std::exp(-rad * rad * 4.2);
                color = mix(color, {.78, .31, .13}, sat(core * .75 + swirl * .45));
                color = mix(color, {.98, .78, .49}, swirl * .42);
                alpha = sat(.18 + .62 * (.55 * band + .45 * fil));
            }
            surface.pixels[q] = byte(color[0]);
            surface.pixels[q + 1] = byte(color[1]);
            surface.pixels[q + 2] = byte(color[2]);
            surface.pixels[q + 3] = byte(alpha);
            double flow = fbm(seed + 991, px * 4 + py * .9, py * 7, pz * 4 - py * .9),
                   wisps = fbm(seed + 1237, px * 13, py * 5, pz * 13, 3),
                   ca = sat((flow * .72 + wisps * .28 - .03) * 2.25);
            ca *= .88 - sat((std::abs(py) - .88) * 5) * .35;
            if (gas)
                ca = sat(.16 + .45 * ca);
            clouds.pixels[q] = byte(.78 + .20 * ca);
            clouds.pixels[q + 1] = byte(.82 + .16 * ca);
            clouds.pixels[q + 2] = byte(.86 + .13 * ca);
            clouds.pixels[q + 3] = byte(std::min(.8, ca));
        }
    }
    PlanetAssets out;
    out.surface_mips.push_back(std::move(surface));
    out.cloud_mips.push_back(std::move(clouds));
    while (out.surface_mips.back().width > 1 || out.surface_mips.back().height > 1) {
        out.surface_mips.push_back(mip(out.surface_mips.back()));
        out.cloud_mips.push_back(mip(out.cloud_mips.back()));
    }
    return out;
}
PlanetAssets load_or_generate(const std::filesystem::path& path, std::uint64_t seed, bool gas, std::uint32_t width) {
    PlanetAssets out;
    if (read_cache(path, seed, gas, width, out))
        return out;
    out = generate_planet(seed, gas, width);
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    auto tmp = path;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        const char m[8] = {'S', 'P', 'A', 'S', 'S', 'E', 'T', '1'};
        std::uint8_t g = gas;
        std::uint32_t n = std::uint32_t(out.surface_mips.size());
        f.write(m, 8);
        f.write((char*)&version, 4);
        f.write((char*)&seed, 8);
        f.write((char*)&g, 1);
        f.write((char*)&width, 4);
        f.write((char*)&n, 4);
        for (auto& i : out.surface_mips)
            write_image(f, i);
        for (auto& i : out.cloud_mips)
            write_image(f, i);
        f.flush();
        if (!f)
            throw std::runtime_error("cannot write asset cache");
    }
    replace_file(tmp, path);
    return out;
}
void save_ppm_preview(const std::filesystem::path& path, const Image& i) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << i.width << ' ' << i.height << "\n255\n";
    for (std::size_t q = 0; q < i.pixels.size(); q += 4)
        f.put(char(i.pixels[q])).put(char(i.pixels[q + 1])).put(char(i.pixels[q + 2]));
    if (!f)
        throw std::runtime_error("cannot write PPM preview");
}
} // namespace space::assets
