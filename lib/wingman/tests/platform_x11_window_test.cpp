#if defined(__linux__)

#include "platform_x11_test_common.hpp"

// ========== 窗口（顶层 Window 装配断链回归守卫 + X11 窗口管理） ==========

TEST_F(X11PlatformTest, WindowEnumerateFindTitle) {
    // 装配断链回归守卫：顶层 Window 在非 Windows 曾恒空 stub，
    // X11Window 全库零消费者；有 X 时 enumerate/find 必须真出结果。
    X11ServerLockGuard x11Lock;
    TestX11Window win(30, 40, 220, 150, "Wingman Test Window");
    ASSERT_TRUE(win.valid());
    win.setActive();

    auto all = wingman::Window::enumerate();
    ASSERT_FALSE(all.empty());
    const wingman::WindowInfo* info = nullptr;
    for (const auto& wi : all) {
        if (wi.handle == win.handle()) info = &wi;
    }
    ASSERT_NE(info, nullptr) << "test window missing from enumerate()";
    EXPECT_EQ(info->title, "Wingman Test Window");
    EXPECT_EQ(info->bounds.x, 30);
    EXPECT_EQ(info->bounds.y, 40);
    EXPECT_EQ(info->bounds.width, 220);
    EXPECT_EQ(info->bounds.height, 150);
    EXPECT_TRUE(info->isForeground);

    EXPECT_EQ(wingman::Window::find("Test Window"), win.handle());
    EXPECT_EQ(wingman::Window::find("no-such-window-title"), 0u);
    auto matches = wingman::Window::findAll("Wingman");
    EXPECT_EQ(matches.size(), 1u);
    EXPECT_EQ(wingman::Window::getTitle(win.handle()), "Wingman Test Window");
    EXPECT_EQ(wingman::Window::getForeground(), win.handle());
    // 已存在 → waitFor 立即真，不耗超时
    EXPECT_TRUE(wingman::Window::waitFor("Test Window", 500));
}

// node 模块 getWindows 胶水（misc_modules.cpp）的窗口枚举循环体：
// 无 X 时 Window::enumerate() 返回空数组、循环体零覆盖；此处借 TestX11Window
// 模拟 _NET_CLIENT_LIST 使胶水循环体真实执行（2026-09-22 覆盖率第六批）。
TEST_F(X11PlatformTest, NodeGlueGetWindowsEnumeratesCreatedWindow) {
    X11ServerLockGuard x11Lock;
    TestX11Window win(50, 60, 180, 120, "Wingman Glue GetWindows");
    ASSERT_TRUE(win.valid());

    wingman::script::ModuleDescriptor mod;
    for (auto& m : wingman::script::modules::getAllModules()) {
        if (m.name == "node") { mod = m; break; }
    }
    ASSERT_EQ(mod.name, "node");
    const wingman::script::ModuleDescriptor::FunctionEntry* fn = nullptr;
    for (const auto& f : mod.functions) {
        if (f.name == "getWindows") { fn = &f; break; }
    }
    ASSERT_NE(fn, nullptr);

    const auto arr = (*fn)({});
    ASSERT_TRUE(arr.isArray());
    ASSERT_GE(arr.size(), 1u);
    const wingman::script::ScriptValue* entry = nullptr;
    for (const auto& v : arr.arrayVal) {
        const auto* h = v.get("handle");
        if (h && static_cast<uint64_t>(h->asInt()) == win.handle()) { entry = &v; break; }
    }
    ASSERT_NE(entry, nullptr) << "test window missing from getWindows()";
    EXPECT_EQ(entry->get("title")->asString(), "Wingman Glue GetWindows");
    EXPECT_EQ(entry->get("isForeground")->asBool(), false);
    const auto* bounds = entry->get("bounds");
    ASSERT_NE(bounds, nullptr);
    EXPECT_EQ(bounds->get("x")->asInt(), 50);
    EXPECT_EQ(bounds->get("width")->asInt(), 180);
}

