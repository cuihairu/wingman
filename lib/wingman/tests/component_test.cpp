#include <gtest/gtest.h>
#include "wingman/core/component.hpp"

using namespace wingman::core;

namespace {

class TestComponent : public ComponentBase {
public:
    TestComponent() { setName("TestComponent"); }
    bool initCalled = false;
    bool startCalled = false;
    bool pauseCalled = false;
    bool resumeCalled = false;
    bool stopCalled = false;
    bool shutdownCalled = false;
    bool initResult = true;
    bool startResult = true;

protected:
    bool onInitialize() override {
        initCalled = true;
        return initResult;
    }
    bool onStart() override {
        startCalled = true;
        return startResult;
    }
    void onPause() override { pauseCalled = true; }
    void onResume() override { resumeCalled = true; }
    void onStop() override { stopCalled = true; }
    void onShutdown() override { shutdownCalled = true; }
};

class FailInitComponent : public ComponentBase {
protected:
    bool onInitialize() override { return false; }
};

class FailStartComponent : public ComponentBase {
protected:
    bool onStart() override { return false; }
};

} // anonymous namespace

// ========== Lifecycle Tests ==========

TEST(ComponentTest, FullLifecycle) {
    TestComponent c;

    EXPECT_EQ(c.getState(), ComponentState::Uninitialized);

    ASSERT_TRUE(c.initialize());
    EXPECT_TRUE(c.initCalled);
    EXPECT_EQ(c.getState(), ComponentState::Ready);

    ASSERT_TRUE(c.start());
    EXPECT_TRUE(c.startCalled);
    EXPECT_EQ(c.getState(), ComponentState::Running);

    c.pause();
    EXPECT_TRUE(c.pauseCalled);
    EXPECT_EQ(c.getState(), ComponentState::Paused);

    c.resume();
    EXPECT_TRUE(c.resumeCalled);
    EXPECT_EQ(c.getState(), ComponentState::Running);

    c.stop();
    EXPECT_TRUE(c.stopCalled);
    EXPECT_EQ(c.getState(), ComponentState::Stopped);
}

// ========== Initialize ==========

TEST(ComponentTest, InitializeFromUninitialized) {
    TestComponent c;
    EXPECT_TRUE(c.initialize());
    EXPECT_EQ(c.getState(), ComponentState::Ready);
}

TEST(ComponentTest, InitializeFromStopped) {
    TestComponent c;
    c.initialize();
    c.start();
    c.stop();
    EXPECT_EQ(c.getState(), ComponentState::Stopped);

    EXPECT_TRUE(c.initialize());
    EXPECT_EQ(c.getState(), ComponentState::Ready);
}

TEST(ComponentTest, InitializeFromReadyFails) {
    TestComponent c;
    c.initialize();
    EXPECT_FALSE(c.initialize());
    EXPECT_EQ(c.getState(), ComponentState::Ready);
}

TEST(ComponentTest, InitializeFromRunningFails) {
    TestComponent c;
    c.initialize();
    c.start();
    EXPECT_FALSE(c.initialize());
}

TEST(ComponentTest, FailedInitializeGoesToError) {
    FailInitComponent c;
    EXPECT_FALSE(c.initialize());
    EXPECT_EQ(c.getState(), ComponentState::Error);
}

// ========== Start ==========

TEST(ComponentTest, StartFromReady) {
    TestComponent c;
    c.initialize();
    EXPECT_TRUE(c.start());
    EXPECT_EQ(c.getState(), ComponentState::Running);
}

TEST(ComponentTest, StartFromUninitializedFails) {
    TestComponent c;
    EXPECT_FALSE(c.start());
    EXPECT_EQ(c.getState(), ComponentState::Uninitialized);
}

TEST(ComponentTest, FailedStartGoesToError) {
    FailStartComponent c;
    c.initialize();
    EXPECT_FALSE(c.start());
    EXPECT_EQ(c.getState(), ComponentState::Error);
}

// ========== Pause/Resume ==========

TEST(ComponentTest, PauseOnlyFromRunning) {
    TestComponent c;
    c.initialize();
    c.pause();
    EXPECT_EQ(c.getState(), ComponentState::Ready);
}

TEST(ComponentTest, ResumeOnlyFromPaused) {
    TestComponent c;
    c.initialize();
    c.start();
    c.resume();
    EXPECT_EQ(c.getState(), ComponentState::Running);
}

// ========== Stop ==========

TEST(ComponentTest, StopFromRunning) {
    TestComponent c;
    c.initialize();
    c.start();
    c.stop();
    EXPECT_EQ(c.getState(), ComponentState::Stopped);
}

TEST(ComponentTest, StopFromPaused) {
    TestComponent c;
    c.initialize();
    c.start();
    c.pause();
    c.stop();
    EXPECT_EQ(c.getState(), ComponentState::Stopped);
}

TEST(ComponentTest, StopFromReadyFails) {
    TestComponent c;
    c.initialize();
    c.stop();
    EXPECT_EQ(c.getState(), ComponentState::Ready);
}

// ========== Shutdown ==========

TEST(ComponentTest, ShutdownFromStopped) {
    TestComponent c;
    c.initialize();
    c.start();
    c.stop();
    c.shutdown();
    EXPECT_TRUE(c.shutdownCalled);
}

TEST(ComponentTest, ShutdownFromUninitialized) {
    TestComponent c;
    c.shutdown();
    EXPECT_TRUE(c.shutdownCalled);
}

