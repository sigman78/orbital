#include "core/file.hpp"
#include "planet/crater_terrain.hpp"
#include "planet/patch.hpp"
#include "planet/tile_sampling.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
using namespace planet;
using namespace space;
bool image(const char* path) {
    constexpr unsigned side = 512, border = side + 2;
    const MinorPlanetTerrain terrain(1007);
    const double component = -84. / 110, phi = .914007112654, r = std::sqrt(1 - component * component);
    const auto coord = cube_coordinates(Vec3d{r * std::sin(phi), component, r * std::cos(phi)});
    constexpr double extent = .035, step = extent / side;
    const auto direction = [&](int x, int y) {
        return cube_direction(coord.face, coord.s + (x - side * .5) * step, coord.t + (y - side * .5) * step);
    };
    std::vector<float> h(border * border);
    const auto at = [&](int x, int y) -> float& { return h[(y + 1) * border + x + 1]; };
    for (int y = -1; y <= int(side); ++y)
        for (int x = -1; x <= int(side); ++x)
            at(x, y) = terrain.detail(direction(x, y), 1e6, 1);
    std::string out = "P5\n" + std::to_string(side) + " " + std::to_string(side) + "\n255\n";
    const Vec3d light = normalized(Vec3d{-.6, .3, 1});
    for (int y = 0; y < int(side); ++y)
        for (int x = 0; x < int(side); ++x) {
            const Vec3d d = direction(x, y), us = direction(x + 1, y) - direction(x - 1, y),
                        vt = direction(x, y + 1) - direction(x, y - 1);
            const Vec3d u = normalized(us), v = normalized(vt);
            const double ds = (at(x + 1, y) - at(x - 1, y)) / length(us),
                         dt = (at(x, y + 1) - at(x, y - 1)) / length(vt), c = dot(u, v);
            const Vec3d g = u * ((ds - c * dt) / (1 - c * c)) + v * ((dt - c * ds) / (1 - c * c));
            const auto frame = patch_tangent_frame(coord.face, d);
            const Vec3d normal = normalized(Vec3d{-dot(g, frame.tangent), -dot(g, frame.bitangent), 1});
            const double shade = .1 + .9 * std::max(0., dot(normal, light));
            out.push_back(char(std::lround(std::clamp(shade, 0., 1.) * 255)));
        }
    return file::write_text(path, out);
}
int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--image")
        return image(argv[2]) ? 0 : 1;
    std::puts("seed,axis,samples,jumps_over_1e_minus8,max_jump,large_epsilon_jump,plane,phi");
    for (unsigned seed : {1007u, 1008u, 2024u}) {
        MinorPlanetTerrain terrain(seed);
        for (int axis = 0; axis < 3; ++axis) {
            double worst = 0, large = 0, angle = 0;
            int plane = 0, count = 0, samples = 0;
            for (int n = -100; n <= 100; n += 2)
                for (int k = 0; k < 64; ++k) {
                    double phi = (k + .31) * 2 * pi<double> / 64;
                    auto d = [&](double e) {
                        double x = (n + e) / 110, r = std::sqrt(1 - x * x);
                        Vec3d v{x, r * std::cos(phi), r * std::sin(phi)};
                        return axis == 0 ? v : axis == 1 ? Vec3d{v.z, v.x, v.y} : Vec3d{v.y, v.z, v.x};
                    };
                    double small = std::abs(double(terrain.detail(d(1e-7), 1e6, 1)) - terrain.detail(d(-1e-7), 1e6, 1));
                    ++samples;
                    count += small > 1e-8;
                    if (small > worst) {
                        worst = small;
                        large = std::abs(double(terrain.detail(d(1e-5), 1e6, 1)) - terrain.detail(d(-1e-5), 1e6, 1));
                        plane = n;
                        angle = phi;
                    }
                }
            std::printf("%u,%d,%d,%d,%.12g,%.12g,%d,%.12g\n", seed, axis, samples, count, worst, large, plane, angle);
        }
    }
}