TEST_F(X11PlatformTest, WindowBoundsMoveResize) {
    X11ServerLockGuard x11Lock;
    TestX11Window win(10, 20, 200, 100, "Wingman Move Window");
    ASSERT_TRUE(win.valid());

    EXPECT_TRUE(wingman::Window::isValid(win.handle()));
    EXPECT_TRUE(wingman::Window::isVisible(win.handle()));

    // XMoveWindow/XResizeWindow 直接生效，无需窗口管理器（Xvfb 可验证）
    EXPECT_TRUE(wingman::Window::move(win.handle(), 77, 88));
    auto b = wingman::Window::getBounds(win.handle());
    EXPECT_EQ(b.x, 77);
    EXPECT_EQ(b.y, 88);
    EXPECT_EQ(b.width, 200);
    EXPECT_EQ(b.height, 100);

    EXPECT_TRUE(wingman::Window::resize(win.handle(), 111, 122));
    b = wingman::Window::getBounds(win.handle());
    EXPECT_EQ(b.x, 77);
    EXPECT_EQ(b.y, 88);
    EXPECT_EQ(b.width, 111);
    EXPECT_EQ(b.height, 122);

    EXPECT_TRUE(wingman::Window::setBounds(win.handle(), {11, 22, 101, 103}));
    b = wingman::Window::getBounds(win.handle());
    EXPECT_EQ(b.x, 11);
    EXPECT_EQ(b.y, 22);
    EXPECT_EQ(b.width, 101);
    EXPECT_EQ(b.height, 103);
}

TEST_F(X11PlatformTest, WindowInvalidHandleIsSafe) {
    // 回归守卫：无效句柄曾触发 BadWindow → Xlib 默认 error handler exit()
    // 杀死整个进程；现在以空值/false 优雅呈现（宽容 handler，同 x11_capture），
    // 且写操作经顶层 isValid 前置不再假成功。
    X11ServerLockGuard x11Lock;
    const wingman::WindowHandle dead = 0xDEADBEEF;

    EXPECT_FALSE(wingman::Window::isValid(dead));
    EXPECT_EQ(wingman::Window::getTitle(dead), "");
    const auto emptyBounds = wingman::Window::getBounds(dead);
    EXPECT_EQ(emptyBounds.x, 0);
    EXPECT_EQ(emptyBounds.y, 0);
    EXPECT_EQ(emptyBounds.width, 0);
    EXPECT_EQ(emptyBounds.height, 0);
    EXPECT_FALSE(wingman::Window::isVisible(dead));
    EXPECT_FALSE(wingman::Window::isForeground(dead));
    EXPECT_FALSE(wingman::Window::activate(dead));
    EXPECT_FALSE(wingman::Window::close(dead));
    EXPECT_FALSE(wingman::Window::minimize(dead));
    EXPECT_FALSE(wingman::Window::maximize(dead));
    EXPECT_FALSE(wingman::Window::restore(dead));
    EXPECT_FALSE(wingman::Window::move(dead, 1, 2));
    EXPECT_FALSE(wingman::Window::resize(dead, 3, 4));
    EXPECT_FALSE(wingman::Window::setBounds(dead, {0, 0, 10, 10}));
    EXPECT_FALSE(wingman::Window::waitFor("never-appears-anywhere", 200));
    EXPECT_TRUE(wingman::Window::waitClose("never-appears-anywhere", 200));
}

