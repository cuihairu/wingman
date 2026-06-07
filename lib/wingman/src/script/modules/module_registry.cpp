#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "screen_module.hpp"
#include "input_module.hpp"
#include "window_module.hpp"

namespace wingman {
namespace script {
namespace modules {

// Forward declarations (these modules' create functions are defined in their own .cpp files, no separate headers)
ModuleDescriptor createProcessModule();
ModuleDescriptor createUtilModule();
ModuleDescriptor createSystemModule();
ModuleDescriptor createPerformanceModule();
ModuleDescriptor createSecurityModule();
ModuleDescriptor createCryptoModule();
ModuleDescriptor createGameProfileModule();
ModuleDescriptor createVisionModule();
ModuleDescriptor createOcrModule();
ModuleDescriptor createSmartTriggerModule();
ModuleDescriptor createBehaviorTreeModule();
ModuleDescriptor createNodeModule();
ModuleDescriptor createConfigModule();
ModuleDescriptor createVerificationModule();
ModuleDescriptor createHumanModule();
ModuleDescriptor createDebuggerModule();
ModuleDescriptor createUIAutomationModule();
ModuleDescriptor createHttpModule();
ModuleDescriptor createTransportModule();
ModuleDescriptor createJsonModule();
ModuleDescriptor createKvModule();
ModuleDescriptor createEventModule();
ModuleDescriptor createFsmModule();
ModuleDescriptor createTaskModule();
ModuleDescriptor createNotifyModule();
ModuleDescriptor createOrchestrationModule();
ModuleDescriptor createInboxModule();
ModuleDescriptor createTeamModule();

std::vector<ModuleDescriptor> getAllModules() {
	std::vector<ModuleDescriptor> modules;
	modules.push_back(createScreenModule());
	modules.push_back(createInputModule());
	modules.push_back(createWindowModule());
	modules.push_back(createProcessModule());
	modules.push_back(createUtilModule());
	modules.push_back(createSystemModule());
	modules.push_back(createPerformanceModule());
	modules.push_back(createSecurityModule());
	modules.push_back(createCryptoModule());
	modules.push_back(createGameProfileModule());
	modules.push_back(createVisionModule());
	modules.push_back(createOcrModule());
	modules.push_back(createSmartTriggerModule());
	modules.push_back(createBehaviorTreeModule());
	modules.push_back(createNodeModule());
	modules.push_back(createConfigModule());
	modules.push_back(createVerificationModule());
	modules.push_back(createHumanModule());
	modules.push_back(createDebuggerModule());
	modules.push_back(createUIAutomationModule());
	modules.push_back(createHttpModule());
	modules.push_back(createTransportModule());
	modules.push_back(createJsonModule());
	modules.push_back(createKvModule());
	modules.push_back(createEventModule());
	modules.push_back(createFsmModule());
	modules.push_back(createTaskModule());
	modules.push_back(createNotifyModule());
	modules.push_back(createOrchestrationModule());
	modules.push_back(createInboxModule());
	modules.push_back(createTeamModule());
	return modules;
}

void registerAllModules(IScriptEngine& engine) {
	auto modules = getAllModules();
	for (const auto& mod : modules) {
		engine.registerModule(mod);
	}
}

} // namespace modules
} // namespace script
} // namespace wingman
