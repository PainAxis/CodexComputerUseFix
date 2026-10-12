#pragma once
#include <windows.h>
#include <cstdlib>

namespace capture_compat {
using ProtectPageFn = BOOL(WINAPI*)(LPVOID, SIZE_T, DWORD, PDWORD);
using ProtectionFailureFn = void(*)();

[[noreturn]] inline void StopOnProtectionFailure() {
    OutputDebugStringW(L"[CodexCaptureCompat] Cannot restore memory protection; terminating capture process.\n");
    RaiseFailFastException(nullptr, nullptr, 0);
    // A debugger can resume a fail-fast exception. Do not continue with an
    // unexpectedly writable IAT/vtable even in that case.
    TerminateProcess(GetCurrentProcess(), 0xC0000409u);
    std::abort();
}

inline bool RestorePageProtection(void* address, SIZE_T bytes, DWORD original,
                                  ProtectPageFn protect = &VirtualProtect,
                                  ProtectionFailureFn fatal = &StopOnProtectionFailure) {
    DWORD ignored = 0;
    for (unsigned attempt = 0; attempt != 3; ++attempt) {
        if (protect(address, bytes, original, &ignored)) return true;
    }
    // Production never returns. Tests inject a recorder to exercise this path
    // without modifying real memory or terminating the test runner.
    fatal();
    return false;
}
}
