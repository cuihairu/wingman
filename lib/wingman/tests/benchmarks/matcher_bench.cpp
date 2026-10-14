// PatternMatcher::templateMatch 匹配核基准：脚本 findImage 在 Android 端与
// 触发器路径的匹配实现（桌面 Vision::findImage 走 OpenCV，不经此处）。
// 纯合成 Bitmap、无需显示面——帧/模板尺寸与 vision_bench 的 findImage
// 场景对齐（1600x1000 全屏 vs 32x32 噪声模板），便于两端横向读数。
// 对比请同机同负载成对测（见 docs/benchmarks.md 优化记录）。

#include "bench.hpp"

#include "wingman/screen.hpp"
#include "wingman/vision/image_analyzer.hpp"

#include <cstdint>

using namespace wingman;
using wingman::bench::Bench;

namespace {

// LCG 噪声帧：逐像素确定性填充（与真实 UI 切片同为非纯色，避免
// 未命中扫描里纯色模板的匹配代价失真）
Bitmap noiseBitmap(int w, int h, uint32_t seed) {
    Bitmap bmp(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            seed = seed * 1103515245u + 12345u;
            const auto r = static_cast<uint8_t>((seed >> 16) & 0xFF);
            seed = seed * 1103515245u + 12345u;
            const auto g = static_cast<uint8_t>((seed >> 16) & 0xFF);
            seed = seed * 1103515245u + 12345u;
            const auto b = static_cast<uint8_t>((seed >> 16) & 0xFF);
            bmp.setPixel(x, y, Color(r, g, b));
        }
    }
    return bmp;
}

} // namespace

int main() {
    const int fw = 1600;
    const int fh = 1000;
    Bitmap frame = noiseBitmap(fw, fh, 424242);
    Bitmap tpl = noiseBitmap(32, 32, 999);

    Bench b("matcher");

    b.header("templateMatch 全屏搜索（1600x1000 vs 32x32，未命中最坏路径）");
    Point best;
    b.run("templateMatch full-screen miss", [&] {
        best = Point{};
        (void)wingman::vision::PatternMatcher::templateMatch(frame, tpl, best, Rect{});
    }, 1);

    b.header("templateMatch 区域限定（400x300，等比缩小的真实用法）");
    Bitmap small = noiseBitmap(400, 300, 77);
    b.run("templateMatch 400x300 region miss", [&] {
        best = Point{};
        (void)wingman::vision::PatternMatcher::templateMatch(small, tpl, best, Rect{});
    }, 30);

    std::cout << "\ndone\n";
    return 0;
}
