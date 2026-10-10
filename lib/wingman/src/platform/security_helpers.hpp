// 安全子系统平台薄层：反调试/反 VM/签名校验/内存保护的底层探测原语。
// 平台分支由 CMake 按目录选源收敛（platform/win/ vs platform/posix/），
// 本头文件与调用点保持零平台宏（薄层纪律，见
// docs/platform-abstraction-design.md §8）。
#pragma once

#include "wingman/security.hpp"

#include <cstddef>
#include <string>

namespace wingman::platform {

// 模拟人类操作节奏的短暂暂停（Windows Sleep / POSIX std::this_thread）
void sleepMilliseconds(int milliseconds);

// 进程保护（Windows 提权 SE_DEBUG_NAME；POSIX 无对应物，恒 false）
bool enableProcessProtection();

// 反调试：PEB BeingDebugged / IsDebuggerPresent+调试器窗口 / Dr0-Dr7 调试寄存器
bool checkDebuggerPEB();
bool checkDebuggerFlags();
bool checkHardwareBreakpoints();

// 反 VM：注册表指纹 / VM 进程名 / VM 设备驱动 / CPUID 厂商串
bool checkVMRegistry();
bool checkVMProcesses();
bool checkVMDrivers();
bool checkVMCPUID();

// 代码签名校验（Windows WinTrust/CryptoAPI；POSIX 无对应物，恒 false/默认未签名）
bool verifySignature();
CodeSignature getSignatureInfo();

// 内存保护（Windows VirtualProtect/VirtualLock / POSIX mprotect/mlock）
bool protectMemory(void* addr, size_t size, bool protect);

// 安全清零（Windows SecureZeroMemory / POSIX volatile 逐字节写，防优化器略过）
void secureZero(void* ptr, size_t size);
bool lockMemory(void* ptr, size_t size);
void unlockMemory(void* ptr, size_t size);

// 脱敏日志输出（Windows OutputDebugStringA / POSIX spdlog debug）
void logSecureMessage(const std::string& message);

} // namespace wingman::platform
