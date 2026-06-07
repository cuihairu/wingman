#include "wingman/script/iscript_engine.hpp"
#include <spdlog/spdlog.h>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace wingman {
namespace script {
namespace modules {

namespace {
	// ========== INI 解析辅助函数 ==========

	// 去除字符串首尾空白
	std::string trim(const std::string& str) {
		size_t start = 0;
		while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
			++start;
		}

		if (start == str.size()) {
			return "";
		}

		size_t end = str.size() - 1;
		while (end > start && std::isspace(static_cast<unsigned char>(str[end]))) {
			--end;
		}

		return str.substr(start, end - start + 1);
	}

	// 检查是否为注释行
	bool isComment(const std::string& str) {
		std::string trimmed = trim(str);
		if (trimmed.empty()) return false;
		char c = trimmed[0];
		return c == ';' || c == '#';
	}

	// 解析 section 名称
	std::string parseSection(const std::string& str) {
		std::string trimmed = trim(str);

		if (trimmed.size() < 2 || trimmed[0] != '[' || trimmed[trimmed.size() - 1] != ']') {
			return "";
		}

		return trim(trimmed.substr(1, trimmed.size() - 2));
	}

	// 解析 key=value 行
	bool parseKeyValue(const std::string& str, std::string& key, std::string& value) {
		std::string trimmed = trim(str);

		// 查找等号
		size_t eqPos = trimmed.find('=');
		if (eqPos == std::string::npos || eqPos == 0) {
			return false;
		}

		key = trim(trimmed.substr(0, eqPos));
		value = trim(trimmed.substr(eqPos + 1));

		return !key.empty();
	}

	// 编码 INI 值（转义特殊字符）
	std::string escapeValue(const std::string& value) {
		std::string result;
		result.reserve(value.size());

		for (char c : value) {
			switch (c) {
				case '\n': result += "\\n"; break;
				case '\r': result += "\\r"; break;
				case '\t': result += "\\t"; break;
				case '\\': result += "\\\\"; break;
				default: result += c; break;
			}
		}

		return result;
	}

	// 解码 INI 值
	std::string unescapeValue(const std::string& value) {
		std::string result;
		result.reserve(value.size());

		for (size_t i = 0; i < value.size(); ++i) {
			if (value[i] == '\\' && i + 1 < value.size()) {
				char next = value[i + 1];
				switch (next) {
					case 'n': result += '\n'; ++i; break;
					case 'r': result += '\r'; ++i; break;
					case 't': result += '\t'; ++i; break;
					case '\\': result += '\\'; ++i; break;
					default: result += value[i]; break;
				}
			} else {
				result += value[i];
			}
		}

		return result;
	}

	// 编码 key（检查合法性）
	std::string escapeKey(const std::string& key) {
		// INI key 通常不需要转义，但需要检查是否包含特殊字符
		// 如果包含 [ ] = ; # 等字符，应该警告或拒绝
		for (char c : key) {
			if (c == '[' || c == ']' || c == '=' || c == ';' || c == '#') {
				spdlog::warn("INI key contains invalid character: '{}'", key);
				// 可以选择拒绝或替换
				// return "";
			}
		}
		return key;
	}

	// 编码 section 名称
	std::string escapeSection(const std::string& section) {
		// section 名称通常不需要转义，但不能包含 ] 或换行
		for (char c : section) {
			if (c == ']' || c == '\n' || c == '\r') {
				spdlog::warn("INI section name contains invalid character: '{}'", section);
				// return "";
			}
		}
		return section;
	}
} // anonymous namespace

// ========== INI 模块函数 ==========

static ScriptValue iniDecode(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::fromObject({});
	}

	std::string content = args[0].asString();

	// 解析结果
	std::unordered_map<std::string, ScriptValue> result;
	std::string currentSection;
	std::unordered_map<std::string, ScriptValue>* currentSectionData = nullptr;

	std::istringstream stream(content);
	std::string line;
	int lineNum = 0;

