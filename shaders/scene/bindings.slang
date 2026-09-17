#pragma once
#include "../scene_shared.h"

[[vk::binding(0, 0)]] Texture2D<float4> textures[ORBITAL_TEXTURE_COUNT];
[[vk::binding(1, 0)]] SamplerState samplers[ORBITAL_SAMPLER_COUNT];
[[vk::binding(2, 0)]] Texture2DArray<float4> textureArrays[ORBITAL_TEXTURE_ARRAY_COUNT];
