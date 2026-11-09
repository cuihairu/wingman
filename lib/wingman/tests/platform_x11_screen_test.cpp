#if defined(__linux__)

#include "platform_x11_test_common.hpp"

// ========== 显示器元数据（走 createPlatformScreen 分发链） ==========

TEST_F(X11PlatformTest, ScreenMetadataViaPlatformFactory) {
    auto screen = wingman::platform::createPlatformScreen();
    ASSERT_NE(screen, nullptr);
    EXPECT_EQ(screen->getBackendName(), "X11");
    EXPECT_GE(screen->getMonitorCount(), 1);

    const auto bounds = screen->getMonitorBounds(0);
    EXPECT_GT(bounds.width, 0);
    EXPECT_GT(bounds.height, 0);
    EXPECT_FALSE(screen->getMonitorName(0).empty());
}

// ========== 截图（X11Capture：XGetImage 真读回 Xvfb 画面） ==========

TEST_F(X11PlatformTest, CaptureScreenMatchesMonitorBounds) {
    auto capture = wingman::platform::linux::createX11Capture({});
    ASSERT_NE(capture, nullptr);
    ASSERT_TRUE(capture->isAvailable());
    ASSERT_GE(capture->getMonitorCount(), 1);

    const auto bounds = capture->getMonitorBounds(0);
    auto full = capture->captureScreen(0);
    ASSERT_NE(full, nullptr);
    EXPECT_EQ(full->getWidth(), bounds.width);
    EXPECT_EQ(full->getHeight(), bounds.height);
}

TEST_F(X11PlatformTest, CaptureRegionReturnsRequestedSize) {
    auto capture = wingman::platform::linux::createX11Capture({});
    ASSERT_NE(capture, nullptr);
    ASSERT_TRUE(capture->isAvailable());

    const wingman::platform::Rect region{0, 0, 64, 64};
    auto bitmap = capture->captureRegion(region);
    ASSERT_NE(bitmap, nullptr);
    EXPECT_EQ(bitmap->getWidth(), 64);
    EXPECT_EQ(bitmap->getHeight(), 64);
}

// ========== 顶层 Screen 装配（此前 Linux 恒 nullptr 的 stub，断链接线回归守卫） ==========

TEST_F(X11PlatformTest, ScreenCaptureRegionReturnsBitmap) {
    auto bitmap = wingman::Screen::capture(wingman::Rect{0, 0, 64, 32});
    ASSERT_NE(bitmap, nullptr);
    EXPECT_EQ(bitmap->getWidth(), 64);
    EXPECT_EQ(bitmap->getHeight(), 32);
}

TEST_F(X11PlatformTest, ScreenCaptureFullScreenMatchesDimensions) {
    auto full = wingman::Screen::capture();
    ASSERT_NE(full, nullptr);
    EXPECT_EQ(full->getWidth(), wingman::Screen::getScreenWidth());
    EXPECT_EQ(full->getHeight(), wingman::Screen::getScreenHeight());
}

TEST_F(X11PlatformTest, ScreenGetPixelOutOfBoundsIsSafe) {
    // 越界坐标曾会因 XGetImage BadMatch 触发默认 error handler exit 进程；
    // 现以宽容 handler 呈现为 Color() 默认值（Xvfb 根窗口黑 → 越界返回黑）
    const auto pixel = wingman::Screen::getPixel(-5, -5);
    EXPECT_EQ(pixel.r, 0);
    EXPECT_EQ(pixel.g, 0);
    EXPECT_EQ(pixel.b, 0);
}

