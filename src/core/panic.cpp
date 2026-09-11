#include "core/panic.hpp"

#include "core/log.hpp"

#include <cstdlib>

#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
#endif

namespace space {

void panic(std::string_view message, std::source_location location) {
    log::error("{} ({}:{})", message, location.file_name(), location.line());
    std::fflush(nullptr);
#if defined(_WIN32)
    if (IsDebuggerPresent())
        __debugbreak();
#endif
    // _Exit skips destructors on purpose: GPU objects and worker threads may be
    // in any state, and running their teardown could hang or crash again.
    std::_Exit(EXIT_FAILURE);
}

} // namespace space
