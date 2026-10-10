#include "wingman/security.hpp"
#include "wingman/crypt.hpp"
#include "platform/security_helpers.hpp"

#include <random>
#include <spdlog/spdlog.h>

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
    platform::sleepMilliseconds(delay);
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

    return platform::enableProcessProtection();
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
    return platform::checkDebuggerPEB();
}

bool SecurityManager::checkDebuggerFlags() {
    return platform::checkDebuggerFlags();
}

bool SecurityManager::checkHardwareBreakpoints() {
    return platform::checkHardwareBreakpoints();
}

bool SecurityManager::isRunningInVM() {
    if (!m_processProtection.enableAntiVM) {
        return false;
    }

    return checkVMRegistry() || checkVMProcesses() || checkVMDrivers() || checkVMCPUID();
}

bool SecurityManager::checkVMRegistry() {
    return platform::checkVMRegistry();
}

bool SecurityManager::checkVMProcesses() {
    return platform::checkVMProcesses();
}

bool SecurityManager::checkVMDrivers() {
    return platform::checkVMDrivers();
}

bool SecurityManager::checkVMCPUID() {
    return platform::checkVMCPUID();
}

bool SecurityManager::verifyIntegrity() {
    if (!m_processProtection.enableIntegrityCheck) {
        return true;
    }

    spdlog::warn("verifyIntegrity: integrity checking is not yet implemented "
                 "(no baseline hash configured). Returning false.");
    return false;
}

// ========== Code Signing ==========

bool SecurityManager::verifySignature() {
    return platform::verifySignature();
}

CodeSignature SecurityManager::getSignatureInfo() {
    return platform::getSignatureInfo();
}

bool SecurityManager::selfSign(const std::string& certPath, const std::string& keyPath) {
    // Self-signing is only for development environment
    // Production should use proper certificates
    (void)certPath;
    (void)keyPath;
    return false;
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
    // 统一走 wingman::crypt 的 OpenSSL 实现（原手写 SHA-256 已删除）
    return crypt::sha256(input);
}

// ========== Memory Protection ==========

bool SecurityManager::protectMemory(void* addr, size_t size, bool protect) {
    return platform::protectMemory(addr, size, protect);
}

void SecurityManager::secureZero(void* ptr, size_t size) {
    platform::secureZero(ptr, size);
}

bool SecurityManager::lockMemory(void* ptr, size_t size) {
    return platform::lockMemory(ptr, size);
}

void SecurityManager::unlockMemory(void* ptr, size_t size) {
    platform::unlockMemory(ptr, size);
}

// ========== Log Security ==========

void SecurityManager::secureLog(const std::string& message) {
    // Log after filtering sensitive information
    std::string filtered = filterSensitive(message);
    platform::logSecureMessage(filtered);
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
