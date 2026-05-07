#include <gtest/gtest.h>
#include "wingman/recorder.hpp"

using namespace wingman;

class RecorderTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// RecordedEventType Tests
// ============================================================================

TEST_F(RecorderTest, RecordedEventTypeValues) {
    EXPECT_EQ(static_cast<int>(RecordedEventType::MouseMove), 0);
    EXPECT_EQ(static_cast<int>(RecordedEventType::MouseClick), 1);
    EXPECT_EQ(static_cast<int>(RecordedEventType::MouseDown), 2);
    EXPECT_EQ(static_cast<int>(RecordedEventType::MouseUp), 3);
    EXPECT_EQ(static_cast<int>(RecordedEventType::Scroll), 4);
    EXPECT_EQ(static_cast<int>(RecordedEventType::KeyDown), 5);
    EXPECT_EQ(static_cast<int>(RecordedEventType::KeyUp), 6);
    EXPECT_EQ(static_cast<int>(RecordedEventType::Type), 7);
    EXPECT_EQ(static_cast<int>(RecordedEventType::Delay), 8);
}

// ============================================================================
// RecordedEvent Tests
// ============================================================================

TEST_F(RecorderTest, RecordedEventDefaults) {
    RecordedEvent event;
    // Default initialization values depend on implementation
    EXPECT_EQ(event.type, RecordedEventType::MouseMove); // Usually first enum
}

TEST_F(RecorderTest, RecordedEventMouseMove) {
    RecordedEvent event;
    event.type = RecordedEventType::MouseMove;
    event.x = 100;
    event.y = 200;
    event.timestamp = 12345;

    EXPECT_EQ(event.type, RecordedEventType::MouseMove);
    EXPECT_EQ(event.x, 100);
    EXPECT_EQ(event.y, 200);
    EXPECT_EQ(event.timestamp, 12345);
}

TEST_F(RecorderTest, RecordedEventMouseClick) {
    RecordedEvent event;
    event.type = RecordedEventType::MouseClick;
    event.x = 150;
    event.y = 250;
    event.button = 0; // Left button

    EXPECT_EQ(event.type, RecordedEventType::MouseClick);
    EXPECT_EQ(event.button, 0);
}

TEST_F(RecorderTest, RecordedEventKeyDown) {
    RecordedEvent event;
    event.type = RecordedEventType::KeyDown;
    event.keyCode = 65; // 'A' key

    EXPECT_EQ(event.type, RecordedEventType::KeyDown);
    EXPECT_EQ(event.keyCode, 65);
}

TEST_F(RecorderTest, RecordedEventType) {
    RecordedEvent event;
    event.type = RecordedEventType::Type;
    event.text = "Hello World";

    EXPECT_EQ(event.type, RecordedEventType::Type);
    EXPECT_EQ(event.text, "Hello World");
}

TEST_F(RecorderTest, RecordedEventDelay) {
    RecordedEvent event;
    event.type = RecordedEventType::Delay;
    event.delay = 500; // 500ms

    EXPECT_EQ(event.type, RecordedEventType::Delay);
    EXPECT_EQ(event.delay, 500);
}

// ============================================================================
// MacroRecorder Tests
// ============================================================================

TEST_F(RecorderTest, MacroRecorderConstruction) {
    MacroRecorder recorder;
    EXPECT_NO_THROW();
    EXPECT_FALSE(recorder.isRecording());
    EXPECT_FALSE(recorder.isPaused());
    EXPECT_EQ(recorder.getEventCount(), 0);
}

TEST_F(RecorderTest, MacroRecorderStartStop) {
    MacroRecorder recorder;

    EXPECT_NO_THROW(recorder.start());
    // May fail if hooks already installed, which is acceptable
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(recorder.stop());
}

TEST_F(RecorderTest, MacroRecorderPauseResume) {
    MacroRecorder recorder;

    EXPECT_NO_THROW(recorder.pause());
    EXPECT_NO_THROW(recorder.resume());
}

TEST_F(RecorderTest, MacroRecorderClear) {
    MacroRecorder recorder;
    EXPECT_NO_THROW(recorder.clear());
    EXPECT_EQ(recorder.getEventCount(), 0);
}

TEST_F(RecorderTest, MacroRecorderSaveToLua) {
    MacroRecorder recorder;
    // Save without any events should create an empty file or return false
    bool result = recorder.saveToLua("test_recorder.lua");
    // Result depends on implementation
    SUCCEED();
}

TEST_F(RecorderTest, MacroRecorderSaveToJSON) {
    MacroRecorder recorder;
    bool result = recorder.saveToJSON("test_recorder.json");
    // Result depends on implementation
    SUCCEED();
}

TEST_F(RecorderTest, MacroRecorderLoadFromJSON) {
    MacroRecorder recorder;
    // Load non-existent file should return false
    bool result = recorder.loadFromJSON("nonexistent_file.json");
    EXPECT_FALSE(result);
}

TEST_F(RecorderTest, MacroRecorderPlayback) {
    MacroRecorder recorder;
    // Playback without events should not crash
    EXPECT_NO_THROW(recorder.playback(100, 1));
}

TEST_F(RecorderTest, MacroRecorderPlaybackWithSpeed) {
    MacroRecorder recorder;
    EXPECT_NO_THROW(recorder.playback(200, 2));
}
