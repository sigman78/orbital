#include "image.hpp"

#include <climits>
#include <fstream>
#include <stdexcept>
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
#endif
#include <stb_image_write.h>
#include <wuffs-v0.4.c>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace space::assets {
namespace {

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Cannot open image file: " + path.string());
    const auto size = file.tellg();
    if (size <= 0 || size > INT_MAX)
        throw std::runtime_error("Image file is empty or too large: " + path.string());
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file)
        throw std::runtime_error("Cannot read image file: " + path.string());
    return bytes;
}

void check(const wuffs_base__status& status, const std::filesystem::path& path, const char* step) {
    if (!wuffs_base__status__is_ok(&status))
        throw std::runtime_error("PNG decode failed for '" + path.string() + "' while " + step + ": " +
                                 wuffs_base__status__message(&status));
}

} // namespace

Image load_png(const std::filesystem::path& path) {
    auto bytes = read_file(path);
    auto decoder = wuffs_png__decoder::alloc();
    if (!decoder)
        throw std::runtime_error("PNG decoder allocation failed");
    wuffs_base__io_buffer source = wuffs_base__ptr_u8__reader(bytes.data(), bytes.size(), true);
    wuffs_base__image_config config{};
    check(wuffs_png__decoder__decode_image_config(decoder.get(), &config, &source), path, "reading the header");
    const std::uint32_t width = wuffs_base__pixel_config__width(&config.pixcfg);
    const std::uint32_t height = wuffs_base__pixel_config__height(&config.pixcfg);
    constexpr std::uint64_t max_pixels = std::uint64_t{1} << 28;
    if (!width || !height || std::uint64_t(width) * height > max_pixels)
        throw std::runtime_error("PNG has unsupported dimensions: " + path.string());
    // Decode straight into tightly packed, non-premultiplied RGBA8.
    wuffs_base__pixel_config__set(&config.pixcfg, WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL,
                                  WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, width, height);
    Image image{width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4)};
    wuffs_base__pixel_buffer pixels{};
    check(wuffs_base__pixel_buffer__set_from_slice(&pixels, &config.pixcfg,
                                                   wuffs_base__make_slice_u8(image.pixels.data(), image.pixels.size())),
          path, "binding the output buffer");
    std::vector<std::uint8_t> work(wuffs_png__decoder__workbuf_len(decoder.get()).max_incl);
    wuffs_base__frame_config frame{};
    check(wuffs_png__decoder__decode_frame_config(decoder.get(), &frame, &source), path, "reading the frame header");
    check(wuffs_png__decoder__decode_frame(decoder.get(), &pixels, &source, WUFFS_BASE__PIXEL_BLEND__SRC,
                                           wuffs_base__make_slice_u8(work.data(), work.size()), nullptr),
          path, "decoding pixels");
    return image;
}

void save_png(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height, unsigned channels,
              const std::uint8_t* pixels) {
    if (!width || !height || channels < 1 || channels > 4 || !pixels)
        throw std::invalid_argument("save_png: invalid image description");
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot create image file: " + path.string());
    const auto write = [](void* context, void* data, int size) {
        static_cast<std::ofstream*>(context)->write(static_cast<const char*>(data), size);
    };
    const int stride = static_cast<int>(width * channels);
    if (!stbi_write_png_to_func(write, &file, static_cast<int>(width), static_cast<int>(height),
                                static_cast<int>(channels), pixels, stride) ||
        !file)
        throw std::runtime_error("PNG encode failed: " + path.string());
}

} // namespace space::assets
