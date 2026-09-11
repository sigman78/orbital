#include "kernels.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#if defined(_M_X64) || defined(__x86_64__)
#define ORBITAL_KERNELS_X64 1
#include <immintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#define ORBITAL_TARGET_AVX2
#else
#define ORBITAL_TARGET_AVX2 __attribute__((target("avx2")))
#endif
#else
#define ORBITAL_KERNELS_X64 0
#endif

namespace space::assets::kernels {
namespace {

bool detect_avx2() {
#if ORBITAL_KERNELS_X64 && defined(_MSC_VER)
    int info[4];
    __cpuid(info, 0);
    if (info[0] < 7)
        return false;
    __cpuid(info, 1);
    const bool osxsave = (info[2] & (1 << 27)) != 0, avx = (info[2] & (1 << 28)) != 0;
    if (!osxsave || !avx || (_xgetbv(0) & 6) != 6)
        return false;
    __cpuidex(info, 7, 0);
    return (info[1] & (1 << 5)) != 0;
#elif ORBITAL_KERNELS_X64
    return __builtin_cpu_supports("avx2");
#else
    return false;
#endif
}

const bool use_avx2 = detect_avx2();

// Built once; the only double-precision math in the module, so the table stays
// identical to the values the renderer has always uploaded.
const std::array<std::uint8_t, 256>& srgb_table() {
    static const auto table = [] {
        std::array<std::uint8_t, 256> result{};
        for (unsigned i = 0; i < result.size(); ++i) {
            const double s = i / 255.0;
            const double linear = s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
            result[i] = static_cast<std::uint8_t>(std::lround(std::clamp(linear, 0.0, 1.0) * 255.0));
        }
        return result;
    }();
    return table;
}

// Normal encoding: byte = (n * 0.5 + 0.5) * 255 = n * 127.5 + 127.5. A 2x2 block
// sum of decoded vectors is (byte_sum - 510) / 127.5; only its direction matters.
constexpr float normal_offset = 510.f, normal_scale = 127.5f;

// One output pixel of the box filter with the general edge rules.
inline void box_pixel(const std::uint8_t* row0, const std::uint8_t* row1, std::uint32_t width, std::uint32_t x,
                      std::uint8_t* out) {
    const std::uint8_t* p00 = row0 + std::size_t((2 * x) % width) * 4;
    const std::uint8_t* p01 = row0 + std::size_t((2 * x + 1) % width) * 4;
    const std::uint8_t* p10 = row1 + std::size_t((2 * x) % width) * 4;
    const std::uint8_t* p11 = row1 + std::size_t((2 * x + 1) % width) * 4;
    for (unsigned c = 0; c < 4; ++c)
        out[c] = static_cast<std::uint8_t>((p00[c] + p01[c] + p10[c] + p11[c] + 2) / 4);
}

// Scalar normal-map output pixel from the four-sample channel sums.
inline void encode_normal(unsigned sum_r, unsigned sum_g, unsigned sum_b, unsigned sum_a, std::uint8_t* out) {
    float nx = float(sum_r) - normal_offset, ny = float(sum_g) - normal_offset, nz = float(sum_b) - normal_offset;
    const float dot = nx * nx + ny * ny + nz * nz;
    if (dot > .5f) {
        const float inverse = 1.f / std::sqrt(dot);
        nx *= inverse;
        ny *= inverse;
        nz *= inverse;
    } else {
        nx = ny = 0.f;
        nz = 1.f;
    }
    out[0] = static_cast<std::uint8_t>(std::lrintf(nx * normal_scale + normal_scale));
    out[1] = static_cast<std::uint8_t>(std::lrintf(ny * normal_scale + normal_scale));
    out[2] = static_cast<std::uint8_t>(std::lrintf(nz * normal_scale + normal_scale));
    out[3] = static_cast<std::uint8_t>((sum_a + 2) / 4);
}

inline void normal_pixel(const std::uint8_t* row0, const std::uint8_t* row1, std::uint32_t width, std::uint32_t x,
                         std::uint8_t* out) {
    const std::uint8_t* p00 = row0 + std::size_t((2 * x) % width) * 4;
    const std::uint8_t* p01 = row0 + std::size_t((2 * x + 1) % width) * 4;
    const std::uint8_t* p10 = row1 + std::size_t((2 * x) % width) * 4;
    const std::uint8_t* p11 = row1 + std::size_t((2 * x + 1) % width) * 4;
    unsigned sums[4];
    for (unsigned c = 0; c < 4; ++c)
        sums[c] = p00[c] + p01[c] + p10[c] + p11[c];
    encode_normal(sums[0], sums[1], sums[2], sums[3], out);
}

#if ORBITAL_KERNELS_X64
// Loads four source pixels from each row and returns their 2x2 sums as eight
// 16-bit lanes: output pixel 0 (RGBA) then output pixel 1 (RGBA).
inline __m128i block_sums_sse2(const std::uint8_t* row0, const std::uint8_t* row1, std::uint32_t x) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(row0 + std::size_t(x) * 8));
    const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(row1 + std::size_t(x) * 8));
    // Widen to 16 bits and add the two rows: lo holds pixels 0,1 and hi pixels 2,3.
    const __m128i lo = _mm_add_epi16(_mm_unpacklo_epi8(a, zero), _mm_unpacklo_epi8(b, zero));
    const __m128i hi = _mm_add_epi16(_mm_unpackhi_epi8(a, zero), _mm_unpackhi_epi8(b, zero));
    // Add horizontally adjacent pixels, then gather both results into one register.
    const __m128i s0 = _mm_add_epi16(lo, _mm_srli_si128(lo, 8));
    const __m128i s1 = _mm_add_epi16(hi, _mm_srli_si128(hi, 8));
    return _mm_unpacklo_epi64(s0, s1);
}

