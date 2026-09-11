#pragma once
#ifdef __cplusplus
#include <cstdint>
struct alignas(16) ShaderFloat4 { float x, y, z, w; };
using ShaderMatrix4 = float[16];
#define SHADER_FLOAT4 ShaderFloat4
#define SHADER_MATRIX4 ShaderMatrix4
#define SHADER_ADDRESS std::uint64_t
#define SHADER_UINT std::uint32_t
#else
#define SHADER_FLOAT4 float4
#define SHADER_MATRIX4 float4x4
#define SHADER_ADDRESS(T) T*
#define SHADER_UINT uint
#endif

struct Instance { SHADER_FLOAT4 center_radius; SHADER_FLOAT4 rotation_kind; SHADER_FLOAT4 tint; };
struct Vertex { SHADER_FLOAT4 position; SHADER_FLOAT4 normal; };
struct Frame {
    SHADER_MATRIX4 view_projection;
    SHADER_FLOAT4 camera_time, right_tan, up_aspect, forward_exposure, sun;
    SHADER_FLOAT4 bodies[3];
    SHADER_FLOAT4 options, screen_sun;
    SHADER_MATRIX4 light_projection;
    SHADER_MATRIX4 previous_projection;
    SHADER_FLOAT4 previous_camera_delta, previous_forward, jitter;
};
struct Root {
#ifdef __cplusplus
    SHADER_ADDRESS frame, vertices, instances;
#else
    SHADER_ADDRESS(Frame) frame;
    SHADER_ADDRESS(Vertex) vertices;
    SHADER_ADDRESS(Instance) instances;
#endif
    SHADER_UINT base, mode;
};

#undef SHADER_FLOAT4
#undef SHADER_MATRIX4
#undef SHADER_ADDRESS
#undef SHADER_UINT
