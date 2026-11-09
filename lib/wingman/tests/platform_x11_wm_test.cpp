#if defined(__linux__)

#include "platform_x11_test_common.hpp"

// ========== 真实 WM 集成（自起 Xvfb + openbox 子进程） ==========
//
// 上面的窗口用例在无 WM 的 Xvfb 上由测试进程直写根属性模拟 WM；本节起独立的
// Xvfb + openbox，验证必须由真实 WM 异步兑现的路径：XIconifyWindow 图标化
// （unmap + _NET_WM_STATE_HIDDEN）、_NET_WM_STATE 消息（maximize/restore 状态
// 原子翻转）、_NET_ACTIVE_WINDOW 消息（activate → 前台焦点）、WM 自维护的
// _NET_CLIENT_LIST（enumerate 无需手写属性）。Xvfb / openbox 任一缺失时优雅
// skip（运行期可选依赖，同 xclip 先例）。
// 环境完全自起自毁：专用 display 号（持 flock 串行化，防 ctest -j 并行互抢）+
// 子进程 PDEATHSIG 随测试进程陪葬，不触碰共享 DISPLAY 上的其他 X11 用例。

namespace {

bool xvfbAvailable() {
    static const bool cached = std::system("command -v Xvfb >/dev/null 2>&1") == 0;
    return cached;
}

bool openboxAvailable() {
    static const bool cached = std::system("command -v openbox >/dev/null 2>&1") == 0;
    return cached;
}

// 轮询等待异步条件成立（真实 WM 对图标化/最大化/激活都是异步兑现的）
bool pollUntil(const std::function<bool()>& predicate, int timeoutMs, int intervalMs = 50) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
    return predicate();
}

// 自起 Xvfb + openbox 的 RAII 环境。display 号探测与子进程生命周期全程持有
// 专用 flock（与窗口/剪贴板锁同一模式，锁面不同），ctest -j 并行的 WM 用例
// 进程间串行化。析构按 WM → server 顺序回收（先 TERM 给 Xvfb 清理 socket 的
// 机会，1s 未退再 SIGKILL 兜底）。
//
// 自愈重试：openbox 启动窗口内的外来连接竞态（见 trySetup 注释）高负载下
// 偶发且不可根治，静默沉降 + 低频探测已把概率压到接近零，残余失败由整体
// 重建兜底——搭建链路任一环失败都拆干净、换 display 号重来；归属校验
// （lock 文件 pid 比对）把「display 实际由残留 server 应答」在起 openbox
// 之前就掐断。
class WmEnvironment {
public:
    WmEnvironment() {
        lockFd_ = ::open("/tmp/wingman_test_x11_wm.lock", O_RDONLY | O_CREAT, 0666);
        if (lockFd_ != -1) {
            while (::flock(lockFd_, LOCK_EX) != 0 && errno == EINTR) {}
        }
        for (int attempt = 0; attempt < 2 && !ready_; ++attempt) {
            trySetup(attempt == 0 ? -1 : displayNumber_);
        }
    }
    ~WmEnvironment() {
        if (displaySet_) {
            if (oldDisplay_) {
                ::setenv("DISPLAY", oldDisplay_->c_str(), 1);
            } else {
                ::unsetenv("DISPLAY");
            }
        }
        teardownChildren();
        if (lockFd_ != -1) {
            ::flock(lockFd_, LOCK_UN);
            ::close(lockFd_);
        }
    }

    WmEnvironment(const WmEnvironment&) = delete;
    WmEnvironment& operator=(const WmEnvironment&) = delete;