// Two output pixels per step.
inline std::uint32_t box_row_sse2(const std::uint8_t* row0, const std::uint8_t* row1, std::uint32_t out_width,
                                  std::uint8_t* out, std::uint32_t x) {
    const __m128i two = _mm_set1_epi16(2);
    for (; x + 2 <= out_width; x += 2) {
        const __m128i sum = _mm_srli_epi16(_mm_add_epi16(block_sums_sse2(row0, row1, x), two), 2);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(out + std::size_t(x) * 4), _mm_packus_epi16(sum, sum));
    }
    return x;
}

// Four output pixels per step from eight source pixels of each row; the same
// dance as SSE2 performed in both 128-bit lanes at once, unrolled twice.
ORBITAL_TARGET_AVX2 std::uint32_t box_row_avx2(const std::uint8_t* row0, const std::uint8_t* row1,
                                               std::uint32_t out_width, std::uint8_t* out) {
    const __m256i zero = _mm256_setzero_si256(), two = _mm256_set1_epi16(2);
    std::uint32_t x = 0;
    for (; x + 8 <= out_width; x += 8) {
        for (unsigned k = 0; k < 2; ++k) {
            const std::size_t src = std::size_t(x + 4 * k) * 8;
            const __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(row0 + src));
            const __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(row1 + src));
            const __m256i lo = _mm256_add_epi16(_mm256_unpacklo_epi8(a, zero), _mm256_unpacklo_epi8(b, zero));
            const __m256i hi = _mm256_add_epi16(_mm256_unpackhi_epi8(a, zero), _mm256_unpackhi_epi8(b, zero));
            const __m256i s0 = _mm256_add_epi16(lo, _mm256_srli_si256(lo, 8));
            const __m256i s1 = _mm256_add_epi16(hi, _mm256_srli_si256(hi, 8));
            __m256i sum = _mm256_unpacklo_epi64(s0, s1);
            sum = _mm256_srli_epi16(_mm256_add_epi16(sum, two), 2);
            // Each lane now holds two packed output pixels in its low 8 bytes.
            const __m256i packed = _mm256_permute4x64_epi64(_mm256_packus_epi16(sum, sum), 0x08);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + std::size_t(x + 4 * k) * 4),
                             _mm256_castsi256_si128(packed));
        }
    }
    return x;
}

