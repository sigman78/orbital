#include "core/file.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bc7enc.h>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <thread>

int main(int argc, char** argv) {
    if (argc != 11) {
        std::fprintf(stderr, "Usage: bc7_compress input.rgba output.blocks width height quality threads R G B A\n");
        return 1;
    }
    const auto number = [](const char* text, unsigned& value) {
        const std::string_view s(text);
        const auto parsed = std::from_chars(s.data(), s.data() + s.size(), value);
        return parsed.ec == std::errc{} && parsed.ptr == s.data() + s.size();
    };
    unsigned width = 0, height = 0, threads = 0;
    if (!number(argv[3], width) || !number(argv[4], height) || !number(argv[6], threads) || !width || !height ||
        width > 16384 || height > 16384 || threads > 16)
        return 1;
    constexpr std::array qualities{"fastest", "fast", "medium", "thorough", "verythorough", "exhaustive"};
    const auto quality = std::find(qualities.begin(), qualities.end(), std::string_view(argv[5]));
    if (quality == qualities.end())
        return 1;
    bc7enc_compress_block_params params;
    bc7enc_compress_block_params_init(&params);
    bc7enc_compress_block_params_init_linear_weights(&params);
    const auto effort = unsigned(quality - qualities.begin());
    params.m_uber_level = effort > 1 ? effort - 1 : 0;
    params.m_max_partitions = effort == 0 ? 0 : effort == 1 ? 16 : BC7ENC_MAX_PARTITIONS;
    for (unsigned i = 0; i < 4; ++i)
        if (!number(argv[7 + i], params.m_weights[i]) || !params.m_weights[i] || params.m_weights[i] > 128)
            return 1;
    const auto input = space::file::read(argv[1]);
    if (!input || input->size() != std::size_t(width) * height * 4) {
        std::fprintf(stderr, "Input is not a complete RGBA8 mip\n");
        return 1;
    }
    bc7enc_compress_block_init();
    const auto columns = (width + 3) / 4, rows = (height + 3) / 4;
    space::Bytes output(std::size_t(columns) * rows * BC7ENC_BLOCK_SIZE);
    std::atomic<unsigned> next_row{0};
    const auto worker = [&] {
        std::uint8_t block[64];
        for (auto row = next_row++; row < rows; row = next_row++) {
            for (unsigned column = 0; column < columns; ++column) {
                for (unsigned y = 0; y < 4; ++y)
                    for (unsigned x = 0; x < 4; ++x) {
                        const auto sx = std::min(column * 4 + x, width - 1), sy = std::min(row * 4 + y, height - 1);
                        std::memcpy(block + (y * 4 + x) * 4, input->data() + (std::size_t(sy) * width + sx) * 4, 4);
                    }
                bc7enc_compress_block(output.data() + (std::size_t(row) * columns + column) * BC7ENC_BLOCK_SIZE, block,
                                      &params);
            }
        }
    };
    const auto count = std::min(rows, std::clamp(threads ? threads : std::thread::hardware_concurrency(), 1u, 16u));
    std::vector<std::jthread> pool;
    for (unsigned i = 1; i < count; ++i)
        pool.emplace_back(worker);
    worker();
    pool.clear();
    return space::file::write(argv[2], output) ? 0 : 1;
}
