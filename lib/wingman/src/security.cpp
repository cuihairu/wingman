#include "wingman/security.hpp"

#include <Windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <wintrust.h>
#include <tlhelp32.h>
#include <psapi.h>
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "psapi.lib")
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "bcrypt.lib")

// Define WINTRUST_ACTION_GENERIC_VERIFY_V2 if not available
#ifndef WINTRUST_ACTION_GENERIC_VERIFY_V2
static const GUID WINTRUST_ACTION_GENERIC_VERIFY_V2 =
{ 0xaac56b, 0xcd44, 0x11d0, { 0x8c, 0xc2, 0x00, 0xc0, 0x4f, 0xc2, 0x95, 0xee } };
#endif

namespace wingman {

// ========== SecurityManager Implementation ==========

SecurityManager::SecurityManager() {
    initRandomEngine();
}

SecurityManager::~SecurityManager() = default;

void SecurityManager::initRandomEngine() {
    std::random_device rd;
    m_randomEngine.seed(rd());
}

// ========== Anti-Detection ==========

void SecurityManager::setAntiDetectionConfig(const AntiDetectionConfig& config) {
    m_antiDetection = config;
}

const AntiDetectionConfig& SecurityManager::getAntiDetectionConfig() const {
    return m_antiDetection;
}

int SecurityManager::getRandomDelay() const {
    if (!m_antiDetection.enableRandomDelay) {
        return (m_antiDetection.minDelayMs + m_antiDetection.maxDelayMs) / 2;
    }

    std::uniform_int_distribution<int> dist(m_antiDetection.minDelayMs, m_antiDetection.maxDelayMs);
    return dist(m_randomEngine);
}

std::pair<double, double> SecurityManager::getRandomOffset() const {
    std::uniform_real_distribution<double> dist(-m_antiDetection.clickJitter, m_antiDetection.clickJitter);
    return {dist(m_randomEngine), dist(m_randomEngine)};
}

std::pair<double, double> SecurityManager::getClickJitter() const {
    return getRandomOffset();
}

void SecurityManager::simulateHumanBehavior() {
    // Simulate tiny random pauses and action variations like a human
    int delay = getRandomDelay();
    Sleep(delay);
}

// ========== Process Protection ==========

void SecurityManager::setProcessProtectionConfig(const ProcessProtectionConfig& config) {
    m_processProtection = config;
}

const ProcessProtectionConfig& SecurityManager::getProcessProtectionConfig() const {
    return m_processProtection;
}

bool SecurityManager::enableProcessProtection() {
    if (!m_processProtection.protectFromTermination) {
        return true;
    }

    // Windows process protection requires administrator privileges
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
        return false;
    }

    LUID luid;
    if (!LookupPrivilegeValueA(nullptr, SE_DEBUG_NAME, &luid)) {
        CloseHandle(hToken);
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    bool result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr) &&
                  GetLastError() != ERROR_NOT_ALL_ASSIGNED;

    CloseHandle(hToken);
    return result;
}

void SecurityManager::disableProcessProtection() {
    // Disabling process protection typically does not require special operations
}

bool SecurityManager::isDebuggerPresent() {
    if (!m_processProtection.enableAntiDebug) {
        return false;
    }

    return checkDebuggerPEB() || checkDebuggerFlags() || checkHardwareBreakpoints();
}

bool SecurityManager::checkDebuggerPEB() {
    // Check the BeingDebugged flag in PEB
    typedef struct _PEB {
        BYTE Reserved1[2];
        BYTE BeingDebugged;
        BYTE Reserved2[1];
        PVOID Reserved3[2];
    } PEB, *PPEB;

    typedef struct _PROCESS_BASIC_INFORMATION {
        PVOID Reserved1;
        PPEB PebBaseAddress;
        PVOID Reserved2[2];
        ULONG_PTR UniqueProcessId;
        PVOID Reserved3;
    } PROCESS_BASIC_INFORMATION;

    // Check using NtQueryInformationProcess
    typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(
        HANDLE, ULONG, PVOID, ULONG, PULONG);

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return false;

    auto NtQueryInformationProcess = (pNtQueryInformationProcess)GetProcAddress(
        hNtdll, "NtQueryInformationProcess");

    if (!NtQueryInformationProcess) return false;

    PROCESS_BASIC_INFORMATION pbi;
    NTSTATUS status = NtQueryInformationProcess(
        GetCurrentProcess(),
        0, // ProcessBasicInformation
        &pbi,
        sizeof(pbi),
        nullptr);

    if (status != 0) return false;

    return pbi.PebBaseAddress && pbi.PebBaseAddress->BeingDebugged;
}