TEST_F(X11PlatformTest, ScreenFindColorOnRootWindow) {
    // 自洽断言：先抓图算出「扫描序首命中位」，再对 findColor 结果精确对照
    // （不假设根窗口底色是否均匀，Xvfb/桌面均确定）
    const wingman::Rect region{0, 0, 32, 32};
    auto bitmap = wingman::Screen::capture(region);
    ASSERT_NE(bitmap, nullptr);

    const auto base = bitmap->getPixel(2, 3);
    wingman::Point expected{-1, -1};
    for (int y = 0; y < bitmap->getHeight() && expected.x < 0; ++y) {
        for (int x = 0; x < bitmap->getWidth(); ++x) {
            if (bitmap->getPixel(x, y).matches(base, 0)) {
                expected.x = x;
                expected.y = y;
                break;
            }
        }
    }
    ASSERT_GE(expected.x, 0) << "base color not found in captured region?!";

    wingman::Point result;
    EXPECT_TRUE(wingman::Screen::findColor(base, region, 0, result));
    EXPECT_EQ(result.x, expected.x);
    EXPECT_EQ(result.y, expected.y);

    // 容差 10 下补色需与区域内所有像素距离均超 10 才断言不命中（均匀底色成立）
    const auto inverted = wingman::Color(
        static_cast<uint8_t>(~base.r), static_cast<uint8_t>(~base.g),
        static_cast<uint8_t>(~base.b));
    const int dr = static_cast<int>(inverted.r) - static_cast<int>(base.r);
    const int dg = static_cast<int>(inverted.g) - static_cast<int>(base.g);
    const int db = static_cast<int>(inverted.b) - static_cast<int>(base.b);
    if (dr * dr + dg * dg + db * db > 10 * 10) {
        EXPECT_FALSE(wingman::Screen::findColor(inverted, region, 10, result));
    }

    auto many = wingman::Screen::findColors(base, wingman::Rect{0, 0, 8, 4}, 0, 5);
    EXPECT_GE(many.size(), 1u);
    EXPECT_LE(many.size(), 5u);
}

#ifdef WINGMAN_ENABLE_VISION
TEST_F(X11PlatformTest, ScreenFindImageLocatesDrawnPattern) {
    // 模板匹配（vision 构建接线，与 Windows 共用实现）：根窗口画不对称图案
    // → 截取该区域存 PNG 模板 → findImage 全屏找回。图案唯一性保证命中位置
    // 确定（均匀底色下多个完美匹配会让 minMaxLoc 位置不定）；负向断言：
    // 不存在的模板路径优雅 false
    X11ServerLockGuard x11Lock;
    Display* d = openX11Display(nullptr);
    ASSERT_NE(d, nullptr);
    Window root = DefaultRootWindow(d);
    GC gc = XCreateGC(d, root, 0, nullptr);
    // 红块 + 相邻绿块（宽度不对称）——黑底根窗口上的唯一图案
    XSetForeground(d, gc, 0xFF0000);
    XFillRectangle(d, root, gc, 200, 150, 32, 24);
    XSetForeground(d, gc, 0x00FF00);
    XFillRectangle(d, root, gc, 232, 150, 16, 24);
    XFlush(d);
    XFreeGC(d, gc);
    XCloseDisplay(d);

    const wingman::Rect tplRect{200, 150, 48, 24};
    auto tpl = wingman::Screen::capture(tplRect);
    ASSERT_NE(tpl, nullptr);
    // 自洽前置：截图内容与绘制一致（红/绿块内像素命中对应通道）
    EXPECT_EQ(tpl->getPixel(4, 4).r, 255);
    EXPECT_EQ(tpl->getPixel(40, 12).g, 255);

    char path[64];
    std::snprintf(path, sizeof(path), "/tmp/wingman_test_tpl_%d.png",
                  static_cast<int>(::getpid()));
    ASSERT_TRUE(tpl->save(path));

    wingman::Point hit{-1, -1};
    const wingman::Rect full{0, 0, wingman::Screen::getScreenWidth(),
                             wingman::Screen::getScreenHeight()};
    EXPECT_TRUE(wingman::Screen::findImage(path, full, 0.99, hit));
    EXPECT_EQ(hit.x, tplRect.x);
    EXPECT_EQ(hit.y, tplRect.y);

    // 模板路径不存在（imread 失败）→ 优雅 false
    EXPECT_FALSE(wingman::Screen::findImage(
        "/nonexistent/wingman-template.png", wingman::Rect{0, 0, 64, 64}, 0.9, hit));
    std::remove(path);

    // 清理画在根窗口的图案（跨轮次/其他用例自洽性）
    d = openX11Display(nullptr);
    if (d) {
        XClearArea(d, DefaultRootWindow(d), tplRect.x, tplRect.y,
                   tplRect.width, tplRect.height, False);
        XFlush(d);
        XCloseDisplay(d);
    }
}
#endif // WINGMAN_ENABLE_VISION


