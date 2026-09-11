#pragma once
#include <cstddef>
#include <cstdint>

// Pixel kernels for material preparation. All functions operate on tightly
// packed RGBA8 through raw pointers plus dimensions: they are leaf code called
// from one place with sizes already validated, and the pointer form keeps the
// SIMD loops free of view bookkeeping. Everything above them uses ByteView.
namespace space::assets::kernels {

// Half extent of a mip level, never below 1.
constexpr std::uint32_t half_extent(std::uint32_t extent) {
    return extent > 1 ? extent / 2 : 1;
}

// Converts RGB from sRGB encoding to linear-light bytes in place; alpha is untouched.
void srgb_to_linear(std::uint8_t* rgba, std::size_t pixel_count);

// Rec.709 luma of the encoded RGB becomes alpha and RGB is set to white, in place.
void luminance_to_alpha(std::uint8_t* rgba, std::size_t pixel_count);

// 2x2 box filter to half resolution (see half_extent) with round-half-up.
// Columns wrap and rows clamp at the edge, which only matters for 1-wide or
// odd-height sources. dst must hold half_extent(width) * half_extent(height) pixels.
void downsample_rgba8(const std::uint8_t* src, std::uint32_t width, std::uint32_t height, std::uint8_t* dst);

// Same footprint as downsample_rgba8, but RGB is treated as a unit vector
// (byte / 127.5 - 1), summed over the 2x2 block and renormalized; alpha is box filtered.
void downsample_normals(const std::uint8_t* src, std::uint32_t width, std::uint32_t height, std::uint8_t* dst);

// Straightforward scalar implementation of both downsample kernels, kept as
// the correctness oracle for tests.
void downsample_reference(const std::uint8_t* src, std::uint32_t width, std::uint32_t height, std::uint8_t* dst,
                          bool normal_map);

// Instruction set selected at runtime for downsample_rgba8: "avx2", "sse2" or "scalar".
const char* backend();

} // namespace space::assets::kernels
