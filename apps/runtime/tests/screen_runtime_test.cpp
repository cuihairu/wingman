#include <gtest/gtest.h>

#include "wingman/screen.hpp"

#include <filesystem>

using namespace wingman;

TEST(ScreenRuntimeTest, BitmapSaveWritesFile) {
    Bitmap bmp(2, 2);
    bmp.setPixel(0, 0, Color(255, 0, 0));
    bmp.setPixel(1, 0, Color(0, 255, 0));
    bmp.setPixel(0, 1, Color(0, 0, 255));
    bmp.setPixel(1, 1, Color(255, 255, 255));

    const auto outputPath = std::filesystem::temp_directory_path() / "wingman-runtime-screen-test.png";
    std::error_code ec;
    std::filesystem::remove(outputPath, ec);

    EXPECT_TRUE(bmp.save(outputPath.string()));
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::filesystem::remove(outputPath, ec);
}

#ifdef __APPLE__
TEST(ScreenRuntimeTest, MacScreenBoundsAreAvailable) {
    const Rect bounds = Screen::getScreenBounds();
    EXPECT_GT(bounds.width, 0);
    EXPECT_GT(bounds.height, 0);
    EXPECT_GT(Screen::getScreenWidth(), 0);
    EXPECT_GT(Screen::getScreenHeight(), 0);
}
#endif
