#include "platform/process.hpp"

#include "core/panic_if.hpp"

#include <windows.h>

#include <iterator>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

namespace space::platform {

void init_process() {
#if defined(_MSC_VER) && !defined(NDEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    SetProcessDPIAware();
}

std::filesystem::path executable_directory() {
    wchar_t buffer[32768];
    const auto length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    panic_if(!length || length >= std::size(buffer), "cannot locate the executable directory");
    return std::filesystem::path(buffer).parent_path();
}

} // namespace space::platform