// ========== X11Screen 全方法触达（IScreen 的 X11/XRandR 实现） ==========
// X11Screen 类定义在 x11_screen.cpp 内部（无头文件），工厂 new 后立即
// initialize —— !initialized_ 分支对测试不可达，不硬凑。

TEST_F(X11PlatformTest, ScreenMonitorInfoFullSweep) {
    auto screen = wingman::platform::createPlatformScreen();
    ASSERT_NE(screen, nullptr);

    const int count = screen->getMonitorCount();
    EXPECT_GE(count, 1);
    const int primary = screen->getPrimaryMonitorIndex();
    EXPECT_GE(primary, 0);
    EXPECT_LT(primary, count);

    const auto bounds = screen->getPrimaryMonitorBounds();
    EXPECT_GT(bounds.width, 0);
    EXPECT_GT(bounds.height, 0);
    // 越界索引走 DefaultScreenOfDisplay 回退分支，仍返回有效尺寸
    const auto fallback = screen->getMonitorBounds(count + 3);
    EXPECT_GT(fallback.width, 0);

    const auto workArea = screen->getMonitorWorkArea(0);
    EXPECT_GT(workArea.width, 0);
    EXPECT_FALSE(screen->getMonitorName(count + 3).empty());
    EXPECT_TRUE(screen->isPrimaryMonitor(primary));
    // Xvfb 单屏：仅 primary 存在，非 primary 索引恒 false
    if (count > 1) {
        const int other = (primary + 1) % count;
        EXPECT_EQ(screen->isPrimaryMonitor(other), other == primary);
    }
}

TEST_F(X11PlatformTest, ScreenDpiAndCoordinateMapping) {
    auto screen = wingman::platform::createPlatformScreen();
    ASSERT_NE(screen, nullptr);

    const int dpi = screen->getDpi(0);
    EXPECT_GE(dpi, 48);
    EXPECT_LE(dpi, 960);
    const double scale = screen->getDpiScale(0);
    EXPECT_GT(scale, 0.0);

    // 逻辑↔物理映射往返：允许 ±1 取整误差（Xvfb 的 DPI 由屏幕尺寸/毫米数
    // 自洽算出，scale 常非精确 1.0，两次整除截断可偏差 1 像素）
    const wingman::platform::Point p{123, 45};
    const auto phys = screen->logicalToPhysical(p, 0);
    const auto back = screen->physicalToLogical(phys, 0);
    EXPECT_LE(std::abs(back.x - p.x), 1);
    EXPECT_LE(std::abs(back.y - p.y), 1);
}

TEST_F(X11PlatformTest, ScreenDisplayModesAndVirtualBounds) {
    auto screen = wingman::platform::createPlatformScreen();
    ASSERT_NE(screen, nullptr);

    // Xvfb 提供 screen resources：模式列表可枚举（虚拟模式 dotClock=0 →
    // 刷新率经除零防护为 0，而非 NaN 转换出的垃圾值）
    auto modes = screen->getSupportedDisplayModes(0);
    for (const auto& m : modes) {
        EXPECT_GT(m.width, 0);
        EXPECT_GT(m.height, 0);
        EXPECT_GE(m.refreshRate, 0);
    }
    // 越界索引 → 空列表分支
    EXPECT_TRUE(screen->getSupportedDisplayModes(9).empty());

    auto current = screen->getCurrentDisplayMode(0);
    ASSERT_TRUE(current.has_value());
    EXPECT_GT(current->width, 0);

    // xrandr 模式切换需 root 权限：实现恒 false
    EXPECT_FALSE(screen->setDisplayMode(0, {1280, 800, 60, 32}));
    EXPECT_FALSE(screen->resetDisplayMode(0));

    // 屏保/显示器电源：X11 实现恒"未激活"
    EXPECT_FALSE(screen->isScreenSaverRunning());
    EXPECT_FALSE(screen->isMonitorOff());
    screen->startScreenSaver();
    screen->wakeUpMonitor();

    const auto virt = screen->getVirtualScreenBounds();
    EXPECT_GT(virt.width, 0);
    EXPECT_GT(virt.height, 0);
}

