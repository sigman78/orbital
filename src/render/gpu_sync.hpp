#pragma once
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>

namespace space::render {
struct AccessScope {
    gpu::Stage stage;
    gpu::Access access;
};
namespace access {
inline constexpr AccessScope color_write{gpu::Stage::color_output, gpu::Access::color_write};
inline constexpr AccessScope color_blend{gpu::Stage::color_output, gpu::Access::color_read | gpu::Access::color_write};
inline constexpr AccessScope fragment_sample{gpu::Stage::fragment, gpu::Access::shader_read};
inline constexpr AccessScope depth_write{gpu::Stage::depth_stencil_tests, gpu::Access::depth_stencil_write};
inline constexpr AccessScope depth_read{gpu::Stage::depth_stencil_tests, gpu::Access::depth_stencil_read};
inline constexpr AccessScope transfer_write{gpu::Stage::transfer, gpu::Access::transfer_write};
inline constexpr AccessScope transfer_read{gpu::Stage::transfer, gpu::Access::transfer_read};
inline constexpr AccessScope host_read{gpu::Stage::host, gpu::Access::host_read};
} // namespace access

// Global execution/memory dependency, not a texture state transition.
inline void synchronize(gpu::CommandBuffer* cmd, AccessScope before, AccessScope after) {
    gpu::barrier(cmd, before.stage, before.access, after.stage, after.access);
}
} // namespace space::render
