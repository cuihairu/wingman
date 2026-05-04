#pragma once

#include <string>
#include <random>
#include <chrono>

namespace wingman {

// 防检测配置
struct AntiDetectionConfig {
    bool enableRandomDelay = true;
    bool enableBezierMovement = true;
    bool enableRandomClick = true;
    int minDelayMs = 50;
    int maxDelayMs = 150;
    double clickJitter = 2.0; // 像素抖动范围
    double movementVariance = 0.1; // 移动速度变化
};

// 进程保护配置
struct ProcessProtectionConfig {
    bool protectFromTermination = false;
    bool hideFromTaskManager = false;
    bool enableAntiDebug = false;
    bool enableAntiVM = false;
    bool enableIntegrityCheck = false;
};

// 代码签名信息
struct CodeSignature {
    bool isSigned = false;
    std::string issuer;
    std::string subject;
    std::string thumbprint;
    std::chrono::system_clock::time_point validFrom;
    std::chrono::system_clock::time_point validTo;
};

// 混淆配置
struct ObfuscationConfig {
    bool enableStringEncryption = false;
    bool enableControlFlowFlattening = false;
    bool enableDeadCodeInjection = false;
    bool enableVirtualization = false;
};

class SecurityManager {
public:
    static SecurityManager& instance();

    // ========== 防检测 ==========

    // 设置防检测配置
    void setAntiDetectionConfig(const AntiDetectionConfig& config);
    const AntiDetectionConfig& getAntiDetectionConfig() const;

    // 获取随机延迟 (毫秒)
    int getRandomDelay() const;

    // 获取随机偏移 (像素)
    std::pair<double, double> getRandomOffset() const;

    // 获取随机点击偏移
    std::pair<double, double> getClickJitter() const;

    // 模拟人类行为模式
    void simulateHumanBehavior();

    // ========== 进程保护 ==========

    // 设置进程保护配置
    void setProcessProtectionConfig(const ProcessProtectionConfig& config);
    const ProcessProtectionConfig& getProcessProtectionConfig() const;

    // 启用进程保护
    bool enableProcessProtection();

    // 禁用进程保护
    void disableProcessProtection();

    // 检查调试器
    bool isDebuggerPresent();

    // 检查虚拟机
    bool isRunningInVM();

    // 完整性检查
    bool verifyIntegrity();

    // ========== 代码签名 ==========

    // 验证代码签名
    bool verifySignature();

    // 获取签名信息
    CodeSignature getSignatureInfo();

    // 自签名 (开发用)
    bool selfSign(const std::string& certPath, const std::string& keyPath);

    // ========== 混淆 ==========

    // 加密字符串
    static std::string encryptString(const std::string& input, const std::string& key);
    static std::string decryptString(const std::string& input, const std::string& key);

    // 生成随机字符串
    static std::string generateRandomString(size_t length);

    // 哈希函数
    static std::string hashString(const std::string& input);

    // ========== 内存保护 ==========

    // 保护内存区域
    bool protectMemory(void* addr, size_t size, bool protect = true);

    // 清除敏感数据
    void secureZero(void* ptr, size_t size);

    // 内存锁定 (防止交换到磁盘)
    bool lockMemory(void* ptr, size_t size);

    // 解锁内存
    void unlockMemory(void* ptr, size_t size);

    // ========== 日志安全 ==========

    // 安全日志 (不记录敏感信息)
    void secureLog(const std::string& message);

    // 过滤敏感信息
    static std::string filterSensitive(const std::string& input);

private:
    SecurityManager();
    ~SecurityManager();

    AntiDetectionConfig m_antiDetection;
    ProcessProtectionConfig m_processProtection;
    std::mt19937 m_randomEngine;

    // 初始化随机引擎
    void initRandomEngine();

    // 反调试检查
    bool checkDebuggerPEB();
    bool checkDebuggerFlags();
    bool checkHardwareBreakpoints();

    // 反虚拟机检查
    bool checkVMRegistry();
    bool checkVMProcesses();
    bool checkVMDrivers();
    bool checkVMCPUID();
};

} // namespace wingman
