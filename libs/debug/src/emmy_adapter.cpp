#include "wingman/debug/emmy_adapter.hpp"
#include <spdlog/spdlog.h>
#include <mutex>
#include <filesystem>
#include <lua.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#define MAX_PATH 1024
#endif

// EmmyLua 调试器 C API 接口
// 这是一个简化的适配器实现，实际的 EmmyLua 需要动态加载 emmy_core.dll

extern "C" {
    // EmmyLua API 函数指针类型
    typedef int (*EmmyLuaApi_Init)(void);
    typedef void (*EmmyLuaApi_Shutdown)(void);
    typedef int (*EmmyLuaApi_StartTcp)(int port);
    typedef void (*EmmyLuaApi_Stop)(void);
    typedef int (*EmmyLuaApi_ConnectIde)(const char* host, int port);
    typedef void (*EmmyLuaApi_WaitIde)(void);
    typedef void (*EmmyLuaApi_BreakHere)(void);
}

namespace wingman::debug {

// ========== EmmyAdapter 实现 ==========

class EmmyAdapter::Impl {
public:
    // EmmyLua 核心库句柄
    void* emmyCoreHandle = nullptr;

    // EmmyLua API 函数指针
    EmmyLuaApi_Init apiInit = nullptr;
    EmmyLuaApi_Shutdown apiShutdown = nullptr;
    EmmyLuaApi_StartTcp apiStartTcp = nullptr;
    EmmyLuaApi_Stop apiStop = nullptr;
    EmmyLuaApi_ConnectIde apiConnectIde = nullptr;
    EmmyLuaApi_WaitIde apiWaitIde = nullptr;
    EmmyLuaApi_BreakHere apiBreakHere = nullptr;

    // EmmyLua 加载路径
    std::string loadPath;

    // 监听端口
    int listenPort = 9966;

