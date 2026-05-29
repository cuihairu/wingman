#pragma once

#include <string>
#include <random>
#include <chrono>

namespace wingman {

// Anti-detection configuration
struct AntiDetectionConfig {
    bool enableRandomDelay = true;
    bool enableBezierMovement = true;
    bool enableRandomClick = true;
    int minDelayMs = 50;
    int maxDelayMs = 150;
    double clickJitter = 2.0; // Pixel jitter range
    double movementVariance = 0.1; // Movement speed variation
};

// Process protection configuration
struct ProcessProtectionConfig {
    bool protectFromTermination = false;
    bool hideFromTaskManager = false;
    bool enableAntiDebug = false;
    bool enableAntiVM = false;
    bool enableIntegrityCheck = false;
};

// Code signature information
struct CodeSignature {
    bool isSigned = false;
    std::string issuer;
    std::string subject;
    std::string thumbprint;
    std::chrono::system_clock::time_point validFrom;
    std::chrono::system_clock::time_point validTo;
};

// Obfuscation configuration
struct ObfuscationConfig {
    bool enableStringEncryption = false;
    bool enableControlFlowFlattening = false;
    bool enableDeadCodeInjection = false;
    bool enableVirtualization = false;
};

class SecurityManager {
public:
    static SecurityManager& instance() {
        static SecurityManager inst;
        return inst;
    }

    // ========== Anti-detection ==========

    // Set anti-detection configuration
    void setAntiDetectionConfig(const AntiDetectionConfig& config);
    const AntiDetectionConfig& getAntiDetectionConfig() const;

    // Get random delay (milliseconds)
    int getRandomDelay() const;

    // Get random offset (pixels)
    std::pair<double, double> getRandomOffset() const;

    // Get random click jitter
    std::pair<double, double> getClickJitter() const;

    // Simulate human behavior pattern
    void simulateHumanBehavior();

    // ========== Process protection ==========

    // Set process protection configuration
    void setProcessProtectionConfig(const ProcessProtectionConfig& config);
    const ProcessProtectionConfig& getProcessProtectionConfig() const;

    // Enable process protection
    bool enableProcessProtection();

    // Disable process protection
    void disableProcessProtection();

    // Check for debugger
    bool isDebuggerPresent();

    // Check for virtual machine
    bool isRunningInVM();

    // Integrity check
    bool verifyIntegrity();

    // ========== Code signature ==========

    // Verify code signature
    bool verifySignature();

    // Get signature information
    CodeSignature getSignatureInfo();

    // Self-sign (for development)
    bool selfSign(const std::string& certPath, const std::string& keyPath);

    // ========== Obfuscation ==========

    // Encrypt string
    static std::string encryptString(const std::string& input, const std::string& key);
    static std::string decryptString(const std::string& input, const std::string& key);

    // Generate random string
    static std::string generateRandomString(size_t length);

    // Hash function
    static std::string hashString(const std::string& input);

    // ========== Memory protection ==========

    // Protect memory region
    bool protectMemory(void* addr, size_t size, bool protect = true);

    // Clear sensitive data
    void secureZero(void* ptr, size_t size);

    // Lock memory (prevent swapping to disk)
    bool lockMemory(void* ptr, size_t size);

    // Unlock memory
    void unlockMemory(void* ptr, size_t size);

    // ========== Log security ==========

    // Secure log (does not log sensitive information)
    void secureLog(const std::string& message);

    // Filter sensitive information
    static std::string filterSensitive(const std::string& input);

private:
    SecurityManager();
    ~SecurityManager();

    AntiDetectionConfig m_antiDetection;
    ProcessProtectionConfig m_processProtection;
    mutable std::mt19937 m_randomEngine;

    // Initialize random engine
    void initRandomEngine();

    // Anti-debug checks
    bool checkDebuggerPEB();
    bool checkDebuggerFlags();
    bool checkHardwareBreakpoints();

    // Anti-VM checks
    bool checkVMRegistry();
    bool checkVMProcesses();
    bool checkVMDrivers();
    bool checkVMCPUID();
};

} // namespace wingman