bool SecurityManager::checkDebuggerFlags() {
    // Check other debugger flags
    if (IsDebuggerPresent()) return true;

    // Check for debugger windows
    HWND hWnd = FindWindowA(nullptr, "WinDbgFrameClass");
    if (hWnd) return true;

    hWnd = FindWindowA("OLLYDBG", nullptr);
    if (hWnd) return true;

    hWnd = FindWindowA("ID", nullptr);
    if (hWnd) return true;

    return false;
}

bool SecurityManager::checkHardwareBreakpoints() {
    CONTEXT ctx = {0};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    if (!GetThreadContext(GetCurrentThread(), &ctx)) {
        return false;
    }

    // Check debug registers
    return (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0 ||
            (ctx.Dr7 & 0xFF) != 0);
}

bool SecurityManager::isRunningInVM() {
    if (!m_processProtection.enableAntiVM) {
        return false;
    }

    return checkVMRegistry() || checkVMProcesses() || checkVMDrivers() || checkVMCPUID();
}

bool SecurityManager::checkVMRegistry() {
    // Check VM-related registry keys
    const char* vmKeys[] = {
        "HARDWARE\\DEVICEMAP\\Scsi\\Scsi Port 0\\Scsi Bus 0\\Target Id 0\\Logical Unit Id 0",
        "HARDWARE\\ACPI\\DSDT\\VBOX__",
        "HARDWARE\\ACPI\\FADT\\VBOX__",
        "SYSTEM\\CurrentControlSet\\Services\\VBoxGuest",
        "SYSTEM\\CurrentControlSet\\Services\\VBoxMouse",
        nullptr
    };

    HKEY hKey;
    for (int i = 0; vmKeys[i]; ++i) {
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, vmKeys[i], 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }
    }

    return false;
}

bool SecurityManager::checkVMProcesses() {
    // Check VM-related processes
    const char* vmProcesses[] = {
        "vmwareservice.exe",
        "vmwareuser.exe",
        "vmwaretray.exe",
        "vboxservice.exe",
        "vboxtray.exe",
        "virtualbox.exe",
        "qemu-ga.exe",
        nullptr
    };

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return false;
    }

    bool found = false;
    do {
        char processName[MAX_PATH];
        strcpy_s(processName, pe32.szExeFile);
        std::transform(processName, processName + strlen(processName), processName, ::tolower);

        for (int i = 0; vmProcesses[i]; ++i) {
            if (strstr(processName, vmProcesses[i])) {
                found = true;
                break;
            }
        }
    } while (!found && Process32Next(hSnapshot, &pe32));

    CloseHandle(hSnapshot);
    return found;
}

bool SecurityManager::checkVMDrivers() {
    // Check VM-related drivers
    const char* vmDrivers[] = {
        "\\\\.\\VBoxMiniRdrDN",
        "\\\\.\\VBoxGuest",
        "\\\\.\\VBoxMouse",
        "\\\\.\\VBoxVideo",
        nullptr
    };

    for (int i = 0; vmDrivers[i]; ++i) {
        HANDLE hDevice = CreateFileA(vmDrivers[i], 0, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDevice != INVALID_HANDLE_VALUE) {
            CloseHandle(hDevice);
            return true;
        }
    }

    return false;
}

bool SecurityManager::checkVMCPUID() {
    // Detect virtual machine using CPUID instruction
    int regs[4] = {0};

    // Check VMWare
    __cpuid(regs, 0x40000000);
    if (regs[1] == 0x61774D56 && regs[2] == 0x4D566572 && regs[3] == 0x656C6966) { // "VMware" in reverse
        return true;
    }

    // Check VirtualBox
    if (regs[1] == 0x6F626F78 && regs[2] == 0x72615761 && regs[3] == 0x74656E69) { // "VirtualBox" parts
        return true;
    }

    return false;
}

