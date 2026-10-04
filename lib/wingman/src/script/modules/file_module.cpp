#include "wingman/script/iscript_engine.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace wingman {
namespace script {
namespace modules {

namespace fs = std::filesystem;

namespace {

// readFileBytes 整文件读入字符串（二进制安全），失败返回 false。
// 用 fs::path 重载的 fstream：MSVC 内部按宽字符打开，非 ASCII 路径
// 安全性与 _wfopen 相同，且公共层无平台宏（P0 边界守卫纪律）。
bool readFileBytes(const fs::path& path, std::string& out) {
	std::error_code ec;
	if (!fs::is_regular_file(path, ec)) {
		return false;
	}
	std::ifstream f(path, std::ios::binary);
	if (!f) {
		return false;
	}
	char buf[64 * 1024];
	while (f.good()) {
		f.read(buf, sizeof(buf));
		out.append(buf, static_cast<size_t>(f.gcount()));
	}
	// 读到 EOF 会置 failbit（正常）；bad 才是不可恢复 IO 错误
	return !f.bad();
}

// writeFileBytes 整体写出（append=false 截断写 / true 追加写），失败返回 false。
bool writeFileBytes(const fs::path& path, const std::string& content, bool append) {
	const auto mode = std::ios::binary | (append ? std::ios::app : std::ios::trunc);
	std::ofstream f(path, mode);
	if (!f) {
		return false;
	}
	if (!content.empty()) {
		f.write(content.data(), static_cast<std::streamsize>(content.size()));
		if (!f) {
			return false;
		}
	}
	f.flush();
	return f.good();
}

} // namespace

// file 模块（todo 2026-10-04 P1-4：常用文件 IO 工具）。
// 基于 std::filesystem 的真实实现（非 stub）：所有系统调用异常都吞掉转
// null/false 返回值，不向脚本引擎抛出。filewatcher 模块提供变更监控腿。
ModuleDescriptor createFileModule() {
	ModuleDescriptor mod;
	mod.name = "file";

	// read(path) -> string | nil
	mod.functions.push_back({"read", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isString()) {
			return ScriptValue::null();
		}
		std::string out;
		if (!readFileBytes(fs::path(args[0].asString()), out)) {
			return ScriptValue::null();
		}
		return ScriptValue::fromString(std::move(out));
	}, "path:string -> string|nil"});

	// write(path, content) -> boolean（截断写；不自动创建父目录，用 mkdir）
	mod.functions.push_back({"write", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
			return ScriptValue::fromBool(false);
		}
		return ScriptValue::fromBool(
			writeFileBytes(fs::path(args[0].asString()), args[1].asString(), false));
	}, "path:string, content:string -> boolean"});

	// append(path, content) -> boolean
	mod.functions.push_back({"append", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
			return ScriptValue::fromBool(false);
		}
		return ScriptValue::fromBool(
			writeFileBytes(fs::path(args[0].asString()), args[1].asString(), true));
	}, "path:string, content:string -> boolean"});

	// exists(path) -> boolean
	mod.functions.push_back({"exists", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isString()) {
			return ScriptValue::fromBool(false);
		}
		std::error_code ec;
		return ScriptValue::fromBool(fs::exists(fs::path(args[0].asString()), ec));
	}, "path:string -> boolean"});

	// isFile(path) -> boolean
	mod.functions.push_back({"isFile", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isString()) {
			return ScriptValue::fromBool(false);
		}
		std::error_code ec;
		return ScriptValue::fromBool(fs::is_regular_file(fs::path(args[0].asString()), ec));
	}, "path:string -> boolean"});

	// isDir(path) -> boolean
	mod.functions.push_back({"isDir", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isString()) {
			return ScriptValue::fromBool(false);
		}
		std::error_code ec;
		return ScriptValue::fromBool(fs::is_directory(fs::path(args[0].asString()), ec));
	}, "path:string -> boolean"});

	// size(path) -> int | nil（不存在时 nil；0 是合法大小）
	mod.functions.push_back({"size", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isString()) {
			return ScriptValue::null();
		}
		std::error_code ec;
		const auto sz = fs::file_size(fs::path(args[0].asString()), ec);
		if (ec) {
			return ScriptValue::null();
		}
		return ScriptValue::fromInt(static_cast<int64_t>(sz));
	}, "path:string -> int|nil"});

	// move(src, dst) -> boolean（rename 优先，跨文件系统回退 copy+remove）
	mod.functions.push_back({"move", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
			return ScriptValue::fromBool(false);
		}
		const fs::path src(args[0].asString());
		const fs::path dst(args[1].asString());
		std::error_code ec;
		fs::rename(src, dst, ec);
		if (!ec) {
			return ScriptValue::fromBool(true);
		}
		// 跨设备（EXDEV 等）rename 失败：copy 覆盖 + 递归删源
		fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
		if (ec) {
			return ScriptValue::fromBool(false);
		}
		fs::remove_all(src, ec);
		return ScriptValue::fromBool(!ec);
	}, "src:string, dst:string -> boolean"});

	// copy(src, dst) -> boolean（目标存在则覆盖；目录递归）
	mod.functions.push_back({"copy", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
			return ScriptValue::fromBool(false);
		}
		std::error_code ec;
		fs::copy(fs::path(args[0].asString()), fs::path(args[1].asString()),
			fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
		return ScriptValue::fromBool(!ec);
	}, "src:string, dst:string -> boolean"});

	// remove(path) -> boolean（仅文件或空目录；递归删用 removeAll）
	mod.functions.push_back({"remove", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isString()) {
			return ScriptValue::fromBool(false);
		}
		std::error_code ec;
		const bool ok = fs::remove(fs::path(args[0].asString()), ec);
		return ScriptValue::fromBool(!ec && ok);
	}, "path:string -> boolean"});

	// removeAll(path) -> boolean（递归删；路径不存在也返回 true 幂等）
	mod.functions.push_back({"removeAll", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isString()) {
			return ScriptValue::fromBool(false);
		}
		std::error_code ec;
		const bool ok = fs::remove_all(fs::path(args[0].asString()), ec) > 0;
		return ScriptValue::fromBool(!ec && (ok || !fs::exists(fs::path(args[0].asString()))));
	}, "path:string -> boolean"});

	// mkdir(path) -> boolean（递归创建；已存在返回 true）
	mod.functions.push_back({"mkdir", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isString()) {
			return ScriptValue::fromBool(false);
		}
		const fs::path p(args[0].asString());
		std::error_code ec;
		if (fs::exists(p, ec)) {
			return ScriptValue::fromBool(!ec);
		}
		fs::create_directories(p, ec);
		return ScriptValue::fromBool(!ec);
	}, "path:string -> boolean"});

	// listDir(path) -> string[] | nil（仅条目名，排序；不含 "." ".."）
	mod.functions.push_back({"listDir", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isString()) {
			return ScriptValue::null();
		}
		const fs::path dir(args[0].asString());
		std::error_code ec;
		if (!fs::is_directory(dir, ec)) {
			return ScriptValue::null();
		}
		std::vector<std::string> names;
		for (const auto& entry : fs::directory_iterator(dir, ec)) {
			names.push_back(entry.path().filename().string());
		}
		if (ec) {
			return ScriptValue::null();
		}
		std::sort(names.begin(), names.end());
		std::vector<ScriptValue> arr;
		arr.reserve(names.size());
		for (auto& n : names) {
			arr.push_back(ScriptValue::fromString(std::move(n)));
		}
		return ScriptValue::fromArray(std::move(arr));
	}, "path:string -> string[]|nil"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
