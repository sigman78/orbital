#pragma once
#include "bindings.slang"

// Surface/fullscreen pipelines use Root; culling and UI declare their own root type.
[[vk::push_constant]] ConstantBuffer<Root> root;