	while (std::getline(stream, line)) {
		++lineNum;
		std::string trimmed = trim(line);

		// 跳过空行和注释
		if (trimmed.empty() || isComment(trimmed)) {
			continue;
		}

		// 检查是否为 section
		if (trimmed[0] == '[') {
			std::string section = parseSection(trimmed);
			if (section.empty()) {
				spdlog::warn("Invalid INI section at line {}: '{}'", lineNum, line);
				continue;
			}

			currentSection = section;

			// 创建 section 对象
			auto sectionObj = std::unordered_map<std::string, ScriptValue>();
			result[currentSection] = ScriptValue::fromObject(std::move(sectionObj));

			// 获取 section 对象的指针以便后续修改
			auto it = result.find(currentSection);
			if (it != result.end() && it->second.type == ScriptValue::Object) {
				currentSectionData = &it->second.objectVal;
			} else {
				currentSectionData = nullptr;
			}

			continue;
		}

		// 解析 key=value
		std::string key, value;
		if (parseKeyValue(trimmed, key, value)) {
			value = unescapeValue(value);

			if (currentSection.empty()) {
				// 没有 section 的 key 放入全局 section
				if (result.find("") == result.end()) {
					result[""] = ScriptValue::fromObject({});
				}
				auto it = result.find("");
				if (it != result.end() && it->second.type == ScriptValue::Object) {
					it->second.objectVal[key] = ScriptValue::fromString(value);
				}
			} else if (currentSectionData) {
				(*currentSectionData)[key] = ScriptValue::fromString(value);
			}
		} else {
			spdlog::warn("Invalid INI line {}: '{}'", lineNum, line);
		}
	}

	return ScriptValue::fromObject(std::move(result));
}

static ScriptValue iniEncode(const std::vector<ScriptValue>& args) {
	if (args.empty() || args[0].type != ScriptValue::Object) {
		return ScriptValue::fromString("");
	}

	std::ostringstream output;

	// 遍历所有 section
	for (const auto& [sectionName, sectionData] : args[0].objectVal) {
		// 跳过空 section
		if (sectionData.type != ScriptValue::Object) {
			continue;
		}

		// 写入 section header
		if (!sectionName.empty()) {
			output << "[" << escapeSection(sectionName) << "]" << std::endl;
		}

		// 写入 key-value 对
		for (const auto& [key, value] : sectionData.objectVal) {
			std::string valueStr = value.asString();
			output << escapeKey(key) << "=" << escapeValue(valueStr) << std::endl;
		}

		// section 之间空一行
		if (!sectionName.empty()) {
			output << std::endl;
		}
	}

	return ScriptValue::fromString(output.str());
}

static ScriptValue iniGet(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::null();
	}

	// args[0]: INI 数据对象
	// args[1]: section 名称
	// args[2] (可选): key 名称

	if (args[0].type != ScriptValue::Object) {
		return ScriptValue::null();
	}

	std::string section = args[1].asString();
	const ScriptValue* sectionVal = args[0].get(section);

	if (!sectionVal || sectionVal->type != ScriptValue::Object) {
		return ScriptValue::null();
	}

	// 如果只提供 section，返回整个 section 对象
	if (args.size() < 3) {
		return *sectionVal;
	}

	// 如果提供了 key，返回具体值
	std::string key = args[2].asString();
	const ScriptValue* keyVal = sectionVal->get(key);

	if (keyVal) {
		return *keyVal;
	}

	return ScriptValue::null();
}

static ScriptValue iniSet(const std::vector<ScriptValue>& args) {
	if (args.size() < 3) {
		return ScriptValue::null();
	}

	// args[0]: INI 数据对象
	// args[1]: section 名称
	// args[2]: key 名称
	// args[3]: value

	if (args[0].type != ScriptValue::Object) {
		return ScriptValue::null();
	}

	std::string section = args[1].asString();
	std::string key = args[2].asString();
	std::string value = args.size() > 3 ? args[3].asString() : "";

	// 创建修改后的副本
	ScriptValue result = args[0];

	// 获取或创建 section
	const ScriptValue* sectionVal = result.get(section);
	if (!sectionVal) {
		// 创建新 section
		result.objectVal[section] = ScriptValue::fromObject({});
		sectionVal = result.get(section);
	}

	if (sectionVal && sectionVal->type == ScriptValue::Object) {
		// 需要深拷贝 section 对象
		ScriptValue newSection = *sectionVal;
		newSection.objectVal[key] = ScriptValue::fromString(value);
		result.objectVal[section] = newSection;
		return result;
	}

	return ScriptValue::null();
}

static ScriptValue iniDelete(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::fromBool(false);
	}

	// args[0]: INI 数据对象
	// args[1]: section 名称
	// args[2] (可选): key 名称

	if (args[0].type != ScriptValue::Object) {
		return ScriptValue::fromBool(false);
	}

	std::string section = args[1].asString();

	// 创建修改后的副本
	ScriptValue result = args[0];

	// 如果只提供 section，删除整个 section
	if (args.size() < 3) {
		size_t erased = result.objectVal.erase(section);
		return ScriptValue::fromBool(erased > 0);
	}

	// 如果提供了 key，删除 section 中的 key
	const ScriptValue* sectionVal = result.get(section);
	if (sectionVal && sectionVal->type == ScriptValue::Object) {
		std::string key = args[2].asString();
		// 需要深拷贝 section 对象
		ScriptValue newSection = *sectionVal;
		size_t erased = newSection.objectVal.erase(key);
		if (erased > 0) {
			result.objectVal[section] = newSection;
			return result;
		}
	}

	return ScriptValue::fromBool(false);
}

