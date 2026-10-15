#include "wingman/vision/image_analyzer.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <vector>

namespace wingman::vision {

// ========== ImageAnalyzer ==========

Rect ImageAnalyzer::getSearchRegion(const Bitmap& bitmap, const Rect& region) {
    if (region.isEmpty()) {
        return Rect{0, 0, bitmap.getWidth(), bitmap.getHeight()};
    }
    return clampRegion(region, bitmap);
}

std::optional<ColorMatch> ImageAnalyzer::findColor(
    const Bitmap& bitmap,
    const Color& target,
    const Rect& region,
    int tolerance
) {
    Rect searchRegion = getSearchRegion(bitmap, region);

    for (int y = searchRegion.y; y < searchRegion.y + searchRegion.height; ++y) {
        for (int x = searchRegion.x; x < searchRegion.x + searchRegion.width; ++x) {
            Color pixel = bitmap.getPixel(x, y);
            if (colorMatches(pixel, target, tolerance)) {
                int distance = colorDistance(pixel, target);
                double confidence = 1.0 - (std::min(distance, 255 * 255) / (255.0 * 255.0));

                return ColorMatch{
                    Point{x, y},
                    pixel,
                    distance,
                    confidence
                };
            }
        }
    }

    return std::nullopt;
}

std::vector<ColorMatch> ImageAnalyzer::findColors(
    const Bitmap& bitmap,
    const Color& target,
    const Rect& region,
    int tolerance,
    int maxCount
) {
    std::vector<ColorMatch> results;
    Rect searchRegion = getSearchRegion(bitmap, region);

    for (int y = searchRegion.y; y < searchRegion.y + searchRegion.height; ++y) {
        for (int x = searchRegion.x; x < searchRegion.x + searchRegion.width; ++x) {
            if (maxCount > 0 && results.size() >= static_cast<size_t>(maxCount)) {
                return results;
            }

            Color pixel = bitmap.getPixel(x, y);
            if (colorMatches(pixel, target, tolerance)) {
                int distance = colorDistance(pixel, target);
                double confidence = 1.0 - (std::min(distance, 255 * 255) / (255.0 * 255.0));

                results.push_back(ColorMatch{
                    Point{x, y},
                    pixel,
                    distance,
                    confidence
                });
            }
        }
    }

    return results;
}

std::optional<ImageMatch> ImageAnalyzer::findImage(
    const Bitmap& bitmap,
    const std::string& templatePath,
    const Rect& region,
    double threshold
) {
    auto templateBitmap = Bitmap::fromFile(templatePath);
    if (!templateBitmap) {
        spdlog::warn("[ImageAnalyzer] Failed to load template image: {}", templatePath);
        return std::nullopt;
    }
    return findImage(bitmap, *templateBitmap, region, threshold);
}

std::optional<ImageMatch> ImageAnalyzer::findImage(
    const Bitmap& bitmap,
    const Bitmap& templateBitmap,
    const Rect& region,
    double threshold
) {
    Rect searchRegion = getSearchRegion(bitmap, region);

    if (templateBitmap.getWidth() > searchRegion.width ||
        templateBitmap.getHeight() > searchRegion.height) {
        spdlog::warn("[ImageAnalyzer] Template larger than search region");
        return std::nullopt;
    }

    Point bestPosition;
    double bestScore = PatternMatcher::templateMatch(bitmap, templateBitmap, bestPosition, searchRegion);

    if (bestScore >= threshold) {
        return ImageMatch{
            bestPosition,
            bestScore,
            Rect{bestPosition.x, bestPosition.y, templateBitmap.getWidth(), templateBitmap.getHeight()}
        };
    }

    return std::nullopt;
}

std::optional<ColorMatch> ImageAnalyzer::findColorFromSource(
    std::shared_ptr<capture::ICaptureSource> source,
    const Color& target,
    const Rect& region,
    int tolerance
) {
    if (!source || !source->isAvailable()) {
        return std::nullopt;
    }

    auto bitmap = source->capture(region);
    if (!bitmap) {
        return std::nullopt;
    }

    return findColor(*bitmap, target, {}, tolerance);
}

void ImageAnalyzer::findColorAsync(
    const Bitmap& bitmap,
    const Color& target,
    const Rect& region,
    int tolerance,
    std::function<void(std::optional<ColorMatch>)> callback
) {
    std::thread([bitmap, target, region, tolerance, callback]() {
        auto result = ImageAnalyzer().findColor(bitmap, target, region, tolerance);
        callback(result);
    }).detach();
}

int ImageAnalyzer::colorDistance(const Color& c1, const Color& c2) {
    int dr = c1.r - c2.r;
    int dg = c1.g - c2.g;
    int db = c1.b - c2.b;
    return dr * dr + dg * dg + db * db;
}

bool ImageAnalyzer::colorMatches(const Color& c1, const Color& c2, int tolerance) {
    return colorDistance(c1, c2) <= tolerance * tolerance;
}

Rect ImageAnalyzer::clampRegion(const Rect& region, const Bitmap& bitmap) {
    Rect result = region;
    result.x = std::max(0, std::min(region.x, bitmap.getWidth() - 1));
    result.y = std::max(0, std::min(region.y, bitmap.getHeight() - 1));
    result.width = std::min(region.width, bitmap.getWidth() - result.x);
    result.height = std::min(region.height, bitmap.getHeight() - result.y);
    return result;
}

// ========== PatternMatcher ==========

std::optional<Point> PatternMatcher::matchColorPattern(
    const Bitmap& bitmap,
    const std::vector<Color>& colors,
    const std::vector<Point>& offsets,
    const Rect& region,
    int tolerance
) {
    if (colors.empty() || colors.size() != offsets.size()) {
        return std::nullopt;
    }

    Rect searchRegion = ImageAnalyzer::clampRegion(region, bitmap);

    Color firstColor = colors[0];
    Point firstOffset = offsets[0];

    for (int y = searchRegion.y; y < searchRegion.y + searchRegion.height; ++y) {
        for (int x = searchRegion.x; x < searchRegion.x + searchRegion.width; ++x) {
            Point basePos{x, y};
            Color pixel = bitmap.getPixel(basePos.x, basePos.y);

            if (!ImageAnalyzer::colorMatches(pixel, firstColor, tolerance)) {
                continue;
            }

            bool allMatch = true;
            for (size_t i = 1; i < colors.size(); ++i) {
                Point checkPos{
                    basePos.x + offsets[i].x - firstOffset.x,
                    basePos.y + offsets[i].y - firstOffset.y
                };

                if (checkPos.x < 0 || checkPos.x >= bitmap.getWidth() ||
                    checkPos.y < 0 || checkPos.y >= bitmap.getHeight()) {
                    allMatch = false;
                    break;
                }

                Color checkPixel = bitmap.getPixel(checkPos.x, checkPos.y);
                if (!ImageAnalyzer::colorMatches(checkPixel, colors[i], tolerance)) {
                    allMatch = false;
                    break;
                }
            }

            if (allMatch) {
                return basePos;
            }
        }
    }

    return std::nullopt;
}

namespace {

// 每次调用现读 WINGMAN_MATCHER_THREADS，不缓存：测试可在用例内逐次切换。
// 返回 -1 表示未设置（走自动档）；0/1 强制串行；N>=2 强制 N 线程并绕过
// 自动档的大小阈值。
int matcherThreadsRequested() {
    const char* raw = std::getenv("WINGMAN_MATCHER_THREADS");
    if (raw == nullptr || raw[0] == '\0') {
        return -1;
    }
    return std::atoi(raw);
}

} // namespace

double PatternMatcher::templateMatch(
    const Bitmap& bitmap,
    const Bitmap& tpl,
    Point& bestPosition,
    const Rect& region
) {
    Rect searchRegion = region.isEmpty() ?
        Rect{0, 0, bitmap.getWidth(), bitmap.getHeight()} :
        ImageAnalyzer::clampRegion(region, bitmap);

    int maxX = searchRegion.x + searchRegion.width - tpl.getWidth();
    int maxY = searchRegion.y + searchRegion.height - tpl.getHeight();

    if (maxX < searchRegion.x || maxY < searchRegion.y) {
        bestPosition = Point{0, 0};
        return 0.0;
    }

    // ===== 常数因子优化（输出位级不变，黄金值回归钉见 image_analyzer_test
    // 的 TemplateMatchGoldenBits）。原实现三处冗余：模板范数在每个搜索
    // 位置重算（不变量）；模板/目标像素经 getPixel（边界检查 + Color
    // 构造）在每个位置重读 w*h 次；亮度重复求值。预计算亮度面后内层
    // 只剩两次 double 载入与乘加；累加顺序与原实现逐项一致（ty 外层 /
    // tx 内层，三个累加器相互独立），保证位级一致。
    const int tw = tpl.getWidth();
    const int th = tpl.getHeight();
    const int bw = bitmap.getWidth();
    const uint8_t* const tdata = tpl.getData();
    const uint8_t* const bdata = bitmap.getData();

    // 模板亮度与范数（原为逐位置不变量，位型：ty 外层 / tx 内层累加）
    std::vector<double> tl(static_cast<size_t>(tw) * th);
    double sumTemplate = 0.0;
    for (int ty = 0; ty < th; ++ty) {
        for (int tx = 0; tx < tw; ++tx) {
            const uint8_t* p = tdata + (static_cast<size_t>(ty) * tw + tx) * 4;
            const double v = (p[2] + p[1] + p[0]) / 3.0;
            tl[static_cast<size_t>(ty) * tw + tx] = v;
            sumTemplate += v * v;
        }
    }
    const double sqrtSumTemplate = std::sqrt(sumTemplate);

    // 搜索区亮度面：每像素读一次（内存 sw*sh*8B，全屏 1600x1000 约
    // 12.8MB，相对匹配成本可忽略）
    const int sw = searchRegion.width;
    const int sh = searchRegion.height;
    std::vector<double> sl(static_cast<size_t>(sw) * sh);
    for (int y = 0; y < sh; ++y) {
        const uint8_t* row =
            bdata + (static_cast<size_t>(searchRegion.y + y) * bw + searchRegion.x) * 4;
        double* out = sl.data() + static_cast<size_t>(y) * sw;
        for (int x = 0; x < sw; ++x) {
            out[x] = (row[x * 4 + 2] + row[x * 4 + 1] + row[x * 4]) / 3.0;
        }
    }

    // ===== 按行分块并行（输出位级不变，回归钉见 image_analyzer_test 的
    // TemplateMatchParallelBitExact）。每个搜索位置的 sum/sumTarget 相互
    // 独立，块内保持原扫描序、块间按块序以严格 > 归约，因此 IEEE-754
    // 结果位与「首个最大位置优先」的平局次序都与串行一致。
    const int scanRows = maxY - searchRegion.y + 1;
    const int scanCols = maxX - searchRegion.x + 1;

    struct Best {
        double score = -1.0;
        int x = 0;   // 搜索区相对坐标
        int y = 0;
    };

    const auto processRows = [&](int yBegin, int yEnd, Best& best) {
        for (int y = yBegin; y < yEnd; ++y) {
            for (int x = 0; x < scanCols; ++x) {
                double sum = 0.0;
                double sumTarget = 0.0;

                for (int ty = 0; ty < th; ++ty) {
                    const double* trow = &tl[static_cast<size_t>(ty) * tw];
                    const double* srow = &sl[static_cast<size_t>(y + ty) * sw + x];
                    for (int tx = 0; tx < tw; ++tx) {
                        sum += trow[tx] * srow[tx];
                        sumTarget += srow[tx] * srow[tx];
                    }
                }

                const double denominator = sqrtSumTemplate * std::sqrt(sumTarget);
                const double score = (denominator > 0) ? (sum / denominator) : 0.0;

                if (score > best.score) {
                    best.score = score;
                    best.x = x;
                    best.y = y;
                }
            }
        }
    };

    Best best;

    const int forced = matcherThreadsRequested();
    int threads = 0;   // 0 = 串行
    if (forced >= 2) {
        threads = forced;
    } else if (forced == 0 || forced == 1) {
        threads = 0;
    } else {
        // forced == -1（未设置）：自动档
        const unsigned hc = std::thread::hardware_concurrency();
        // 阈值：分块与线程启动成本约数十微秒，小画面分块净亏
        if (hc >= 2 && scanRows >= 64 &&
            static_cast<size_t>(scanRows) * scanCols >= 16384) {
            threads = static_cast<int>(hc);
        }
    }

    if (threads <= 1 || scanRows < 2) {
        processRows(0, scanRows, best);
    } else {
        const int bands = std::min(threads, scanRows);
        const int base = scanRows / bands;
        const int extra = scanRows % bands;

        std::vector<Best> partials(bands);
        std::vector<std::thread> workers;
        int y = 0;
        for (int band = 0; band < bands; ++band) {
            const int rows = base + (band < extra ? 1 : 0);
            const int yBegin = y;
            y += rows;
            if (band == 0) {
                // 首块留在调用线程，少建一个线程
                processRows(yBegin, y, partials[0]);
                continue;
            }
            workers.emplace_back([&, band, yBegin, y]() {
                processRows(yBegin, y, partials[band]);
            });
        }
        for (auto& worker : workers) {
            worker.join();
        }
        for (const Best& partial : partials) {
            if (partial.score > best.score) {
                best = partial;
            }
        }
    }

    bestPosition = Point{searchRegion.x + best.x, searchRegion.y + best.y};
    return best.score;
}

std::optional<Point> PatternMatcher::matchEdge(
    const Bitmap& bitmap,
    const Rect& region,
    bool horizontal,
    int threshold
) {
    Rect searchRegion = region.isEmpty() ?
        Rect{0, 0, bitmap.getWidth(), bitmap.getHeight()} :
        ImageAnalyzer::clampRegion(region, bitmap);

    if (horizontal) {
        for (int y = searchRegion.y; y < searchRegion.y + searchRegion.height; ++y) {
            for (int x = searchRegion.x; x < searchRegion.x + searchRegion.width - 1; ++x) {
                Color c1 = bitmap.getPixel(x, y);
                Color c2 = bitmap.getPixel(x + 1, y);

                int diff = std::abs((int)c1.r - c2.r) +
                           std::abs((int)c1.g - c2.g) +
                           std::abs((int)c1.b - c2.b);

                if (diff >= threshold * 3) {
                    return Point{x, y};
                }
            }
        }
    } else {
        for (int x = searchRegion.x; x < searchRegion.x + searchRegion.width; ++x) {
            for (int y = searchRegion.y; y < searchRegion.y + searchRegion.height - 1; ++y) {
                Color c1 = bitmap.getPixel(x, y);
                Color c2 = bitmap.getPixel(x, y + 1);

                int diff = std::abs((int)c1.r - c2.r) +
                           std::abs((int)c1.g - c2.g) +
                           std::abs((int)c1.b - c2.b);

                if (diff >= threshold * 3) {
                    return Point{x, y};
                }
            }
        }
    }

    return std::nullopt;
}

} // namespace wingman::vision