TEST(ComponentTest, ShutdownStopsRunningComponent) {
    TestComponent c;
    c.initialize();
    c.start();
    c.shutdown();
    EXPECT_TRUE(c.stopCalled);
}

// ========== Query Methods ==========

TEST(ComponentTest, IsReady) {
    TestComponent c;
    EXPECT_FALSE(c.isReady());
    c.initialize();
    EXPECT_TRUE(c.isReady());
    c.start();
    EXPECT_TRUE(c.isReady());
}

TEST(ComponentTest, IsRunning) {
    TestComponent c;
    EXPECT_FALSE(c.isRunning());
    c.initialize();
    c.start();
    EXPECT_TRUE(c.isRunning());
    c.pause();
    EXPECT_FALSE(c.isRunning());
}

TEST(ComponentTest, GetName) {
    TestComponent c;
    EXPECT_EQ(c.getName(), "TestComponent");
}

// ========== ComponentStateName ==========

TEST(ComponentTest, StateNames) {
    EXPECT_STREQ(componentStateName(ComponentState::Uninitialized), "Uninitialized");
    EXPECT_STREQ(componentStateName(ComponentState::Initializing), "Initializing");
    EXPECT_STREQ(componentStateName(ComponentState::Ready), "Ready");
    EXPECT_STREQ(componentStateName(ComponentState::Running), "Running");
    EXPECT_STREQ(componentStateName(ComponentState::Paused), "Paused");
    EXPECT_STREQ(componentStateName(ComponentState::Stopping), "Stopping");
    EXPECT_STREQ(componentStateName(ComponentState::Stopped), "Stopped");
    EXPECT_STREQ(componentStateName(ComponentState::Error), "Error");
}

// ========== ComponentException ==========

TEST(ComponentTest, ExceptionWhat) {
    ComponentException ex("something failed");
    EXPECT_STREQ(ex.what(), "something failed");

    ComponentException ex2("MyComp", "init error");
    std::string msg = ex2.what();
    EXPECT_NE(msg.find("MyComp"), std::string::npos);
    EXPECT_NE(msg.find("init error"), std::string::npos);
}

// ========== Destructor Does Not Throw ==========

TEST(ComponentTest, DestructorDoesNotThrow) {
    {
        TestComponent c;
        c.initialize();
        c.start();
    }
}

// ========== Additional Tests: Boundary State Operations ==========

TEST(ComponentTest, StopFromUninitializedFails) {
    TestComponent c;
    // stop() only works from Running or Paused; from Uninitialized it should be a no-op
    c.stop();
    EXPECT_FALSE(c.stopCalled);
    EXPECT_EQ(c.getState(), ComponentState::Uninitialized);
}

TEST(ComponentTest, StopFromErrorState) {
    FailInitComponent c;
    c.initialize(); // fails -> Error
    EXPECT_EQ(c.getState(), ComponentState::Error);
    // stop() only works from Running/Paused; from Error it should be a no-op
    c.stop();
    EXPECT_EQ(c.getState(), ComponentState::Error);
}

TEST(ComponentTest, ShutdownFromErrorState) {
    FailInitComponent c;
    c.initialize(); // fails -> Error
    EXPECT_EQ(c.getState(), ComponentState::Error);

    c.shutdown();
    // shutdown() handles Error state explicitly
    EXPECT_EQ(c.getState(), ComponentState::Stopped);
}

TEST(ComponentTest, StartStopRestartCycle) {
    TestComponent c;

    // First cycle: initialize -> start -> stop
    ASSERT_TRUE(c.initialize());
    ASSERT_TRUE(c.start());
    EXPECT_EQ(c.getState(), ComponentState::Running);
    c.stop();
    EXPECT_EQ(c.getState(), ComponentState::Stopped);

    // Reinitialize and restart
    EXPECT_TRUE(c.initCalled);
    c.initCalled = false;
    ASSERT_TRUE(c.initialize());
    EXPECT_TRUE(c.initCalled);
    EXPECT_EQ(c.getState(), ComponentState::Ready);

    c.startCalled = false;
    ASSERT_TRUE(c.start());
    EXPECT_TRUE(c.startCalled);
    EXPECT_EQ(c.getState(), ComponentState::Running);

    c.stop();
    EXPECT_EQ(c.getState(), ComponentState::Stopped);
}

TEST(ComponentTest, MultiplePauseResumeCycles) {
    TestComponent c;
    c.initialize();
    c.start();

    for (int i = 0; i < 3; ++i) {
        c.pause();
        EXPECT_EQ(c.getState(), ComponentState::Paused);
        c.resume();
        EXPECT_EQ(c.getState(), ComponentState::Running);
    }

    c.stop();
    EXPECT_EQ(c.getState(), ComponentState::Stopped);
}

TEST(ComponentTest, GetNameDefaultWhenNotSet) {
    // ComponentBase default getName returns empty string when setName is not called
    FailInitComponent c; // does not call setName
    EXPECT_TRUE(c.getName().empty());
}

TEST(ComponentTest, ComponentExceptionCopyConstructor) {
    ComponentException original("test error");
    ComponentException copy(original);
    EXPECT_STREQ(copy.what(), original.what());
}

TEST(ComponentTest, ComponentExceptionWithEmptyComponentName) {
    ComponentException ex("", "something broke");
    std::string msg = ex.what();
    // Should contain the message text regardless of empty component name
    EXPECT_NE(msg.find("something broke"), std::string::npos);
    EXPECT_NE(msg.find("[]"), std::string::npos); // empty brackets from empty name
}