TEST_F(X11PlatformTest, X11WindowPlatformFeatures) {
    X11ServerLockGuard x11Lock;
    auto window = wingman::platform::linux::createX11Window();
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(window->getBackendName(), "X11");

    TestX11Window win(0, 0, 100, 80, "Wingman Platform Window");
    ASSERT_TRUE(win.valid());

    // WM_CLASS（res_name / res_class 任一匹配）。全量运行时前面的用例
    // 使 X server 侧属性同步偶发滞后，findByClassName 的枚举快照可能
    // 短暂看不到新窗口——轮询等待（至多 ~2s）
    auto findByClass = [&window]() {
        return window->findByClassName("wingman-test");
    };
    Window byClass = findByClass();
    for (int i = 0; i < 40 && byClass != win.handle(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        byClass = findByClass();
    }
    EXPECT_EQ(byClass, win.handle());
    EXPECT_EQ(window->findByClassName("WingmanTest"), win.handle());
    // 无命中类名：枚举耗尽后返回 NullWindowHandle（既有用例只测过命中腿）
    EXPECT_EQ(window->findByClassName("definitely-no-such-class"), wingman::platform::NullWindowHandle);

    // _NET_WM_PID
    auto byPid = window->findByProcessId(static_cast<uint32_t>(::getpid()));
    EXPECT_NE(std::find(byPid.begin(), byPid.end(), win.handle()), byPid.end());
    const auto pid = window->getProcessId(win.handle());
    ASSERT_TRUE(pid.has_value());
    EXPECT_EQ(*pid, static_cast<uint32_t>(::getpid()));

    // hide/show 翻转：XUnmapWindow/XMapWindow 直接生效，无需窗口管理器
    EXPECT_TRUE(window->hide(win.handle()));
    EXPECT_FALSE(window->isVisible(win.handle()));
    EXPECT_TRUE(window->show(win.handle()));
    EXPECT_TRUE(window->isVisible(win.handle()));

    // activate：XRaiseWindow + XSetInputFocus 真执行（无 WM 时
    // _NET_ACTIVE_WINDOW 无人维护，只断言请求本身成功）
    EXPECT_TRUE(window->activate(win.handle()));
}

TEST_F(X11PlatformTest, X11WindowCloseCenterAndWaitFamily) {
    // 第十批补测：close/forceClose/center 与 wait 家族的轮询/超时路径——
    // 顶层 facade 既有用例只覆盖过 waitFor 立即命中与 waitClose 立即真，
    // 后端 waitFor 轮询循环、waitClose 超时 false、waitForForeground 与
    // close/forceClose/center 从未触达。
    X11ServerLockGuard x11Lock;
    auto window = wingman::platform::linux::createX11Window();
    ASSERT_NE(window, nullptr);

    // 标题进程唯一：X server 的窗口枚举是全局的，ctest --parallel 下其他
    // 测试进程的同名窗口会被标题匹配类断言（waitFor/waitClose）互相串扰
    const std::string uniqueTitle = std::string("Wingman Close Center Window #") +
        std::to_string(static_cast<long>(::getpid()));
    TestX11Window win(0, 0, 120, 90, uniqueTitle.c_str());
    ASSERT_TRUE(win.valid());
    win.setActive();

    // center：DefaultScreenOfDisplay 尺寸居中（XGetWindowAttributes + XMoveWindow）
    EXPECT_TRUE(window->center(win.handle(), 0));

    // wait 家族：250ms 轮询窗口足以覆盖 sleep 循环体
    EXPECT_FALSE(window->waitFor("no-such-title-anywhere", 250));
    EXPECT_FALSE(window->waitClose(uniqueTitle, 250));   // 窗口在 → 轮询至超时
    EXPECT_TRUE(window->waitClose("no-such-title-anywhere", 250)); // 无匹配 → 立即真
    EXPECT_FALSE(window->waitForForeground(0xDEADBEEF, 250));      // 死句柄非前台
    EXPECT_TRUE(window->waitForForeground(win.handle(), 2000));    // setActive 已写 _NET_ACTIVE_WINDOW

    const auto info = window->getBackendInfo();
    EXPECT_EQ(info.name, "X11");
    EXPECT_TRUE(info.isInitialized);

    // close：WM_PROTOCOLS/WM_DELETE_WINDOW ClientMessage 发送成功（无 WM 时
    // 无人响应，断言请求本身即可）
    EXPECT_TRUE(window->close(win.handle()));

    // forceClose（XKillClient）必须打在“别的客户端”的窗口上：XKillClient
    // 会终结拥有该资源的连接——win 由本进程 display_ 创建，杀它等于切断
    // 自己的 X 连接（后续任何 Xlib 调用触发 fatal IO error 整进程退出）。
    // 真实场景是强杀外部进程的窗口，故 fork 子进程扮演外部客户端持窗。
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);
    const pid_t holder = fork();
    ASSERT_GE(holder, 0);
    if (holder == 0) {
        // 子进程：开独立 X 连接建窗，句柄经管道交给父进程；连接被杀
        // （EOF 可读）后 _exit 跳过 gcov flush，避免与父进程 gcda 合并竞争
        close(fds[0]);
        Display* d = openX11Display(nullptr);
        if (!d) _exit(1);
        Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), 10, 10, 80, 60, 0, 0, 0);
        if (w == 0) _exit(3); // 窗口创建失败时父侧 forceClose(0) 必落空
        XMapWindow(d, w);
        // XSync 而非 XFlush：句柄交给父进程之前，CreateWindow/MapWindow 必须
        // 已被 server 处理。XFlush 只保证发出——父进程可能抢在 server 建窗之前
        // XKillClient，杀不到还不存在的资源，子进程困在 select 里直到父侧 5s
        // 护栏报警（2026-09-27 flake 登记项之一）
        XSync(d, False);
        if (::write(fds[1], &w, sizeof(w)) != static_cast<ssize_t>(sizeof(w))) _exit(2);
        close(fds[1]);
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(ConnectionNumber(d), &rfds);
        // 限时 select：XKillClient 送达失败时 EOF 永不到来，无超时会让
        // 父进程 waitpid 挂死整个运行（v18 基线采集实测一次）
        struct timeval tv = {10, 0};
        select(ConnectionNumber(d) + 1, &rfds, nullptr, nullptr, &tv);
        _exit(0);
    }
    close(fds[1]);
    Window foreignWin = 0;
    const ssize_t n = ::read(fds[0], &foreignWin, sizeof(foreignWin));
    close(fds[0]);
    ASSERT_EQ(n, static_cast<ssize_t>(sizeof(foreignWin)));

    EXPECT_TRUE(window->forceClose(foreignWin));
    int holderStatus = 0;
    // 防挂死护栏：正常路径连接被杀、select 命中 EOF 立即退出；极端时序下
    // （XKillClient 落空）子进程会困在 select 里，waitpid(0) 阻塞式等待将
    // 挂死整个运行——限时轮询，超时 SIGKILL 收尸并如实报失败。基线 5s 随 CPU
    // 超售缩放：共享机 load 数倍于核数时，子进程从 EOF 到被收割全程都可能被
    // 调度饿死，固定窗口报警的是负载不是被测行为
    const double oversell = x11OversubscriptionFactor();
    const int reapIterations = static_cast<int>(100 * oversell);
    bool holderGone = false;
    for (int i = 0; i < reapIterations; ++i) {
        if (waitpid(holder, &holderStatus, WNOHANG) == holder) {
            holderGone = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (!holderGone) {
        kill(holder, SIGKILL);
        waitpid(holder, &holderStatus, 0);
        ADD_FAILURE() << "holder did not exit within " << (reapIterations * 50 / 1000)
                      << "s after forceClose (load factor " << oversell << ")";
    }
    EXPECT_TRUE(WIFEXITED(holderStatus));
    for (int i = 0; i < 20 && window->isValid(foreignWin); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    EXPECT_FALSE(window->isValid(foreignWin));  // 杀连接连带销毁其全部资源
}

// 回归钉（2026-09-27 flake 根治轮）：X server 对「前一个本地连接刚断开 → 新
// 连接立即到达」会在 accept 阶段瞬态掐断新连接——strace 双侧实证：server 读完
// SO_PEERCRED 与 /proc/<pid>/cmdline 后连 setup 请求都不读就 shutdown，客户端
// XOpenDisplay 返回 NULL，紧接着再 open 立即成功（裸 open→close 循环 load≈40
// 实测 11/3000 失败、11/11 重试即成功）。当时单用例在 xvfb-run 下 ~1/4 红：
// 门面 initialize 撞上一次拒绝 → initialized_=false → center/close/
// isInitialized 成片 false、forceClose 落空、holder 护栏报警，整条多米诺都源自
// 这一次 open。修复 = 生产侧 openX11Display 重试（x11_display.hpp）。
// 每轮 = 「探测连接 open→close」紧接「门面 open」——复刻实证的竞态形状。轮数
// 25：负载高时每次 open ~75ms，整测 ~6s；瞬态拒绝本身不作断言（裸 open 失败是
// 预期压力），钉死的是门面必须靠重试站起来，以及末轮门面功能可用。
TEST_F(X11PlatformTest, WindowInitializeSurvivesConnectionChurn) {
    X11ServerLockGuard x11Lock;
    for (int i = 0; i < 25; ++i) {
        Display* probe = XOpenDisplay(nullptr);
        if (probe) {
            XCloseDisplay(probe);
        }
        auto window = wingman::platform::linux::createX11Window();
        ASSERT_NE(window, nullptr) << "iter " << i;
        EXPECT_TRUE(window->getBackendInfo().isInitialized)
            << "iter " << i << ": facade initialize() succumbed to transient churn";
    }
    // 末轮门面功能可用（churn 后不是只剩空壳）
    auto window = wingman::platform::linux::createX11Window();
    ASSERT_NE(window, nullptr);
    TestX11Window win(0, 0, 120, 90, "Wingman Churn Window");
    ASSERT_TRUE(win.valid());
    win.setActive();
    EXPECT_TRUE(window->center(win.handle(), 0));
    EXPECT_TRUE(window->close(win.handle()));
}


TEST_F(X11PlatformTest, WindowCenterInvalidHandleReturnsFalse) {
    X11ServerLockGuard x11Lock;
    auto window = wingman::platform::linux::createX11Window();
    ASSERT_NE(window, nullptr);
    // XGetWindowAttributes(死句柄) 失败 → false（290）
    EXPECT_FALSE(window->center(0xDEADBEEF, 0));
}


#endif // __linux__