    // 是否已连接 IDE
    bool ideConnected = false;
};

// ========== 全局单例 ==========

static std::unique_ptr<EmmyAdapter> g_instance;
static std::mutex g_instanceMutex;

EmmyAdapter::EmmyAdapter()
    : impl_(std::make_unique<Impl>()) {
}

EmmyAdapter::~EmmyAdapter() {
    stop();
    disconnect();
}

bool EmmyAdapter::initialize(const std::string& loadPath) {
    if (initialized_) {
        spdlog::warn("EmmyAdapter already initialized");
        return true;
    }

    spdlog::info("Initializing EmmyAdapter");

    // 确定加载路径
    impl_->loadPath = loadPath;
    if (impl_->loadPath.empty()) {
        // 默认路径：可执行文件目录下的 emmy 文件夹
        #ifdef _WIN32
        char exePath[MAX_PATH];
        if (GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
            std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
            impl_->loadPath = (exeDir / "emmy").string();
        }
        #else
        char exePath[MAX_PATH];
        ssize_t count = readlink("/proc/self/exe", exePath, MAX_PATH);
        if (count != -1) {
            exePath[count] = '\0';
            std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
            impl_->loadPath = (exeDir / "emmy").string();
        }
        #endif
    }

    // 检查 emmy_core.dll 是否存在
    #ifdef _WIN32
    std::filesystem::path dllPath = std::filesystem::path(impl_->loadPath) / "emmy_core.dll";
    #else
    std::filesystem::path dllPath = std::filesystem::path(impl_->loadPath) / "emmy_core.so";
    #endif
    if (!std::filesystem::exists(dllPath)) {
        spdlog::warn("emmy_core.dll not found at: {}", dllPath.string());
        spdlog::warn("EmmyLua debugger will be disabled");
        spdlog::info("Download EmmyLua from: https://github.com/EmmyLua/VSCode-Plugin");
        return false;
    }

    // 加载 EmmyLua 核心库
    #ifdef _WIN32
    impl_->emmyCoreHandle = LoadLibraryA(dllPath.string().c_str());
    #else
    impl_->emmyCoreHandle = dlopen(dllPath.string().c_str(), RTLD_LAZY);
    #endif

    if (!impl_->emmyCoreHandle) {
        spdlog::error("Failed to load emmy_core.dll");
        return false;
    }

    // 获取 API 函数
    #ifdef _WIN32
    impl_->apiInit = (EmmyLuaApi_Init)GetProcAddress((HMODULE)impl_->emmyCoreHandle, "luaopen_emmy_core");
    impl_->apiShutdown = (EmmyLuaApi_Shutdown)GetProcAddress((HMODULE)impl_->emmyCoreHandle, "emmy_shutdown");
    impl_->apiStartTcp = (EmmyLuaApi_StartTcp)GetProcAddress((HMODULE)impl_->emmyCoreHandle, "emmy_start_tcp");
    impl_->apiStop = (EmmyLuaApi_Stop)GetProcAddress((HMODULE)impl_->emmyCoreHandle, "emmy_stop");
    impl_->apiConnectIde = (EmmyLuaApi_ConnectIde)GetProcAddress((HMODULE)impl_->emmyCoreHandle, "emmy_connect_ide");
    impl_->apiWaitIde = (EmmyLuaApi_WaitIde)GetProcAddress((HMODULE)impl_->emmyCoreHandle, "emmy_wait_ide");
    impl_->apiBreakHere = (EmmyLuaApi_BreakHere)GetProcAddress((HMODULE)impl_->emmyCoreHandle, "emmy_break_here");
    #else
    impl_->apiInit = (EmmyLuaApi_Init)dlsym(impl_->emmyCoreHandle, "luaopen_emmy_core");
    impl_->apiShutdown = (EmmyLuaApi_Shutdown)dlsym(impl_->emmyCoreHandle, "emmy_shutdown");
    impl_->apiStartTcp = (EmmyLuaApi_StartTcp)dlsym(impl_->emmyCoreHandle, "emmy_start_tcp");
    impl_->apiStop = (EmmyLuaApi_Stop)dlsym(impl_->emmyCoreHandle, "emmy_stop");
    impl_->apiConnectIde = (EmmyLuaApi_ConnectIde)dlsym(impl_->emmyCoreHandle, "emmy_connect_ide");
    impl_->apiWaitIde = (EmmyLuaApi_WaitIde)dlsym(impl_->emmyCoreHandle, "emmy_wait_ide");
    impl_->apiBreakHere = (EmmyLuaApi_BreakHere)dlsym(impl_->emmyCoreHandle, "emmy_break_here");
    #endif

    if (!impl_->apiInit) {
        spdlog::error("Failed to get EmmyLua API functions");
        return false;
    }

    // 初始化 EmmyLua
    if (impl_->apiInit() != 0) {
        spdlog::error("Failed to initialize EmmyLua");
        return false;
    }

    initialized_ = true;
    state_ = DebuggerState::Stopped;

    spdlog::info("EmmyAdapter initialized successfully");
    return true;
}

bool EmmyAdapter::startListen(int port) {
    if (!initialized_) {
        spdlog::error("EmmyAdapter not initialized");
        return false;
    }

    if (state_ == DebuggerState::Running) {
        spdlog::warn("Debugger already running");
        return true;
    }

    spdlog::info("Starting EmmyLua debugger on port {}", port);
    impl_->listenPort = port;

    if (!impl_->apiStartTcp) {
        spdlog::error("EmmyLua API not available");
        return false;
    }

    if (impl_->apiStartTcp(port) != 0) {
        spdlog::error("Failed to start debugger");
        return false;
    }

    state_ = DebuggerState::Waiting;
    spdlog::info("Debugger listening on port {}", port);
    return true;
}

bool EmmyAdapter::connectToIDE(const std::string& host, int port) {
    if (!initialized_) {
        spdlog::error("EmmyAdapter not initialized");
        return false;
    }

    spdlog::info("Connecting to IDE at {}:{}", host, port);

    if (!impl_->apiConnectIde) {
        spdlog::error("EmmyLua API not available");
        return false;
    }

    if (impl_->apiConnectIde(host.c_str(), port) != 0) {
        spdlog::error("Failed to connect to IDE");
        return false;
    }

    connected_ = true;
    state_ = DebuggerState::Running;
    spdlog::info("Connected to IDE");
    return true;
}

void EmmyAdapter::waitForIDE() {
    if (!initialized_) {
        spdlog::error("EmmyAdapter not initialized");
        return;
    }

    spdlog::info("Waiting for IDE connection...");

    if (!impl_->apiWaitIde) {
        spdlog::error("EmmyLua API not available");
        return;
    }

    impl_->apiWaitIde();
    connected_ = true;
    state_ = DebuggerState::Running;
    spdlog::info("IDE connected");
}

void EmmyAdapter::disconnect() {
    if (!connected_) {
        return;
    }

    spdlog::info("Disconnecting from IDE");
    connected_ = false;
    state_ = DebuggerState::Waiting;
}

void EmmyAdapter::stop() {
    if (!initialized_) {
        return;
    }

    spdlog::info("Stopping EmmyLua debugger");

    if (impl_->apiStop) {
        impl_->apiStop();
    }

    state_ = DebuggerState::Stopped;
    connected_ = false;
}

bool EmmyAdapter::setBreakpoint(const std::string& file, int line) {
    if (!initialized_ || !connected_) {
        spdlog::warn("Cannot set breakpoint: debugger not ready");
        return false;
    }

    spdlog::info("Setting breakpoint: {}:{}: {}", file, line);

    // EmmyLua 的断点设置需要在 Lua 中完成
    // 这里只是记录，实际断点由 Lua API 设置
    return true;
}

bool EmmyAdapter::removeBreakpoint(const std::string& file, int line) {
    if (!initialized_ || !connected_) {
        return false;
    }

    spdlog::info("Removing breakpoint: {}:{}", file, line);
    return true;
}

bool EmmyAdapter::setupLuaState(lua_State* L) {
    if (!initialized_) {
        spdlog::warn("EmmyAdapter not initialized");
        return false;
    }

    // EmmyLua 需要在 Lua 状态中注册其模块
    // 这通常通过调用 luaopen_emmy_core 完成
    if (impl_->apiInit) {
        impl_->apiInit();
        return true;
    }

    return false;
}

// ========== 全局函数 ==========

bool startDebugger(int port) {
    std::lock_guard lock(g_instanceMutex);

    if (!g_instance) {
        g_instance = std::make_unique<EmmyAdapter>();
    }

    if (!g_instance->initialize()) {
        return false;
    }

    return g_instance->startListen(port);
}

void stopDebugger() {
    std::lock_guard lock(g_instanceMutex);

    if (g_instance) {
        g_instance->stop();
        g_instance->disconnect();
    }
}

EmmyAdapter& getDebugger() {
    std::lock_guard lock(g_instanceMutex);

    if (!g_instance) {
        g_instance = std::make_unique<EmmyAdapter>();
    }

    return *g_instance;
}

} // namespace wingman::debug
