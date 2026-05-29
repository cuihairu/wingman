#include "wingman/core/component.hpp"

namespace wingman::core {

bool ComponentBase::initialize() {
    ComponentState expected = ComponentState::Uninitialized;
    if (!state_.compare_exchange_strong(expected, ComponentState::Initializing)) {
        // If current state is not Uninitialized, check if re-initialization is possible
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

    // If running or paused, stop first
    if (current == ComponentState::Running || current == ComponentState::Paused) {
        stop();
        current = state_.load();
    }

    // Can only close in stopped state
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
