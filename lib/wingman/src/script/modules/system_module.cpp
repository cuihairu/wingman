#include "wingman/script/iscript_engine.hpp"
#include "wingman/system.hpp"

namespace wingman {
namespace script {
namespace modules {

static ScriptValue fromCpuInfo(const CpuInfo& info) {
	return ScriptValue::fromObject({
		{"vendor", ScriptValue::fromString(info.vendor)},
		{"brand", ScriptValue::fromString(info.brand)},
		{"cores", ScriptValue::fromInt(info.cores)},
		{"threads", ScriptValue::fromInt(info.threads)},
		{"maxClock", ScriptValue::fromInt(info.maxClock)},
		{"currentClock", ScriptValue::fromInt(info.currentClock)},
		{"usage", ScriptValue::fromFloat(info.usage)},
		{"temperature", ScriptValue::fromInt(info.temperature)}
	});
}

static ScriptValue fromMemoryInfo(const MemoryInfo& info) {
	return ScriptValue::fromObject({
		{"total", ScriptValue::fromInt(static_cast<int64_t>(info.total))},
		{"available", ScriptValue::fromInt(static_cast<int64_t>(info.available))},
		{"used", ScriptValue::fromInt(static_cast<int64_t>(info.used))},
		{"usage", ScriptValue::fromFloat(info.usage)}
	});
}

static ScriptValue fromDiskInfo(const DiskInfo& info) {
	return ScriptValue::fromObject({
		{"drive", ScriptValue::fromString(info.drive)},
		{"total", ScriptValue::fromInt(static_cast<int64_t>(info.total))},
		{"free", ScriptValue::fromInt(static_cast<int64_t>(info.free))},
		{"used", ScriptValue::fromInt(static_cast<int64_t>(info.used))},
		{"usage", ScriptValue::fromFloat(info.usage)},
		{"fileSystem", ScriptValue::fromString(info.fileSystem)}
	});
}

static ScriptValue fromGpuInfo(const GpuInfo& info) {
	return ScriptValue::fromObject({
		{"name", ScriptValue::fromString(info.name)}
	});
}

static ScriptValue fromOsInfo(const OsInfo& info) {
	return ScriptValue::fromObject({
		{"platform", ScriptValue::fromString(info.platform)},
		{"version", ScriptValue::fromString(info.version)},
		{"build", ScriptValue::fromString(info.build)},
		{"architecture", ScriptValue::fromString(info.architecture)},
		{"computerName", ScriptValue::fromString(info.computerName)},
		{"userName", ScriptValue::fromString(info.userName)}
	});
}

ModuleDescriptor createSystemModule() {
	ModuleDescriptor mod;
	mod.name = "system";

	mod.functions.push_back({"getCpuInfo", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return fromCpuInfo(System::getCpuInfo());
	}, "() -> {vendor,brand,cores,threads,...}"});

	mod.functions.push_back({"getCpuUsage", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromInt(System::getCpuUsage());
	}, "() -> int"});

	mod.functions.push_back({"getMemoryInfo", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return fromMemoryInfo(System::getMemoryInfo());
	}, "() -> {total,available,used,usage}"});

	mod.functions.push_back({"getDiskInfo", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() > 0 && !args[0].asString().empty()) {
			return fromDiskInfo(System::getDiskInfo(args[0].asString()));
		}
		auto disks = System::getDiskInfo();
		std::vector<ScriptValue> arr;
		for (const auto& d : disks) arr.push_back(fromDiskInfo(d));
		return ScriptValue::fromArray(std::move(arr));
	}, "drive:string? -> {drive,total,free,...} or {{drive,...}}"});

	mod.functions.push_back({"getGpuInfo", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto gpus = System::getGpuInfo();
		std::vector<ScriptValue> arr;
		for (const auto& g : gpus) arr.push_back(fromGpuInfo(g));
		return ScriptValue::fromArray(std::move(arr));
	}, "() -> {{name}}"});

	mod.functions.push_back({"getOsInfo", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return fromOsInfo(System::getOsInfo());
	}, "() -> {platform,version,build,...}"});

	mod.functions.push_back({"getNetworkAdapters", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto adapters = System::getNetworkAdapters();
		std::vector<ScriptValue> arr;
		for (const auto& a : adapters) {
			arr.push_back(ScriptValue::fromObject({
				{"name", ScriptValue::fromString(a.name)},
				{"description", ScriptValue::fromString(a.description)},
				{"macAddress", ScriptValue::fromString(a.macAddress)},
				{"ipAddress", ScriptValue::fromString(a.ipAddress)},
				{"isUp", ScriptValue::fromBool(a.isUp)}
			}));
		}
		return ScriptValue::fromArray(std::move(arr));
	}, "() -> {{name,description,...}}"});

	mod.functions.push_back({"getDisplayInfo", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto displays = System::getDisplayInfo();
		std::vector<ScriptValue> arr;
		for (const auto& d : displays) {
			arr.push_back(ScriptValue::fromObject({
				{"index", ScriptValue::fromInt(d.index)},
				{"name", ScriptValue::fromString(d.name)},
				{"width", ScriptValue::fromInt(d.width)},
				{"height", ScriptValue::fromInt(d.height)},
				{"refreshRate", ScriptValue::fromInt(d.refreshRate)},
				{"isPrimary", ScriptValue::fromBool(d.isPrimary)}
			}));
		}
		return ScriptValue::fromArray(std::move(arr));
	}, "() -> {{index,name,width,...}}"});

	mod.functions.push_back({"getUptime", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromInt(System::getUptime());
	}, "() -> int"});

	mod.functions.push_back({"getDateTime", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromString(System::getDateTime());
	}, "() -> string"});

	mod.functions.push_back({"getTimeZone", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromString(System::getTimeZone());
	}, "() -> string"});

	mod.functions.push_back({"getProcessCount", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromInt(System::getProcessCount());
	}, "() -> int"});

	mod.functions.push_back({"getThreadCount", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromInt(System::getThreadCount());
	}, "() -> int"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
