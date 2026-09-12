#include "platform/process.hpp"

#include "core/panic.hpp"

namespace space::platform {

void init_process() {}

std::filesystem::path executable_directory() {
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    panic_if(bool(error), "cannot locate the executable directory: {}", error.message());
    return executable.parent_path();
}

} // namespace space::platform
