#pragma once

#include "iscript_engine.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace wingman {
namespace script {

// 脚本引擎工厂（自注册模式）
class ScriptEngineFactory {
public:
	using Creator = std::function<std::unique_ptr<IScriptEngine>()>;

	static ScriptEngineFactory& instance() {
		static ScriptEngineFactory factory;
		return factory;
	}

	void registerEngine(const std::string& language, Creator creator) {
		std::lock_guard<std::mutex> lock(mutex_);
		creators_[language] = std::move(creator);
	}

	std::unique_ptr<IScriptEngine> createEngine(const std::string& language) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = creators_.find(language);
		if (it == creators_.end()) return nullptr;
		return it->second();
	}

	std::vector<std::string> getAvailableLanguages() const {
		std::vector<std::string> languages;
		for (const auto& [lang, _] : creators_) {
			languages.push_back(lang);
		}
		return languages;
	}

	std::string detectLanguage(const std::string& path) const {
		auto dotPos = path.rfind('.');
		if (dotPos == std::string::npos) return "lua";
		std::string ext = path.substr(dotPos);
		// 转小写
		for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

		for (const auto& [lang, creator] : creators_) {
			auto engine = creator();
			if (!engine) continue;
			auto exts = engine->getSupportedExtensions();
			for (const auto& e : exts) {
				if (e == ext) return lang;
			}
		}
		return "lua";
	}

private:
	ScriptEngineFactory() = default;
	std::unordered_map<std::string, Creator> creators_;
	mutable std::mutex mutex_;
};

} // namespace script
} // namespace wingman