// Normalizes one output pixel whose 2x2 channel sums arrive as four 32-bit lanes.
inline void encode_normal_sse2(__m128i sums, std::uint8_t* out) {
    const __m128 offset = _mm_set_ps(0.f, normal_offset, normal_offset, normal_offset);
    const __m128 scale = _mm_set1_ps(normal_scale);
    const __m128 fallback = _mm_set_ps(0.f, 1.f, 0.f, 0.f);
    const __m128i rgb_mask = _mm_set_epi32(0, -1, -1, -1);
    // [x, y, z, 0]: the alpha lane must not leak into the dot product.
    const __m128 v = _mm_and_ps(_mm_sub_ps(_mm_cvtepi32_ps(sums), offset), _mm_castsi128_ps(rgb_mask));
    const __m128 squares = _mm_mul_ps(v, v);
    // dot = x*x + y*y + z*z, broadcast to every lane.
    const __m128 xy = _mm_add_ps(squares, _mm_shuffle_ps(squares, squares, _MM_SHUFFLE(1, 0, 3, 2)));
    const __m128 dot = _mm_add_ps(xy, _mm_shuffle_ps(xy, xy, _MM_SHUFFLE(2, 3, 0, 1)));
    const __m128 valid = _mm_cmpgt_ps(dot, _mm_set1_ps(.5f));
    const __m128 normalized = _mm_div_ps(v, _mm_sqrt_ps(dot));
    const __m128 n = _mm_or_ps(_mm_and_ps(valid, normalized), _mm_andnot_ps(valid, fallback));
    const __m128i encoded = _mm_cvtps_epi32(_mm_add_ps(_mm_mul_ps(n, scale), scale));
    // Alpha is the plain box average: (sum + 2) / 4, kept in the integer domain.
    const __m128i alpha = _mm_srli_epi32(_mm_add_epi32(sums, _mm_set1_epi32(2)), 2);
    const __m128i pixel = _mm_or_si128(_mm_and_si128(encoded, rgb_mask), _mm_andnot_si128(rgb_mask, alpha));
    const __m128i words = _mm_packs_epi32(pixel, pixel);
    const int bytes = _mm_cvtsi128_si32(_mm_packus_epi16(words, words));
    std::memcpy(out, &bytes, 4);
}

inline std::uint32_t normal_row_sse2(const std::uint8_t* row0, const std::uint8_t* row1, std::uint32_t out_width,
                                     std::uint8_t* out) {
    const __m128i zero = _mm_setzero_si128();
    std::uint32_t x = 0;
    for (; x + 2 <= out_width; x += 2) {
        const __m128i sums = block_sums_sse2(row0, row1, x);
        encode_normal_sse2(_mm_unpacklo_epi16(sums, zero), out + std::size_t(x) * 4);
        encode_normal_sse2(_mm_unpackhi_epi16(sums, zero), out + std::size_t(x + 1) * 4);
    }
    return x;
}
#endif

} // namespace

void srgb_to_linear(std::uint8_t* rgba, std::size_t pixel_count) {
    const auto& table = srgb_table();
    std::uint8_t* end = rgba + pixel_count * 4;
    for (; rgba + 16 <= end; rgba += 16) {
        rgba[0] = table[rgba[0]], rgba[1] = table[rgba[1]], rgba[2] = table[rgba[2]];
        rgba[4] = table[rgba[4]], rgba[5] = table[rgba[5]], rgba[6] = table[rgba[6]];
        rgba[8] = table[rgba[8]], rgba[9] = table[rgba[9]], rgba[10] = table[rgba[10]];
        rgba[12] = table[rgba[12]], rgba[13] = table[rgba[13]], rgba[14] = table[rgba[14]];
    }
    for (; rgba < end; rgba += 4)
        rgba[0] = table[rgba[0]], rgba[1] = table[rgba[1]], rgba[2] = table[rgba[2]];
}

