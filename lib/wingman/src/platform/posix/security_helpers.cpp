// POSIX 实现（Linux/macOS）：反调试/反 VM/签名校验无对应物恒 false，
// 内存保护走 mprotect/mlock
#include "platform/security_helpers.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <thread>
#include <sys/mman.h>
#include <unistd.h>

namespace wingman::platform {

void sleepMilliseconds(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

bool enableProcessProtection() {
    return false;
}

bool checkDebuggerPEB() {
    return false;
}

bool checkDebuggerFlags() {
    return false;
}

bool checkHardwareBreakpoints() {
    return false;
}

bool checkVMRegistry() {
    return false;
}

bool checkVMProcesses() {
    return false;
}

bool checkVMDrivers() {
    return false;
}

bool checkVMCPUID() {
    return false;
}

bool verifySignature() {
    return false;
}

CodeSignature getSignatureInfo() {
    return {};
}

bool protectMemory(void* addr, size_t size, bool protect) {
    if (!addr || size == 0) {
        return true;
    }

    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) {
        return false;
    }

    auto start = reinterpret_cast<uintptr_t>(addr);
    auto pageStart = start & ~(static_cast<uintptr_t>(pageSize) - 1U);
    auto end = start + size;
    auto pageEnd = (end + static_cast<uintptr_t>(pageSize) - 1U) & ~(static_cast<uintptr_t>(pageSize) - 1U);
    int flags = protect ? PROT_READ : (PROT_READ | PROT_WRITE);
    return mprotect(reinterpret_cast<void*>(pageStart), pageEnd - pageStart, flags) == 0;
}

void secureZero(void* ptr, size_t size) {
    volatile unsigned char* p = static_cast<volatile unsigned char*>(ptr);
    while (size--) {
        *p++ = 0;
    }
}

bool lockMemory(void* ptr, size_t size) {
    if (!ptr || size == 0) {
        return true;
    }
    return mlock(ptr, size) == 0;
}

void unlockMemory(void* ptr, size_t size) {
    if (ptr && size > 0) {
        munlock(ptr, size);
    }
}

void logSecureMessage(const std::string& message) {
    spdlog::debug("{}", message);
}

} // namespace wingman::platform
