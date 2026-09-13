#include "assets/material_catalog.hpp"
#include "assets/texture.hpp"
#include "core/file.hpp"
#include <cstdio>
#include <format>

int main(int argc, char** argv) {
    using namespace space;
    if (argc == 2 && std::string_view(argv[1]) == "--list") {
        for (const auto& source : assets::material_catalog)
            std::printf("%s\n", source.file.data());
        return 0;
    }
    if (argc < 3 || argc > 4 || (argc == 4 && std::string_view(argv[3]) != "--raw")) {
        std::fprintf(stderr, "Usage: texture_prepare input.png output-directory [--raw]\n");
        return 1;
    }
    const std::filesystem::path source(argv[1]), directory(argv[2]);
    const auto original = file::read(source);
    if (!original)
        return 1;
    const auto hash = assets::texture_hash(*original);
    const auto desc = assets::material_description(source.filename().string());
    const auto mips = assets::load_material(source, desc);
    const auto base = mips.front().extent;
    if (base.width > 16384 || base.height > 16384)
        return 1;
    for (std::size_t i = 0; i < mips.size(); ++i) {
        const auto& mip = mips[i];
        if (argc == 4) {
            if (!file::write(directory / std::format("{}.rgba", i), mip.pixels))
                return 1;
        } else {
            // Uncompressed KTX avoids PNG re-encoding and decoding between preparation and astcenc.
            Bytes ktx{0xab, 0x4b, 0x54, 0x58, 0x20, 0x31, 0x31, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
            for (const auto value : {0x04030201u, 0x1401u, 1u, 0x1908u, 0x8058u, 0x1908u, mip.extent.width,
                                     mip.extent.height, 0u, 0u, 1u, 1u, 0u, unsigned(mip.pixels.size())})
                for (unsigned byte = 0; byte < 4; ++byte)
                    ktx.push_back(std::uint8_t(value >> (byte * 8)));
            ktx.insert(ktx.end(), mip.pixels.begin(), mip.pixels.end());
            if (!file::write(directory / std::format("{}.ktx", i), ktx))
                return 1;
        }
    }
    const auto after = file::read(source);
    if (!after || assets::texture_hash(*after) != hash) {
        std::fprintf(stderr, "Source changed during preparation\n");
        return 1;
    }
    return file::write_text(
               directory / "metadata.json",
               std::format("{{\"width\":{},\"height\":{},\"levels\":{},\"source_hash\":{},\"flags\":{}}}\n", base.width,
                           base.height, mips.size(), hash, assets::material_flags(desc)))
               ? 0
               : 1;
}
