#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "screen_module.hpp"
#include "input_module.hpp"
#include "window_module.hpp"

namespace wingman {
namespace script {
namespace modules {

// 前置声明（这些模块的 create 函数在各自 .cpp 中定义，无独立头文件）
ModuleDescriptor createProcessModule();
ModuleDescriptor createUtilModule();
ModuleDescriptor createSystemModule();
ModuleDescriptor createPerformanceModule();
ModuleDescriptor createSecurityModule();
ModuleDescriptor createGameProfileModule();
ModuleDescriptor createVisionModule();
ModuleDescriptor createOcrModule();
ModuleDescriptor createQrcodeModule();
ModuleDescriptor createSmartTriggerModule();
ModuleDescriptor createBehaviorTreeModule();
ModuleDescriptor createNodeModule();
ModuleDescriptor createConfigModule();
ModuleDescriptor createVerificationModule();
ModuleDescriptor createHumanModule();
ModuleDescriptor createDebuggerModule();
ModuleDescriptor createUIAutomationModule();
ModuleDescriptor createHttpModule();
ModuleDescriptor createJsonModule();
ModuleDescriptor createKvModule();
ModuleDescriptor createOrchestrationModule();
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
	modules.push_back(createGameProfileModule());
	modules.push_back(createVisionModule());
	modules.push_back(createOcrModule());
	modules.push_back(createQrcodeModule());
	modules.push_back(createSmartTriggerModule());
	modules.push_back(createBehaviorTreeModule());
	modules.push_back(createNodeModule());
	modules.push_back(createConfigModule());
	modules.push_back(createVerificationModule());
	modules.push_back(createHumanModule());
	modules.push_back(createDebuggerModule());
	modules.push_back(createUIAutomationModule());
	modules.push_back(createHttpModule());
	modules.push_back(createJsonModule());
	modules.push_back(createKvModule());
	modules.push_back(createOrchestrationModule());
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
