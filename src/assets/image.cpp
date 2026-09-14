#include "assets/image_io.hpp"

#include "core/file.hpp"
#include "core/log.hpp"
#include "core/panic_if.hpp"

#include <climits>
#include <format>
#include <string>

// Wuffs: PNG decoder plus the modules it depends on. On MSVC the SIMD paths
// must be opted into explicitly; they still dispatch on CPUID at runtime.
#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__ZLIB
#if defined(_MSC_VER) && defined(_M_X64)
#define WUFFS_CONFIG__ENABLE_MSVC_CPU_ARCH__X86_64_V3
#endif
// stb_image_write: PNG encoder for screenshots (Wuffs has no PNG encoder).
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <stb_image_write.h>
#include <wuffs-v0.4.c>
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace space::assets {
namespace {

// Returns the Wuffs error text for a failed step, or nothing when it succeeded.
std::optional<std::string> failure(const wuffs_base__status& status, const char* step) {
    if (wuffs_base__status__is_ok(&status))
        return std::nullopt;
    return std::format("{}: {}", step, wuffs_base__status__message(&status));
}

std::optional<Rgba8Image> decode_png(MutableByteView bytes, std::string& error) {
    auto decoder = wuffs_png__decoder::alloc();
    if (!decoder) {
        error = "decoder allocation failed";
        return std::nullopt;
    }
    wuffs_base__io_buffer source = wuffs_base__ptr_u8__reader(bytes.data(), bytes.size(), true);
    wuffs_base__image_config config{};
    if (auto why = failure(wuffs_png__decoder__decode_image_config(decoder.get(), &config, &source), "header")) {
        error = *why;
        return std::nullopt;
    }
    const Extent2D extent{wuffs_base__pixel_config__width(&config.pixcfg),
                          wuffs_base__pixel_config__height(&config.pixcfg)};
    if (!valid_image_extent(extent)) {
        error = std::format("unsupported dimensions {}x{}", extent.width, extent.height);
        return std::nullopt;
    }
    // Decode straight into tightly packed, non-premultiplied RGBA8.
    wuffs_base__pixel_config__set(&config.pixcfg, WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL,
                                  WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, extent.width, extent.height);
    Rgba8Image image{extent, Bytes(static_cast<std::size_t>(extent.width) * extent.height * 4)};
    wuffs_base__pixel_buffer pixels{};
    if (auto why = failure(
            wuffs_base__pixel_buffer__set_from_slice(
                &pixels, &config.pixcfg, wuffs_base__make_slice_u8(image.pixels.data(), image.pixels.size())),
            "output buffer")) {
        error = *why;
        return std::nullopt;
    }
    Bytes work(wuffs_png__decoder__workbuf_len(decoder.get()).max_incl);
    wuffs_base__frame_config frame{};
    if (auto why = failure(wuffs_png__decoder__decode_frame_config(decoder.get(), &frame, &source), "frame header")) {
        error = *why;
        return std::nullopt;
    }
    if (auto why = failure(
            wuffs_png__decoder__decode_frame(decoder.get(), &pixels, &source, WUFFS_BASE__PIXEL_BLEND__SRC,
                                             wuffs_base__make_slice_u8(work.data(), work.size()), nullptr),
            "pixels")) {
        error = *why;
        return std::nullopt;
    }
    return image;
}

} // namespace

std::optional<Rgba8Image> try_load_png(const std::filesystem::path& path) {
    auto bytes = file::read(path);
    if (!bytes || bytes->empty()) {
        log::error("cannot read image file {}", path.string());
        return std::nullopt;
    }
    if (bytes->size() > INT_MAX) {
        log::error("image file too large: {}", path.string());
        return std::nullopt;
    }
    std::string error;
    auto image = decode_png(*bytes, error);
    if (!image)
        log::error("PNG decode failed for {} ({})", path.string(), error);
    return image;
}

Rgba8Image load_png(const std::filesystem::path& path) {
    auto image = try_load_png(path);
    panic_if(!image, "required image is missing or unreadable: {}", path.string());
    return std::move(*image);
}

bool save_png(const std::filesystem::path& path, ImageView image) {
    const auto extent = image.extent();
    const auto channels = image.channels();
    const auto pixels = image.bytes();
    // stb indexes rows using signed int arithmetic, including the stride product.
    if (image.row_stride() > std::size_t(INT_MAX) / extent.height) {
        log::error("PNG dimensions exceed encoder limits: {}", path.string());
        return false;
    }
    Bytes encoded;
    encoded.reserve(std::size_t(extent.width) * extent.height * channels / 2);
    const auto append = [](void* context, void* data, int size) {
        auto* out = static_cast<Bytes*>(context);
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        out->insert(out->end(), bytes, bytes + size);
    };
    const int stride = static_cast<int>(image.row_stride());
    if (!stbi_write_png_to_func(append, &encoded, static_cast<int>(extent.width), static_cast<int>(extent.height),
                                static_cast<int>(channels), pixels.data(), stride)) {
        log::error("PNG encode failed for {}", path.string());
        return false;
    }
    if (!file::write(path, encoded)) {
        log::error("cannot write {}", path.string());
        return false;
    }
    return true;
}

} // namespace space::assets
