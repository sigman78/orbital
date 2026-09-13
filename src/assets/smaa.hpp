#pragma once

#include <cstddef>
#include <span>

namespace space::assets {

inline constexpr unsigned smaa_area_width = 160, smaa_area_height = 560, smaa_area_channels = 2;
inline constexpr unsigned smaa_search_width = 64, smaa_search_height = 16, smaa_search_channels = 1;
inline constexpr std::size_t smaa_area_size = smaa_area_width * smaa_area_height * smaa_area_channels;
inline constexpr std::size_t smaa_search_size = smaa_search_width * smaa_search_height * smaa_search_channels;

// Immutable lookup data embedded at build time from third_party/smaa/*.bin.
std::span<const unsigned char, smaa_area_size> smaa_area();
std::span<const unsigned char, smaa_search_size> smaa_search();

} // namespace space::assets
