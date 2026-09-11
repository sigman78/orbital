#pragma once
#include "app/camera.hpp"
#include "scene/system.hpp"
#include <filesystem>
#include <memory>

namespace space::render {
struct Stats {
    double frame_ms{}, gpu_ms{}, shadow_ms{}, surface_ms{}, atmosphere_ms{}, post_ms{};
    unsigned visible_asteroids{}, triangles{};
};
class Renderer {
public:
    Renderer(void* hwnd, const SystemDescription&, const std::filesystem::path& executable_directory);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool draw(const Camera&, const std::vector<BodyState>&, double simulation_time, float exposure, bool high_quality,
              bool overlay, bool auto_exposure = true);
    void capture(const std::filesystem::path& path);
    const char* device_name() const;
    Stats stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace space::render