static ScriptValue iniHasSection(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::fromBool(false);
	}

	if (args[0].type != ScriptValue::Object) {
		return ScriptValue::fromBool(false);
	}

	std::string section = args[1].asString();
	return ScriptValue::fromBool(args[0].get(section) != nullptr);
}

static ScriptValue iniHasKey(const std::vector<ScriptValue>& args) {
	if (args.size() < 3) {
		return ScriptValue::fromBool(false);
	}

	if (args[0].type != ScriptValue::Object) {
		return ScriptValue::fromBool(false);
	}

	std::string section = args[1].asString();
	std::string key = args[2].asString();

	const ScriptValue* sectionVal = args[0].get(section);
	if (sectionVal && sectionVal->type == ScriptValue::Object) {
		return ScriptValue::fromBool(sectionVal->get(key) != nullptr);
	}

	return ScriptValue::fromBool(false);
}

static ScriptValue iniSections(const std::vector<ScriptValue>& args) {
	if (args.empty() || args[0].type != ScriptValue::Object) {
		return ScriptValue::fromArray({});
	}

	std::vector<ScriptValue> result;
	for (const auto& [sectionName, _] : args[0].objectVal) {
		result.push_back(ScriptValue::fromString(sectionName));
	}

	return ScriptValue::fromArray(std::move(result));
}

static ScriptValue iniKeys(const std::vector<ScriptValue>& args) {
	if (args.size() < 2 || args[0].type != ScriptValue::Object) {
		return ScriptValue::fromArray({});
	}

	std::string section = args[1].asString();
	const ScriptValue* sectionVal = args[0].get(section);

	if (!sectionVal || sectionVal->type != ScriptValue::Object) {
		return ScriptValue::fromArray({});
	}

	std::vector<ScriptValue> result;
	for (const auto& [key, _] : sectionVal->objectVal) {
		result.push_back(ScriptValue::fromString(key));
	}

	return ScriptValue::fromArray(std::move(result));
}

static ScriptValue iniMerge(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::null();
	}

	// args[0]: 基础 INI 对象
	// args[1]: 要合并的 INI 对象

	if (args[0].type != ScriptValue::Object || args[1].type != ScriptValue::Object) {
		return ScriptValue::null();
	}

	// 创建结果对象（深拷贝基础对象）
	std::unordered_map<std::string, ScriptValue> result = args[0].objectVal;

	// 合并第二个对象
	for (const auto& [sectionName, sectionData] : args[1].objectVal) {
		auto it = result.find(sectionName);

		if (it == result.end()) {
			// section 不存在，直接复制
			result[sectionName] = sectionData;
		} else if (it->second.type == ScriptValue::Object && sectionData.type == ScriptValue::Object) {
			// section 存在，合并 key-value
			for (const auto& [key, value] : sectionData.objectVal) {
				it->second.objectVal[key] = value;
			}
		} else {
			// 类型不匹配，直接覆盖
			result[sectionName] = sectionData;
		}
	}

	return ScriptValue::fromObject(std::move(result));
}

// ========== 模块描述 ==========

ModuleDescriptor createIniModule() {
	ModuleDescriptor mod;
	mod.name = "ini";

	mod.functions.push_back({"decode", iniDecode,
		"content:string -> {section:{key:value}}"});
	mod.functions.push_back({"encode", iniEncode,
		"data:object -> string"});
	mod.functions.push_back({"get", iniGet,
		"data:object, section:string, key?:string -> value|object"});
	mod.functions.push_back({"set", iniSet,
		"data:object, section:string, key:string, value:string -> value"});
	mod.functions.push_back({"delete", iniDelete,
		"data:object, section:string, key?:string -> bool"});
	mod.functions.push_back({"has_section", iniHasSection,
		"data:object, section:string -> bool"});
	mod.functions.push_back({"has_key", iniHasKey,
		"data:object, section:string, key:string -> bool"});
	mod.functions.push_back({"sections", iniSections,
		"data:object -> [string]"});
	mod.functions.push_back({"keys", iniKeys,
		"data:object, section:string -> [string]"});
	mod.functions.push_back({"merge", iniMerge,
		"base:object, override:object -> object"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
