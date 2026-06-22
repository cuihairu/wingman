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

	// ========== 目录与扫描 ==========
	mod.functions.push_back({"setProfilesDirectory", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		GameProfileManager::instance().setProfilesDirectory(args[0].asString());
		return ScriptValue::fromBool(true);
	}, "dir:string -> bool"});

	mod.functions.push_back({"getProfilesDirectory", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromString(GameProfileManager::instance().getProfilesDirectory());
	}, "() -> string"});

	mod.functions.push_back({"scan", [](const std::vector<ScriptValue>&) -> ScriptValue {
		GameProfileManager::instance().scanProfilesDirectory();
		return ScriptValue::fromBool(true);
	}, "() -> bool"});

	// ========== 模板 ==========
	mod.functions.push_back({"createTemplate", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string id = GameProfileManager::instance().createProfileTemplate(args[0].asString());
		if (id.empty()) return ScriptValue::null();
		const auto* p = GameProfileManager::instance().getProfile(id);
		return ScriptValue::fromArray({
			ScriptValue::fromString(id),
			ScriptValue::fromString(p ? p->name : args[0].asString())
		});
	}, "gameName:string -> id:string, name:string?"});

	// ========== 导入/导出（JSON）==========
	mod.functions.push_back({"exportJson", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string jsonStr = GameProfileManager::instance().exportProfileToJson(args[0].asString());
		return ScriptValue::fromString(jsonStr);
	}, "id:string -> json:string"});

	mod.functions.push_back({"importJson", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string jsonStr = args[0].asString();
		std::string id = args.size() > 1 ? args[1].asString() : "";
		return ScriptValue::fromBool(GameProfileManager::instance().importProfileFromJson(jsonStr, id));
	}, "json:string, id?:string -> bool"});

	// ========== 导入/导出（包文件）==========
	mod.functions.push_back({"exportPackage", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromBool(
			GameProfileManager::instance().exportProfilePackage(args[0].asString(), args[1].asString()));
	}, "id:string, outputPath:string -> bool"});

	mod.functions.push_back({"importPackage", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromBool(
			GameProfileManager::instance().importProfilePackage(args[0].asString()));
	}, "packagePath:string -> bool"});

	// ========== 删除 ==========
	mod.functions.push_back({"delete", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromBool(GameProfileManager::instance().deleteProfile(args[0].asString()));
	}, "id:string -> bool"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