    bool valid() const { return ready_; }
    const std::string& failReason() const { return failReason_; }

private:
    // 一次完整搭建：选 display → 起 Xvfb → 校验归属 → 起 openbox → canary 验证
    // WM 已真正管理窗口。任一环失败即返回（子进程由构造函数拆干净后重试）。
    // exclude 为上一轮失败的 display 号，重试时避开。
    void trySetup(int exclude) {
        teardownChildren();  // 重试路径先拆上一轮残留（首轮为 no-op）
        displayNumber_ = probeDisplayNumber(exclude);
        if (displayNumber_ < 0) {
            failReason_ = "no free X display number in 20..90";
            return;
        }
        std::snprintf(display_, sizeof(display_), ":%d", displayNumber_);

        char xvfbArg0[] = "Xvfb";
        char screenArg[] = "-screen";
        char screenNum[] = "0";
        char screenSpec[] = "1280x800x24";
        char* xvfbArgv[] = {xvfbArg0, display_, screenArg, screenNum, screenSpec, nullptr};
        xvfbPid_ = spawn(xvfbArgv, nullptr);
        if (!childAlive(xvfbPid_)) {
            failReason_ = "Xvfb failed to start";
            return;
        }
        if (!pollUntil([this] { return displayAccepts(); }, 5000)) {
            failReason_ = "Xvfb did not become ready within 5s";
            return;
        }
        // 归属校验：lock 文件由 X server 自己写 pid。若 :N 实际由残留的旧
        // server 应答（displayAccepts 连上的是它），本子进程的 Xvfb 必然绑定
        // 失败——pid 对不上则换号重来，避免 openbox 连上垂死连接
        if (!displayOwnedBy(xvfbPid_)) {
            failReason_ = "display answered by a stale X server (lock pid mismatch)";
            return;
        }

        char openboxArg0[] = "openbox";
        char* openboxArgv[] = {openboxArg0, nullptr};
        openboxPid_ = spawn(openboxArgv, display_);
        if (!childAlive(openboxPid_)) {
            failReason_ = "openbox failed to start";
            return;
        }
        // openbox 启动窗口（connect + EWMH 注册 + grab 初始化，亚秒级）对
        // server 上的外来连接/断开敏感：高负载下探测连接的断开与其连接建立
        // 竞态，openbox 要么 setup 即死（"Failed to open the display"），要么
        // 连接被 server 摘出事件分发——进程存活、_NET_SUPPORTING_WM_CHECK 已
        // 写，但 MapRequest 永不投递（root 重定向空闲、socket Recv-Q=0），
        // canary 卡 IsUnmapped 直到超时。实测 50ms 间隔连断轮询 ~40% 失败，
        // spawn 后静默 600ms 再低频探测 0/45：先沉降覆盖启动窗口，探测间隔
        // 拉到 500ms 降低残余竞态面；openbox 中途死亡也在此被识别换轮重建
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        if (!pollUntil([this] { return wmRegistered(); }, 5000, 500)) {
            failReason_ = childAlive(openboxPid_)
                ? "openbox did not register (_NET_SUPPORTING_WM_CHECK absent) within 5s"
                : "openbox died during startup";
            return;
        }

        // canary 窗口：openbox 写 _NET_SUPPORTING_WM_CHECK（EWMH 初始化）与真正
        // 进入事件循环之间还有字体/Xft 加载等尾部启动工作——期间 MapRequest
        // 无人处理，首个映射的窗口会悬着。用 canary 把这段预热吸收进环境构造，
        // 测试窗口随后的映射即时被处理，断言轮询保持紧凑。健康环境亚秒完成；
        // 8s 仍未管理说明 WM 已卡死（守死连接），交给上层换号重建
        if (!waitForWmManaging()) {
            failReason_ = "openbox did not start managing windows within 8s";
            return;
        }

        // 测试体与 wingman 后端经 DISPLAY 环境发现本环境；析构时还原
        if (const char* old = ::getenv("DISPLAY")) oldDisplay_ = old;
        ::setenv("DISPLAY", display_, 1);
        displaySet_ = true;
        std::cerr << "[wm-env] using display " << display_
                  << " (xvfb=" << xvfbPid_ << " openbox=" << openboxPid_ << ")\n";
        ready_ = true;
    }

    // 选一个空闲 display 号（socket 与 lock 文件都不存在才算空闲；lock 是 X
    // server 的互斥凭据，早于 socket 存在，只探 socket 会漏掉占号未监听的 server）
    int probeDisplayNumber(int exclude) const {
        for (int candidate = 90; candidate >= 20; --candidate) {
            if (candidate == exclude) continue;
            char socketPath[64], lockPath[32];
            std::snprintf(socketPath, sizeof(socketPath), "/tmp/.X11-unix/X%d", candidate);
            std::snprintf(lockPath, sizeof(lockPath), "/tmp/.X%d-lock", candidate);
            if (::access(socketPath, F_OK) != 0 && ::access(lockPath, F_OK) != 0) {
                return candidate;
            }
        }
        return -1;
    }

