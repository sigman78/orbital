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
using AsteroidInstance = ::AsteroidInstance;
using Root = ::Root;
using RockData = ::RockData;
using DrawArgs = ::DrawArgs;
using CullParams = ::CullParams;
using CullScratch = ::CullScratch;
using CullRoot = ::CullRoot;
static_assert(sizeof(Root) == 40 && sizeof(Vertex) == 32 && sizeof(Instance) == 80);
static_assert(sizeof(AsteroidInstance) == 48 && offsetof(AsteroidInstance, payload) == 16 &&
              offsetof(AsteroidInstance, previous) == 32);
static_assert(sizeof(CullRoot) == 48 && sizeof(RockData) == 48 && sizeof(DrawArgs) == 32);
static_assert(sizeof(MeterRoot) == 24 && sizeof(MeterHistogram) == (ORBITAL_METER_BINS + 4) * 4);
// Scalar layout the shader sees: float4 members first, then 8-byte pointers and 4-byte words.
static_assert(offsetof(CullParams, index_counts) == 160 && sizeof(CullParams) == 160 + 12 * ORBITAL_ROCK_GROUPS);
static_assert(offsetof(CullScratch, instances) == sizeof(CullParams) &&
              offsetof(CullScratch, candidates) == sizeof(CullParams) + 8 &&
              offsetof(CullScratch, counts) == sizeof(CullParams) + 16);
static_assert(offsetof(CullScratch, draw_count) == sizeof(CullParams) + 16 + 8 * (ORBITAL_ROCK_GROUPS + 1));
static_assert(offsetof(CullScratch, args) == offsetof(CullScratch, draw_count) + 16);
static_assert(offsetof(FrameData, camera_time) == 64 && sizeof(FrameData) == 1088);
static_assert(std::is_trivially_copyable_v<FrameData>);
} // namespace space::render
