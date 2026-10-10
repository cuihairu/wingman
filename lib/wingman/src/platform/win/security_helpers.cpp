// Windows 实现：反调试/反 VM/签名校验/内存保护的 Win32 底层原语
#include "platform/security_helpers.hpp"

#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <wintrust.h>
#include <tlhelp32.h>
#include <psapi.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "psapi.lib")

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

// Define WINTRUST_ACTION_GENERIC_VERIFY_V2 if not available
#ifndef WINTRUST_ACTION_GENERIC_VERIFY_V2
static const GUID WINTRUST_ACTION_GENERIC_VERIFY_V2 =
{ 0xaac56b, 0xcd44, 0x11d0, { 0x8c, 0xc2, 0x00, 0xc0, 0x4f, 0xc2, 0x95, 0xee } };
#endif

namespace wingman::platform {

void sleepMilliseconds(int milliseconds) {
    Sleep(milliseconds);
}

bool enableProcessProtection() {
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

bool checkDebuggerPEB() {
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

bool checkDebuggerFlags() {
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

bool checkHardwareBreakpoints() {
#if !defined(_M_X64) && !defined(_M_IX86)
    // 调试寄存器 Dr0-Dr7 是 x86/x64 专属，ARM64 的 _CONTEXT 无此字段
    return false;
#else
    CONTEXT ctx = {0};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    if (!GetThreadContext(GetCurrentThread(), &ctx)) {
        return false;
    }

    // Check debug registers
    return (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0 ||
            (ctx.Dr7 & 0xFF) != 0);
#endif
}

bool checkVMRegistry() {
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

bool checkVMProcesses() {
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
        std::transform(processName, processName + strlen(processName), processName,
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

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

bool checkVMDrivers() {
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

bool checkVMCPUID() {
#if !defined(_M_X64) && !defined(_M_IX86)
    // __cpuid 内建函数 x86/x64 专属；ARM64 由注册表/进程/驱动检测兜底
    return false;
#else
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
#endif
}

bool verifySignature() {
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

CodeSignature getSignatureInfo() {
    CodeSignature info;

    // Get current executable path
    WCHAR modulePath[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return info;

    // Query signature from file
    DWORD encoding = 0, contentType = 0, formatType = 0;
    HCERTSTORE hStore = nullptr;
    HCRYPTMSG hMsg = nullptr;
    PCCERT_CONTEXT pSignerCert = nullptr;

    BOOL ok = CryptQueryObject(CERT_QUERY_OBJECT_FILE,
        modulePath,
        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
        CERT_QUERY_FORMAT_FLAG_BINARY,
        0, &encoding, &contentType, &formatType, &hStore, &hMsg, nullptr);

    if (!ok) return info;

    info.isSigned = true;

    // Get signer certificate
    DWORD signerInfoSize = 0;
    CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &signerInfoSize);
    if (signerInfoSize == 0) goto cleanup;

    {
        auto* signerInfo = static_cast<PCMSG_SIGNER_INFO>(malloc(signerInfoSize));
        if (!signerInfo) goto cleanup;

        if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, signerInfo, &signerInfoSize)) {
            free(signerInfo);
            goto cleanup;
        }

        // Find signer certificate in the store
        CERT_INFO certInfo = {};
        certInfo.Issuer = signerInfo->Issuer;
        certInfo.SerialNumber = signerInfo->SerialNumber;

        pSignerCert = CertFindCertificateInStore(hStore,
            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
            0, CERT_FIND_SUBJECT_CERT, &certInfo, nullptr);

        free(signerInfo);
    }

    if (pSignerCert) {
        // Issuer
        WCHAR issuerName[256] = {};
        CertGetNameStringW(pSignerCert, CERT_NAME_SIMPLE_DISPLAY_TYPE,
            CERT_NAME_ISSUER_FLAG, nullptr, issuerName, 256);
        {
            int nameLen = WideCharToMultiByte(CP_UTF8, 0, issuerName, -1, nullptr, 0, nullptr, nullptr);
            if (nameLen > 0) {
                info.issuer.resize(nameLen - 1);
                WideCharToMultiByte(CP_UTF8, 0, issuerName, -1, info.issuer.data(), nameLen, nullptr, nullptr);
            }
        }

        // Subject
        WCHAR subjectName[256] = {};
        CertGetNameStringW(pSignerCert, CERT_NAME_SIMPLE_DISPLAY_TYPE,
            0, nullptr, subjectName, 256);
        {
            int nameLen = WideCharToMultiByte(CP_UTF8, 0, subjectName, -1, nullptr, 0, nullptr, nullptr);
            if (nameLen > 0) {
                info.subject.resize(nameLen - 1);
                WideCharToMultiByte(CP_UTF8, 0, subjectName, -1, info.subject.data(), nameLen, nullptr, nullptr);
            }
        }

        // Thumbprint (SHA1 hash)
        DWORD thumbprintSize = 20;
        std::vector<BYTE> thumbprint(thumbprintSize);
        if (CertGetCertificateContextProperty(pSignerCert, CERT_SHA1_HASH_PROP_ID,
            thumbprint.data(), &thumbprintSize)) {
            std::ostringstream oss;
            for (DWORD i = 0; i < thumbprintSize; ++i) {
                oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
                    << static_cast<int>(thumbprint[i]);
            }
            info.thumbprint = oss.str();
        }

        // Validity period
        if (pSignerCert->pCertInfo) {
            FILETIME ft = pSignerCert->pCertInfo->NotBefore;
            SYSTEMTIME st = {};
            FileTimeToSystemTime(&ft, &st);
            struct tm tm = {};
            tm.tm_year = st.wYear - 1900;
            tm.tm_mon = st.wMonth - 1;
            tm.tm_mday = st.wDay;
            tm.tm_hour = st.wHour;
            tm.tm_min = st.wMinute;
            tm.tm_sec = st.wSecond;
            info.validFrom = std::chrono::system_clock::from_time_t(mktime(&tm));

            ft = pSignerCert->pCertInfo->NotAfter;
            FileTimeToSystemTime(&ft, &st);
            tm = {};
            tm.tm_year = st.wYear - 1900;
            tm.tm_mon = st.wMonth - 1;
            tm.tm_mday = st.wDay;
            tm.tm_hour = st.wHour;
            tm.tm_min = st.wMinute;
            tm.tm_sec = st.wSecond;
            info.validTo = std::chrono::system_clock::from_time_t(mktime(&tm));
        }
    }

cleanup:
    if (pSignerCert) CertFreeCertificateContext(pSignerCert);
    if (hStore) CertCloseStore(hStore, 0);
    if (hMsg) CryptMsgClose(hMsg);

    return info;
}

bool protectMemory(void* addr, size_t size, bool protect) {
    DWORD oldProtect;
    DWORD newProtect = protect ? PAGE_READONLY : PAGE_READWRITE;

    return VirtualProtect(addr, size, newProtect, &oldProtect) != 0;
}

void secureZero(void* ptr, size_t size) {
    SecureZeroMemory(ptr, size);
}

bool lockMemory(void* ptr, size_t size) {
    return VirtualLock(ptr, size) != 0;
}

void unlockMemory(void* ptr, size_t size) {
    VirtualUnlock(ptr, size);
}

void logSecureMessage(const std::string& message) {
    OutputDebugStringA(message.c_str());
}

} // namespace wingman::platform
