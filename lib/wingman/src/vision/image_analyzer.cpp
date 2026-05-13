#include "wingman/vision/image_analyzer.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace wingman::vision {

// ========== ImageAnalyzer ==========

Rect ImageAnalyzer::getSearchRegion(const Bitmap& bitmap, const Rect& region) {
    if (region.isEmpty()) {
        return Rect{0, 0, bitmap.width, bitmap.height};
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
    (void)bitmap;
    (void)templatePath;
    (void)region;
    (void)threshold;
    spdlog::warn("[ImageAnalyzer] findImage from file not implemented yet");
    return std::nullopt;
}

std::optional<ImageMatch> ImageAnalyzer::findImage(
    const Bitmap& bitmap,
    const Bitmap& templateBitmap,
    const Rect& region,
    double threshold
) {
    Rect searchRegion = getSearchRegion(bitmap, region);

    if (templateBitmap.width > searchRegion.width ||
        templateBitmap.height > searchRegion.height) {
        spdlog::warn("[ImageAnalyzer] Template larger than search region");
        return std::nullopt;
    }

    Point bestPosition;
    double bestScore = PatternMatcher::templateMatch(bitmap, templateBitmap, bestPosition, searchRegion);

    if (bestScore >= threshold) {
        return ImageMatch{
            bestPosition,
            bestScore,
            Rect{bestPosition.x, bestPosition.y, templateBitmap.width, templateBitmap.height}
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
    result.x = std::max(0, std::min(region.x, bitmap.width - 1));
    result.y = std::max(0, std::min(region.y, bitmap.height - 1));
    result.width = std::min(region.width, bitmap.width - result.x);
    result.height = std::min(region.height, bitmap.height - result.y);
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

                if (checkPos.x < 0 || checkPos.x >= bitmap.width ||
                    checkPos.y < 0 || checkPos.y >= bitmap.height) {
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

double PatternMatcher::templateMatch(
    const Bitmap& bitmap,
    const Bitmap& tpl,
    Point& bestPosition,
    const Rect& region
) {
    Rect searchRegion = region.isEmpty() ?
        Rect{0, 0, bitmap.width, bitmap.height} :
        ImageAnalyzer::clampRegion(region, bitmap);

    int maxX = searchRegion.x + searchRegion.width - tpl.width;
    int maxY = searchRegion.y + searchRegion.height - tpl.height;

    if (maxX < searchRegion.x || maxY < searchRegion.y) {
        bestPosition = Point{0, 0};
        return 0.0;
    }

    double bestScore = -1.0;

    for (int y = searchRegion.y; y <= maxY; ++y) {
        for (int x = searchRegion.x; x <= maxX; ++x) {
            double sum = 0.0;
            double sumTemplate = 0.0;
            double sumTarget = 0.0;

            for (int ty = 0; ty < tpl.height; ++ty) {
                for (int tx = 0; tx < tpl.width; ++tx) {
                    Color targetPixel = bitmap.getPixel(x + tx, y + ty);
                    Color templatePixel = tpl.getPixel(tx, ty);

                    double tVal = (templatePixel.r + templatePixel.g + templatePixel.b) / 3.0;
                    double sVal = (targetPixel.r + targetPixel.g + targetPixel.b) / 3.0;

                    sum += tVal * sVal;
                    sumTemplate += tVal * tVal;
                    sumTarget += sVal * sVal;
                }
            }

            double denominator = std::sqrt(sumTemplate) * std::sqrt(sumTarget);
            double score = (denominator > 0) ? (sum / denominator) : 0.0;

            if (score > bestScore) {
                bestScore = score;
                bestPosition = Point{x, y};
            }
        }
    }

    return bestScore;
}

std::optional<Point> PatternMatcher::matchEdge(
    const Bitmap& bitmap,
    const Rect& region,
    bool horizontal,
    int threshold
) {
    Rect searchRegion = region.isEmpty() ?
        Rect{0, 0, bitmap.width, bitmap.height} :
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