bool SecurityManager::verifyIntegrity() {
    if (!m_processProtection.enableIntegrityCheck) {
        return true;
    }

    // Get current module handle
    HMODULE hModule = GetModuleHandleA(nullptr);
    if (!hModule) return false;

    MODULEINFO modInfo;
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo))) {
        return false;
    }

    // Calculate module hash
    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return false;
    }

    HCRYPTHASH hHash = 0;
    if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return false;
    }

    bool valid = CryptHashData(hHash, (BYTE*)hModule, modInfo.SizeOfImage, 0);

    // TODO: Compare with stored hash

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    return valid;
}

// ========== Code Signing ==========

bool SecurityManager::verifySignature() {
    // Windows verify code signature
    WINTRUST_FILE_INFO fileInfo = {0};
    fileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath = L"wingman.exe";

    WINTRUST_DATA trustData = {0};
    trustData.cbStruct = sizeof(WINTRUST_DATA);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;

    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    LONG result = WinVerifyTrust(nullptr, &policyGUID, &trustData);

    return result == ERROR_SUCCESS;
}

CodeSignature SecurityManager::getSignatureInfo() {
    CodeSignature info;

    // TODO: Implement signature info extraction

    return info;
}

bool SecurityManager::selfSign(const std::string& certPath, const std::string& keyPath) {
    // Self-signing is only for development environment
    // Production should use proper certificates
    return false;
}

// ========== Obfuscation ==========

std::string SecurityManager::encryptString(const std::string& input, const std::string& key) {
    std::string output;
    output.reserve(input.size());

    size_t keyLen = key.size();
    for (size_t i = 0; i < input.size(); ++i) {
        output += input[i] ^ key[i % keyLen];
    }

    return output;
}

std::string SecurityManager::decryptString(const std::string& input, const std::string& key) {
    return encryptString(input, key); // XOR is symmetric
}

std::string SecurityManager::generateRandomString(size_t length) {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::uniform_int_distribution<int> dist(0, sizeof(chars) - 2);

    std::string result;
    result.reserve(length);

    std::random_device rd;
    std::mt19937 gen(rd());

    for (size_t i = 0; i < length; ++i) {
        result += chars[dist(gen)];
    }

    return result;
}

std::string SecurityManager::hashString(const std::string& input) {
    // Hash using BCrypt
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status != 0) return "";

    BCRYPT_HASH_HANDLE hHash = nullptr;
    status = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    if (status != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    status = BCryptHashData(hHash, (PUCHAR)input.data(), input.size(), 0);
    if (status != 0) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    UCHAR hash[32];
    status = BCryptFinishHash(hHash, hash, sizeof(hash), 0);

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (status != 0) return "";

    // Convert to hexadecimal string
    std::stringstream ss;
    for (size_t i = 0; i < sizeof(hash); ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return ss.str();
}

// ========== Memory Protection ==========

bool SecurityManager::protectMemory(void* addr, size_t size, bool protect) {
    DWORD oldProtect;
    DWORD newProtect = protect ? PAGE_READONLY : PAGE_READWRITE;

    return VirtualProtect(addr, size, newProtect, &oldProtect) != 0;
}

void SecurityManager::secureZero(void* ptr, size_t size) {
    SecureZeroMemory(ptr, size);
}

bool SecurityManager::lockMemory(void* ptr, size_t size) {
    return VirtualLock(ptr, size) != 0;
}

void SecurityManager::unlockMemory(void* ptr, size_t size) {
    VirtualUnlock(ptr, size);
}

// ========== Log Security ==========

void SecurityManager::secureLog(const std::string& message) {
    // Log after filtering sensitive information
    std::string filtered = filterSensitive(message);
    OutputDebugStringA(filtered.c_str());
}

std::string SecurityManager::filterSensitive(const std::string& input) {
    std::string output = input;

    // Filter common sensitive information patterns
    const char* patterns[] = {
        "password",
        "passwd",
        "pwd",
        "token",
        "key",
        "secret",
        "api_key",
        "apikey",
        nullptr
    };

    for (int i = 0; patterns[i]; ++i) {
        size_t pos = 0;
        std::string pattern = patterns[i];
        while ((pos = output.find(pattern, pos)) != std::string::npos) {
            output.replace(pos, pattern.size(), "***");
            pos += 3;
        }
    }

    return output;
}

} // namespace wingman