void luminance_to_alpha(std::uint8_t* rgba, std::size_t pixel_count) {
    for (std::uint8_t* end = rgba + pixel_count * 4; rgba < end; rgba += 4) {
        const unsigned luma = 54u * rgba[0] + 183u * rgba[1] + 19u * rgba[2];
        rgba[0] = rgba[1] = rgba[2] = 255;
        rgba[3] = static_cast<std::uint8_t>((luma + 128) >> 8);
    }
}

void downsample_rgba8(const std::uint8_t* src, std::uint32_t width, std::uint32_t height, std::uint8_t* dst) {
    const std::uint32_t out_width = half_extent(width), out_height = half_extent(height);
    const std::size_t row_bytes = std::size_t(width) * 4;
    for (std::uint32_t y = 0; y < out_height; ++y) {
        const std::uint8_t* row0 = src + std::min(height - 1, 2 * y) * row_bytes;
        const std::uint8_t* row1 = src + std::min(height - 1, 2 * y + 1) * row_bytes;
        std::uint8_t* out = dst + std::size_t(y) * out_width * 4;
        std::uint32_t x = 0;
        // Vector paths need two real source columns per output; only a 1-wide
        // source (which wraps onto itself) falls entirely to the scalar tail.
#if ORBITAL_KERNELS_X64
        if (width >= 2) {
            if (use_avx2)
                x = box_row_avx2(row0, row1, out_width, out);
            x = box_row_sse2(row0, row1, out_width, out, x);
        }
#endif
        for (; x < out_width; ++x)
            box_pixel(row0, row1, width, x, out + std::size_t(x) * 4);
    }
}

void downsample_normals(const std::uint8_t* src, std::uint32_t width, std::uint32_t height, std::uint8_t* dst) {
    const std::uint32_t out_width = half_extent(width), out_height = half_extent(height);
    const std::size_t row_bytes = std::size_t(width) * 4;
    for (std::uint32_t y = 0; y < out_height; ++y) {
        const std::uint8_t* row0 = src + std::min(height - 1, 2 * y) * row_bytes;
        const std::uint8_t* row1 = src + std::min(height - 1, 2 * y + 1) * row_bytes;
        std::uint8_t* out = dst + std::size_t(y) * out_width * 4;
        std::uint32_t x = 0;
#if ORBITAL_KERNELS_X64
        if (width >= 2)
            x = normal_row_sse2(row0, row1, out_width, out);
#endif
        for (; x < out_width; ++x)
            normal_pixel(row0, row1, width, x, out + std::size_t(x) * 4);
    }
}

void downsample_reference(const std::uint8_t* src, std::uint32_t width, std::uint32_t height, std::uint8_t* dst,
                          bool normal_map) {
    const std::uint32_t out_width = half_extent(width), out_height = half_extent(height);
    for (std::uint32_t y = 0; y < out_height; ++y) {
        for (std::uint32_t x = 0; x < out_width; ++x) {
            unsigned sums[4]{};
            for (unsigned dy = 0; dy < 2; ++dy) {
                const auto sy = std::min(height - 1, y * 2 + dy);
                for (unsigned dx = 0; dx < 2; ++dx) {
                    const auto sx = (x * 2 + dx) % width;
                    const auto q = (static_cast<std::size_t>(sy) * width + sx) * 4;
                    for (unsigned c = 0; c < 4; ++c)
                        sums[c] += src[q + c];
                }
            }
            std::uint8_t* out = dst + (static_cast<std::size_t>(y) * out_width + x) * 4;
            if (normal_map) {
                encode_normal(sums[0], sums[1], sums[2], sums[3], out);
            } else {
                for (unsigned c = 0; c < 4; ++c)
                    out[c] = static_cast<std::uint8_t>((sums[c] + 2) / 4);
            }
        }
    }
}

const char* backend() {
#if ORBITAL_KERNELS_X64
    return use_avx2 ? "avx2" : "sse2";
#else
    return "scalar";
#endif
}

} // namespace space::assets::kernels