TEST_F(X11PlatformTest, ScreenMonitorFromPointAndWindow) {
    auto screen = wingman::platform::createPlatformScreen();
    ASSERT_NE(screen, nullptr);

    // 原点必落在某个显示器上（Xvfb 单屏 → 0）
    EXPECT_EQ(screen->getMonitorFromPoint({0, 0}), 0);

    // TestX11Window 的 class 为 WingmanTest/wingman-test，稳定可寻
    TestX11Window win(0, 0, 100, 80, "Wingman Screen Monitor Window");
    ASSERT_TRUE(win.valid());
    EXPECT_EQ(screen->getMonitorFromWindow(win.handle()), 0);
    EXPECT_EQ(screen->getMonitorFromWindow(0), 0); // 无效句柄分支

    const auto info = screen->getBackendInfo();
    EXPECT_EQ(info.name, "X11");
    EXPECT_TRUE(info.isInitialized);
}


// ========== 窗口捕获（X11Capture::captureWindow / captureWindowRegion） ==========

TEST_F(X11PlatformTest, CaptureWindowGrabsWindowContent) {
    TestX11Window win(40, 50, 180, 120, "Wingman Capture Window");
    ASSERT_TRUE(win.valid());
    auto capture = wingman::platform::linux::createX11Capture({});
    ASSERT_NE(capture, nullptr);

    // 画白色块做内容指纹（XCreateSimpleWindow 背景为 0 → 黑）
    Display* d = openX11Display(nullptr);
    ASSERT_NE(d, nullptr);
    GC gc = XCreateGC(d, win.handle(), 0, nullptr);
    XSetForeground(d, gc, 0xFFFFFF);
    XFillRectangle(d, win.handle(), gc, 5, 5, 24, 16);
    XFlush(d);
    XFreeGC(d, gc);
    XCloseDisplay(d);

    auto bmp = capture->captureWindow(win.handle());
    ASSERT_NE(bmp, nullptr);
    EXPECT_EQ(bmp->getWidth(), 180);
    EXPECT_EQ(bmp->getHeight(), 120);
    EXPECT_EQ(bmp->getPixel(5, 5).r, 255);    // 白块内容被真实抓到
    EXPECT_EQ(bmp->getPixel(5, 5).g, 255);
    EXPECT_EQ(bmp->getPixel(100, 100).r, 0);  // 未绘制区保持黑底
}

TEST_F(X11PlatformTest, CaptureWindowInvalidHandlesAreSafe) {
    auto capture = wingman::platform::linux::createX11Capture({});
    ASSERT_NE(capture, nullptr);

    // 无效句柄直接拒绝
    EXPECT_EQ(capture->captureWindow(0), nullptr);
    EXPECT_EQ(capture->captureWindowRegion(0, wingman::platform::Rect{0, 0, 8, 8}), nullptr);

    // 已销毁句柄：XGetWindowAttributes 失败 → nullptr（宽容 error handler 吞 BadWindow）
    Display* d = openX11Display(nullptr);
    ASSERT_NE(d, nullptr);
    Window doomed = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, 8, 8, 0, 0, 0);
    XMapWindow(d, doomed);
    XSync(d, False);
    XDestroyWindow(d, doomed);
    XSync(d, False);
    XCloseDisplay(d);
    EXPECT_EQ(capture->captureWindow(doomed), nullptr);
}

TEST_F(X11PlatformTest, CaptureWindowRegionOfWindow) {
    TestX11Window win(0, 0, 160, 100, "Wingman Window Region");
    ASSERT_TRUE(win.valid());
    auto capture = wingman::platform::linux::createX11Capture({});
    ASSERT_NE(capture, nullptr);

    auto bmp = capture->captureWindowRegion(win.handle(), wingman::platform::Rect{0, 0, 64, 48});
    ASSERT_NE(bmp, nullptr);
    EXPECT_EQ(bmp->getWidth(), 64);
    EXPECT_EQ(bmp->getHeight(), 48);

    // 窗口内越界区域：XGetImage BadMatch → nullptr（XGetImage 失败分支）
    EXPECT_EQ(capture->captureWindowRegion(win.handle(), wingman::platform::Rect{0, 0, 4096, 4096}),
              nullptr);
}

