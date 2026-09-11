#include "core/log.hpp"

namespace space::log {

void write(std::FILE* stream, std::string_view prefix, std::string_view line) {
    std::fwrite(prefix.data(), 1, prefix.size(), stream);
    std::fwrite(line.data(), 1, line.size(), stream);
    std::fputc('\n', stream);
    if (stream == stderr)
        std::fflush(stream);
}

} // namespace space::log