    // /tmp/.X<n>-lock 由 X server 启动时写入自身 pid（先于监听 socket）。
    // 读取并比对 pid，确认当前应答 :N 的 server 就是本子进程
    bool displayOwnedBy(pid_t expected) const {
        char lockPath[32];
        std::snprintf(lockPath, sizeof(lockPath), "/tmp/.X%d-lock", displayNumber_);
        FILE* f = ::fopen(lockPath, "r");
        if (!f) return false;
        char buf[32] = {};
        const size_t n = ::fread(buf, 1, sizeof(buf) - 1, f);
        ::fclose(f);
        return n > 0 && std::strtol(buf, nullptr, 10) == static_cast<long>(expected);
    }

    void teardownChildren() {
        terminateChild(openboxPid_);
        openboxPid_ = -1;
        terminateChild(xvfbPid_);
        xvfbPid_ = -1;
    }

    // fork + exec：子进程 PDEATHSIG 随父陪葬（防测试崩溃泄漏 Xvfb/openbox），
    // 输出重定向日志文件（WM 启动失败时可查 /tmp/wingman_wm_child.log）
    pid_t spawn(char* const argv[], const char* childDisplay) {
        pid_t pid = ::fork();
        if (pid != 0) return pid;  // 父进程；-1 由 childAlive 上抛
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (::getppid() == 1) ::_exit(127);  // prctl 前父已亡的窗口期
        if (childDisplay) ::setenv("DISPLAY", childDisplay, 1);
        const int log = ::open("/tmp/wingman_wm_child.log", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (log != -1) {
            ::dup2(log, STDOUT_FILENO);
            ::dup2(log, STDERR_FILENO);
        }
        ::execvp(argv[0], argv);
        ::_exit(127);
    }

    static bool childAlive(pid_t pid) {
        if (pid <= 0) return false;
        return ::waitpid(pid, nullptr, WNOHANG) == 0;
    }

    static void terminateChild(pid_t pid) {
        if (pid <= 0) return;
        ::kill(pid, SIGTERM);
        for (int i = 0; i < 20; ++i) {  // 最多 1s 等 TERM 兑现（Xvfb 清理 socket）
            if (::waitpid(pid, nullptr, WNOHANG) == pid) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        ::kill(pid, SIGKILL);
        ::waitpid(pid, nullptr, 0);
    }

    bool displayAccepts() const {
        Display* d = XOpenDisplay(display_);
        if (!d) return false;
        XCloseDisplay(d);
        return true;
    }

    // openbox 就绪标志：接管 SubstructureRedirect 的 WM 会在根窗口写入
    // _NET_SUPPORTING_WM_CHECK（探测用独立连接，不动测试进程环境）
    bool wmRegistered() const {
        Display* d = XOpenDisplay(display_);
        if (!d) return false;
        Atom actualType;
        int actualFormat;
        unsigned long nItems, bytesAfter;
        unsigned char* data = nullptr;
        const Atom check = XInternAtom(d, "_NET_SUPPORTING_WM_CHECK", False);
        bool registered = false;
        if (XGetWindowProperty(d, DefaultRootWindow(d), check, 0, 1, False,
                               XA_WINDOW, &actualType, &actualFormat,
                               &nItems, &bytesAfter, &data) == Success && data) {
            registered = (nItems > 0);
            XFree(data);
        }
        XCloseDisplay(d);
        return registered;
    }

    // 映射 canary 窗口并等它进入 WM 维护的 _NET_CLIENT_LIST（专用探测连接，
    // 不污染测试进程环境；canary 用后即毁）。与 WmTestWindow 相同的 ICCCM
    // 合规属性——openbox 会怠慢缺 WM_HINTS/WM_NORMAL_HINTS 的裸窗口
    bool waitForWmManaging() {
        Display* d = XOpenDisplay(display_);
        if (!d) return false;
        const Window canary = XCreateSimpleWindow(d, DefaultRootWindow(d),
                                                  0, 0, 64, 48, 0, 0, 0);
        XWMHints wmHints{};
        wmHints.flags = InputHint | StateHint;
        wmHints.input = True;
        wmHints.initial_state = NormalState;
        XSetWMHints(d, canary, &wmHints);
        XSizeHints sizeHints{};
        sizeHints.flags = PPosition | PSize;
        sizeHints.x = 0;
        sizeHints.y = 0;
        sizeHints.width = 64;
        sizeHints.height = 48;
        XSetNormalHints(d, canary, &sizeHints);
        XMapWindow(d, canary);
        XFlush(d);
        const bool managed = pollUntil([&] { return listContains(d, canary); }, 8000);
        XDestroyWindow(d, canary);
        XFlush(d);
        XCloseDisplay(d);
        return managed;
    }

    static bool listContains(Display* d, Window target) {
        Atom actualType;
        int actualFormat;
        unsigned long nItems, bytesAfter;
        unsigned char* data = nullptr;
        const Atom list = XInternAtom(d, "_NET_CLIENT_LIST", False);
        bool found = false;
        if (XGetWindowProperty(d, DefaultRootWindow(d), list, 0, 64, False,
                               XA_WINDOW, &actualType, &actualFormat,
                               &nItems, &bytesAfter, &data) == Success && data) {
            const auto* windows = reinterpret_cast<const Window*>(data);
            for (unsigned long i = 0; i < nItems; ++i) {
                if (windows[i] == target) {
                    found = true;
                    break;
                }
            }
            XFree(data);
        }
        return found;
    }

    int displayNumber_ = -1;
    char display_[16] = {};
    pid_t xvfbPid_ = -1;
    pid_t openboxPid_ = -1;
    int lockFd_ = -1;
    bool ready_ = false;
    bool displaySet_ = false;
    std::string failReason_;
    std::optional<std::string> oldDisplay_;
};

// 真实 WM 下的测试窗口：只创建/命名/映射，不写任何根属性——WM 自己维护
// _NET_CLIENT_LIST / _NET_ACTIVE_WINDOW，手写反而与 WM 状态打架。
class WmTestWindow {
public:
    WmTestWindow(int x, int y, int width, int height, const char* title) {
        display_ = openX11Display(nullptr);  // WmEnvironment 已设 DISPLAY
        if (!display_) return;
        window_ = XCreateSimpleWindow(display_, DefaultRootWindow(display_),
                                      x, y, width, height, 0, 0, 0);
        XStoreName(display_, window_, title);
        XClassHint hint;
        hint.res_name = const_cast<char*>("wingman-wm-test");
        hint.res_class = const_cast<char*>("WingmanWmTest");
        XSetClassHint(display_, window_, &hint);
        // ICCCM 合规：openbox 会怠慢缺 WM_HINTS（initial state）/WM_NORMAL_HINTS
        // 的裸 Xlib 窗口（实测 15s+ 不管理；xmessage 等合规客户端即时管理）
        XWMHints wmHints{};
        wmHints.flags = InputHint | StateHint;
        wmHints.input = True;
        wmHints.initial_state = NormalState;
        XSetWMHints(display_, window_, &wmHints);
        XSizeHints sizeHints{};
        sizeHints.flags = PPosition | PSize;
        sizeHints.x = x;
        sizeHints.y = y;
        sizeHints.width = width;
        sizeHints.height = height;
        XSetNormalHints(display_, window_, &sizeHints);
        XMapWindow(display_, window_);
        XFlush(display_);
    }

    ~WmTestWindow() {
        if (!display_) return;
        if (window_ != 0) {
            XDestroyWindow(display_, window_);
            XFlush(display_);
        }
        XCloseDisplay(display_);
    }

    WmTestWindow(const WmTestWindow&) = delete;
    WmTestWindow& operator=(const WmTestWindow&) = delete;

    bool valid() const { return display_ != nullptr && window_ != 0; }
    wingman::WindowHandle handle() const { return window_; }

private:
    Display* display_ = nullptr;
    Window window_ = 0;
};

} // namespace

TEST(X11WmIntegrationTest, MinimizeAndShowRoundtrip) {
    if (!xvfbAvailable() || !openboxAvailable()) {
        GTEST_SKIP() << "Xvfb/openbox not installed — 真实 WM 集成为运行期可选依赖";
    }
    WmEnvironment env;
    ASSERT_TRUE(env.valid()) << env.failReason();
    // isMinimized 在顶层 facade 未导出，经平台后端直查（同 X11WindowPlatformFeatures 先例）
    auto backend = wingman::platform::linux::createX11Window();

    WmTestWindow win(30, 40, 300, 200, "Wingman WM Minimize Window");
    ASSERT_TRUE(win.valid());
    // 映射经 WM 兑现为 viewable（异步）
    ASSERT_TRUE(pollUntil([&] { return wingman::Window::isVisible(win.handle()); }, 3000));
    EXPECT_FALSE(backend->isMinimized(win.handle()));

    // XIconifyWindow → WM 图标化：unmap + 写 _NET_WM_STATE_HIDDEN
    EXPECT_TRUE(wingman::Window::minimize(win.handle()));
    EXPECT_TRUE(pollUntil([&] { return !wingman::Window::isVisible(win.handle()); }, 3000));
    EXPECT_TRUE(backend->isMinimized(win.handle()));

    // XMapWindow → WM 取消图标化（show/hide 为后端方法，facade 未导出）
    EXPECT_TRUE(backend->show(win.handle()));
    EXPECT_TRUE(pollUntil([&] { return wingman::Window::isVisible(win.handle()); }, 3000));
    EXPECT_FALSE(backend->isMinimized(win.handle()));
}

TEST(X11WmIntegrationTest, MaximizeRestoreRoundtrip) {
    if (!xvfbAvailable() || !openboxAvailable()) {
        GTEST_SKIP() << "Xvfb/openbox not installed — 真实 WM 集成为运行期可选依赖";
    }
    WmEnvironment env;
    ASSERT_TRUE(env.valid()) << env.failReason();
    auto backend = wingman::platform::linux::createX11Window();

    WmTestWindow win(50, 60, 200, 120, "Wingman WM Maximize Window");
    ASSERT_TRUE(win.valid());
    ASSERT_TRUE(pollUntil([&] { return wingman::Window::isVisible(win.handle()); }, 3000));

    // maximize() 发送 _NET_WM_STATE MAXIMIZED_HORZ（产品语义只请求水平方向），
    // openbox 兑现后写状态原子并加宽窗口到工作区
    const auto before = wingman::Window::getBounds(win.handle());
    EXPECT_TRUE(wingman::Window::maximize(win.handle()));
    EXPECT_TRUE(pollUntil([&] { return backend->isMaximized(win.handle()); }, 3000));
    const auto maximized = wingman::Window::getBounds(win.handle());
    EXPECT_GT(maximized.width, before.width);

    EXPECT_TRUE(wingman::Window::restore(win.handle()));
    EXPECT_TRUE(pollUntil([&] { return !backend->isMaximized(win.handle()); }, 3000));
}

TEST(X11WmIntegrationTest, ActivateForegroundAndEnumerate) {
    if (!xvfbAvailable() || !openboxAvailable()) {
        GTEST_SKIP() << "Xvfb/openbox not installed — 真实 WM 集成为运行期可选依赖";
    }
    WmEnvironment env;
    ASSERT_TRUE(env.valid()) << env.failReason();

    WmTestWindow win(10, 10, 220, 140, "Wingman WM Active Window");
    ASSERT_TRUE(win.valid());

    // WM 自维护 _NET_CLIENT_LIST：映射后无需手写属性即可被枚举/查找
    ASSERT_TRUE(pollUntil(
        [&] { return wingman::Window::find("WM Active") == win.handle(); }, 3000));

    // _NET_ACTIVE_WINDOW 消息 → WM 设焦点并更新根属性；isForeground/getForeground
    // 读的就是这份 WM 维护的状态
    EXPECT_TRUE(wingman::Window::activate(win.handle()));
    EXPECT_TRUE(pollUntil([&] { return wingman::Window::isForeground(win.handle()); }, 3000));
    EXPECT_EQ(wingman::Window::getForeground(), win.handle());
}


#endif // __linux__