TEST_F(X11PlatformTest, CaptureMonitorMetadataAndRegionFallback) {
    auto capture = wingman::platform::linux::createX11Capture({});
    ASSERT_NE(capture, nullptr);

    const int count = capture->getMonitorCount();
    EXPECT_GE(count, 1);
    const auto bounds0 = capture->getMonitorBounds(0);
    EXPECT_GT(bounds0.width, 0);
    EXPECT_GT(bounds0.height, 0);
    EXPECT_FALSE(capture->getMonitorName(0).empty());

    // 越界索引回退整屏 / 占位名（与 X11Screen 同构）
    const auto fallback = capture->getMonitorBounds(count + 3);
    EXPECT_GT(fallback.width, 0);
    EXPECT_EQ(capture->getMonitorName(count + 3), "Monitor " + std::to_string(count + 3));

    // captureRegion(monitorIndex, region)：显式区域按请求尺寸
    auto bmp = capture->captureRegion(0, wingman::platform::Rect{4, 4, 32, 24});
    ASSERT_NE(bmp, nullptr);
    EXPECT_EQ(bmp->getWidth(), 32);
    EXPECT_EQ(bmp->getHeight(), 24);

    // 空区域回退显示器全 bounds
    auto full = capture->captureRegion(0, wingman::platform::Rect{});
    ASSERT_NE(full, nullptr);
    EXPECT_EQ(full->getWidth(), bounds0.width);

    EXPECT_TRUE(capture->isAvailable());
    EXPECT_EQ(capture->getBackendName(), "X11");
    const auto info = capture->getBackendInfo();
    EXPECT_EQ(info.name, "X11");
    EXPECT_TRUE(info.isInitialized);
}


// ========== 第十一批补测：查询回退 / RESOURCE_MANAGER DPI 链 / 无显示降级 ==========
//
// XResourceManagerString 返回的是 Xlib 连接建立时缓存的字符串，属性写入后
// 必须新开 Display 连接（新的 createPlatformScreen）才能观察到。

namespace {

// RESOURCE_MANAGER 属性 RAII：构造时保存原值，析构恢复；display 由 guard
// 拥有并在恢复完成后关闭（恢复必须发生在连接存活期内）
class ResourceMgrGuard {
public:
    explicit ResourceMgrGuard(Display* d) : d_(d) {
        root_ = DefaultRootWindow(d_);
        prop_ = XInternAtom(d_, "RESOURCE_MANAGER", False);
        unsigned char* data = nullptr;
        if (XGetWindowProperty(d_, root_, prop_, 0, 8192, False, AnyPropertyType,
                               &type_, &format_, &n_, &after_, &data) == Success && data) {
            orig_.assign(data, data + n_);
            XFree(data);
        }
    }
    ~ResourceMgrGuard() {
        if (!orig_.empty()) {
            XChangeProperty(d_, root_, prop_, type_, format_, PropModeReplace,
                            orig_.data(), static_cast<int>(orig_.size()));
        } else {
            XDeleteProperty(d_, root_, prop_);
        }
        XSync(d_, False);
        XCloseDisplay(d_);
    }

    ResourceMgrGuard(const ResourceMgrGuard&) = delete;
    ResourceMgrGuard& operator=(const ResourceMgrGuard&) = delete;

    void set(const char* text) {
        XChangeProperty(d_, root_, prop_, XA_STRING, 8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(text),
                        static_cast<int>(std::strlen(text)));
        XSync(d_, False);
    }

private:
    Display* d_;
    Window root_ = 0;
    Atom prop_ = 0;
    Atom type_ = 0;
    int format_ = 0;
    unsigned long n_ = 0;
    unsigned long after_ = 0;
    std::vector<unsigned char> orig_;
};

// 坏 DISPLAY RAII：指向 probe 范围（20..90）之外、本机不会存在的 display
class BrokenDisplayGuard {
public:
    BrokenDisplayGuard() {
        if (const char* old = ::getenv("DISPLAY")) old_ = old;
        ::setenv("DISPLAY", ":91", 1);
    }
    ~BrokenDisplayGuard() {
        if (old_) {
            ::setenv("DISPLAY", old_->c_str(), 1);
        } else {
            ::unsetenv("DISPLAY");
        }
    }

