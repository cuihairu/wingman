// 视觉基准（需 WINGMAN_ENABLE_VISION 构建 + 可用显示面）：截屏与找色/找图
// 是像素触发与脚本视觉面的热路径。Xvfb 纯色桌面下找色命中与未命中成本
// 不同，这里按「全屏未命中」（最坏扫描）与「已知位置命中」分档量。
// 模板图运行期合成到临时目录（唯一文件名，退出清理）。

#include "bench.hpp"

#include <wingman/screen.hpp>
#include <wingman/vision.hpp>

#include <opencv2/opencv.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace wingman;
using wingman::bench::Bench;

namespace {

// 32x32 噪声模板（纯色模板在未命中扫描里的匹配代价有偏，噪声更接近
// 真实 UI 切片）；写入临时 PNG 供 findImage 读盘。
std::string writeTempTemplate() {
    cv::Mat tpl(32, 32, CV_8UC4);
    cv::randu(tpl, cv::Scalar(0, 0, 0, 255), cv::Scalar(255, 255, 255, 255));
    const auto path = (std::filesystem::temp_directory_path() /
        ("wingman_bench_tpl_" + std::to_string(::getpid()) + ".png")).string();
    cv::imwrite(path, tpl);
    return path;
}

} // namespace

int main() {
    const int w = Screen::getScreenWidth();
    const int h = Screen::getScreenHeight();
    if (w <= 0 || h <= 0) {
        std::cout << "no usable display (set DISPLAY), skipping vision bench\n";
        return 0;
    }
    std::cout << "display: " << w << "x" << h << "\n";

    Bench b("vision");

    b.header("截屏");
    b.run("Screen::capture full screen", [&] {
        auto frame = Screen::capture();
        (void)frame;
    }, 30);

    b.header("找色（全屏）");
    const Color missColor{0x12, 0x34, 0x56, 0xFF};
    const Rect full(0, 0, w, h);
    b.run("findColor full-screen miss (tol=10)", [&] {
        auto hit = Vision::findColor(missColor, 10, full);
        (void)hit;
    }, 50);

    // 桌面像素自身做命中基准：取左上角实际颜色，必然命中
    const auto frame = Screen::capture();
    Color hitColor{0, 0, 0, 255};
    if (frame) {
        const auto pixel = frame->getPixel(0, 0);
        hitColor = Color{pixel.r, pixel.g, pixel.b, 255};
    }
    b.run("findColor full-screen hit (tol=0)", [&] {
        auto hit = Vision::findColor(hitColor, 0, full);
        (void)hit;
    }, 50);

    b.header("找图（全屏）");
    const std::string tpl = writeTempTemplate();
    b.run("findImage 32x32 template full-screen miss", [&] {
        auto match = Vision::findImage(tpl, 0.8);
        (void)match;
    }, 20);
    std::error_code ec;
    std::filesystem::remove(tpl, ec);

    std::cout << "\ndone\n";
    return 0;
}
