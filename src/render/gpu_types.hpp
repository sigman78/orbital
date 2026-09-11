#pragma once
#include "scene_shared.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace space::render {
using Float4 = ::ShaderFloat4;
using FrameData = ::Frame;
using Vertex = ::Vertex;
using Instance = ::Instance;
using Root = ::Root;
static_assert(sizeof(Root) == 32 && sizeof(Vertex) == 32 && sizeof(Instance) == 48);
static_assert(offsetof(FrameData, camera_time) == 64 && sizeof(FrameData) == 432);
static_assert(std::is_trivially_copyable_v<FrameData>);
} // namespace space::render