    BrokenDisplayGuard(const BrokenDisplayGuard&) = delete;
    BrokenDisplayGuard& operator=(const BrokenDisplayGuard&) = delete;

private:
    std::optional<std::string> old_;
};

} // namespace

TEST_F(X11PlatformTest, ScreenMonitorQueryFallbacks) {
    auto screen = wingman::platform::createPlatformScreen();
    ASSERT_NE(screen, nullptr);

    // 越界坐标不落任何显示器 → XRRGetMonitors 遍历空手 return 0（237-238）
    EXPECT_EQ(screen->getMonitorFromPoint({999999, 999999}), 0);
    // 非零死句柄 → XGetWindowAttributes 失败 → 0（246；hwnd==0 已由
    // ScreenMonitorFromPointAndWindow 覆盖，这里是同行的另一分支）
    EXPECT_EQ(screen->getMonitorFromWindow(0xDEADBEEF), 0);
}

TEST_F(X11PlatformTest, ScreenDpiResourceManagerChain) {
    X11ServerLockGuard x11Lock;
    Display* d = openX11Display(nullptr);
    ASSERT_NE(d, nullptr);
    ResourceMgrGuard guard(d);

    // 变体 1：Xft.dpi 资源命中 → atoi 直返（114-121）
    guard.set("Xft.dpi: 112\n");
    {
        auto screen = wingman::platform::createPlatformScreen();
        ASSERT_NE(screen, nullptr);
        EXPECT_EQ(screen->getDpi(0), 112);
    }

    // 变体 2：资源库可解析但无 Xft.dpi → GetResource 失败，销毁 db 后
    // 落屏幕物理尺寸换算（123-130）
    guard.set("custom.key: value\n");
    {
        auto screen = wingman::platform::createPlatformScreen();
        ASSERT_NE(screen, nullptr);
        const int dpi = screen->getDpi(0);
        EXPECT_GT(dpi, 0);
        EXPECT_LE(dpi, 960);
    }
}

TEST_F(X11PlatformTest, ScreenStaticApiFailsGracefullyWithoutDisplay) {
    // 坏 DISPLAY：静态门面的全部捕获/查询失败分支优雅呈现
    // （screen.cpp 854/867-869/878-880/887-889/896-898/917-919 +
    // x11_screen initialize 失败 20-21）
    BrokenDisplayGuard broken;

    auto screen = wingman::platform::createPlatformScreen();
    ASSERT_NE(screen, nullptr);
    EXPECT_FALSE(screen->getBackendInfo().isInitialized);

    EXPECT_EQ(wingman::Screen::capture(), nullptr);
    EXPECT_EQ(wingman::Screen::capture(wingman::Rect{1, 1, 5, 5}), nullptr);
    const auto px = wingman::Screen::getPixel(3, 4);
    EXPECT_EQ(px.r, 0);
    EXPECT_EQ(px.g, 0);
    EXPECT_EQ(px.b, 0);
    wingman::Point pt;
    EXPECT_FALSE(wingman::Screen::findColor(
        wingman::Color(1, 2, 3), wingman::Rect{0, 0, 8, 8}, 0, pt));
    EXPECT_TRUE(wingman::Screen::findColors(
        wingman::Color(1, 2, 3), wingman::Rect{0, 0, 8, 8}, 0).empty());
}

TEST_F(X11PlatformTest, ScreenCaptureEmptyRegionRejectedEarly) {
    // 空区域在创建捕获连接之前即拒绝（874-876，与 X 可用性无关）
    EXPECT_EQ(wingman::Screen::capture(wingman::Rect{0, 0, 0, 0}), nullptr);
    EXPECT_EQ(wingman::Screen::capture(wingman::Rect{5, 5, -1, 10}), nullptr);
}


#endif // __linux__
