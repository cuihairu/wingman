#include "wingman/platform/input_factory.hpp"
#include "wingman/platform/mock_input.hpp"

#include <mutex>

namespace wingman::platform {

#if defined(_WIN32)
namespace win {
std::unique_ptr<IInput> createSendInputInput(const InputConfig& config);
}
#elif defined(__APPLE__)
namespace mac {
std::unique_ptr<IInput> createCGEventInput(const InputConfig& config);
}
#elif defined(__linux__)
namespace linux {
std::unique_ptr<IInput> createXTestInput(const InputConfig& config);
}
#endif

std::unique_ptr<IInput> createDefaultInput(const InputConfig& config) {
#if defined(_WIN32)
    return win::createSendInputInput(config);
#elif defined(__APPLE__)
    return mac::createCGEventInput(config);
#elif defined(__linux__)
    return linux::createXTestInput(config);
#else
    auto input = std::make_unique<mock::MockInput>();
    input->initialize(config);
    return input;
#endif
}

std::shared_ptr<IInput> defaultSharedInput() {
    static std::once_flag initFlag;
    static std::shared_ptr<IInput> input;

    std::call_once(initFlag, [] {
        input = std::shared_ptr<IInput>(createDefaultInput(InputConfig{}));
    });

    return input;
}

std::shared_ptr<IInput> createSharedInput(const InputConfig& config) {
    return std::shared_ptr<IInput>(createDefaultInput(config));
}

} // namespace wingman::platform
