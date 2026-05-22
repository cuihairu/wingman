#include "wingman/script/iscript_engine.hpp"
#include "wingman/game_profile.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createGameProfileModule() {
	ModuleDescriptor mod;
	mod.name = "gameprofile";

	mod.functions.push_back({"load", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromBool(GameProfileManager::instance().loadProfile(args[0].asString()));
	}, "id:string -> bool"});

	mod.functions.push_back({"save", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto* profile = GameProfileManager::instance().getProfile(args[0].asString());
		if (profile) return ScriptValue::fromBool(GameProfileManager::instance().saveProfile(*profile));
		return ScriptValue::fromBool(false);
	}, "id:string -> bool"});

	mod.functions.push_back({"get", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		const auto* profile = GameProfileManager::instance().getProfile(args[0].asString());
		if (profile) {
			return ScriptValue::fromArray({
				ScriptValue::fromString(profile->name),
				ScriptValue::fromString(profile->window.title)
			});
		}
		return ScriptValue::null();
	}, "id:string -> name:string, title:string?"});

	mod.functions.push_back({"getActive", [](const std::vector<ScriptValue>&) -> ScriptValue {
		const auto* profile = GameProfileManager::instance().getActiveProfile();
		if (profile) {
			return ScriptValue::fromArray({
				ScriptValue::fromString(profile->id),
				ScriptValue::fromString(profile->name)
			});
		}
		return ScriptValue::null();
	}, "() -> id:string, name:string?"});

	mod.functions.push_back({"setActive", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromBool(GameProfileManager::instance().setActiveProfile(args[0].asString()));
	}, "id:string -> bool"});

	mod.functions.push_back({"list", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto ids = GameProfileManager::instance().getProfileIds();
		std::vector<ScriptValue> arr;
		for (const auto& id : ids) arr.push_back(ScriptValue::fromString(id));
		return ScriptValue::fromArray(std::move(arr));
	}, "() -> {string}"});

	mod.functions.push_back({"findByWindow", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto* profile = GameProfileManager::instance().findProfileByWindow(args[0].asString());
		if (profile) {
			return ScriptValue::fromArray({
				ScriptValue::fromString(profile->id),
				ScriptValue::fromString(profile->name)
			});
		}
		return ScriptValue::null();
	}, "title:string -> id:string, name:string?"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
