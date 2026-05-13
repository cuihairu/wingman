#include "wingman/core/component.hpp"

namespace wingman::core {

bool ComponentBase::initialize() {
    ComponentState expected = ComponentState::Uninitialized;
    if (!state_.compare_exchange_strong(expected, ComponentState::Initializing)) {
        // 如果当前状态不是 Uninitialized，检查是否可以重新初始化
        if (expected == ComponentState::Stopped) {
            if (!state_.compare_exchange_strong(expected, ComponentState::Initializing)) {
                return false;
            }
        } else {
            return false;
        }
    }

    if (onInitialize()) {
        state_.store(ComponentState::Ready);
        return true;
    }

    state_.store(ComponentState::Error);
    return false;
}

bool ComponentBase::start() {
    ComponentState expected = ComponentState::Ready;
    if (!state_.compare_exchange_strong(expected, ComponentState::Running)) {
        return false;
    }

    if (onStart()) {
        return true;
    }

    state_.store(ComponentState::Error);
    return false;
}

void ComponentBase::pause() {
    ComponentState expected = ComponentState::Running;
    if (!state_.compare_exchange_strong(expected, ComponentState::Paused)) {
        return;
    }

    onPause();
}

void ComponentBase::resume() {
    ComponentState expected = ComponentState::Paused;
    if (!state_.compare_exchange_strong(expected, ComponentState::Running)) {
        return;
    }

    onResume();
}

void ComponentBase::stop() {
    ComponentState expected = ComponentState::Running;
    if (!state_.compare_exchange_strong(expected, ComponentState::Stopping)) {
        if (expected == ComponentState::Paused) {
            if (!state_.compare_exchange_strong(expected, ComponentState::Stopping)) {
                return;
            }
        } else {
            return;
        }
    }

    onStop();
    state_.store(ComponentState::Stopped);
}

void ComponentBase::shutdown() {
    ComponentState current = state_.load();

    // 如果正在运行或暂停，先停止
    if (current == ComponentState::Running || current == ComponentState::Paused) {
        stop();
        current = state_.load();
    }

    // 只有在停止状态才能关闭
    if (current != ComponentState::Stopped &&
        current != ComponentState::Uninitialized &&
        current != ComponentState::Error) {
        return;
    }

    onShutdown();
    state_.store(ComponentState::Stopped);
}

void ComponentBase::setState(ComponentState state) {
    state_.store(state);
}

} // namespace wingman::core
