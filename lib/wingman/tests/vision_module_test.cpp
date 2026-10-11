#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h> // getpid
#else
#include <unistd.h> // getpid
#endif

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createVisionModule();

} // namespace modules
} // namespace script
} // namespace wingman

using namespace wingman::script;
using namespace wingman::script::modules;

namespace {

ModuleDescriptor::FunctionEntry findFn(const ModuleDescriptor& mod, const std::string& name) {
	for (const auto& fn : mod.functions) {
		if (fn.name == name) {
			return fn;
		}
	}
	return {};
}

// 临时模板文件助手：登记面校验路径存在，测试用真实落盘文件
struct TempImageFile {
	std::string path;

	explicit TempImageFile(const std::string& tag) {
		path = (std::filesystem::temp_directory_path() /
			("wingman_tpl_" + tag + "_" + std::to_string(::getpid()) + ".bin")).string();
		std::ofstream out(path, std::ios::binary);
		out << "template:" << tag;
	}
	~TempImageFile() {
		std::error_code ec;
		std::filesystem::remove(path, ec);
	}
};

} // namespace

// ========== 具名模板批量管理 ==========

TEST(VisionModuleTest, TemplateRegistryLifecycle) {
	auto mod = createVisionModule();
	auto add = findFn(mod, "templateAdd");
	auto remove = findFn(mod, "templateRemove");
	auto has = findFn(mod, "templateHas");
	auto list = findFn(mod, "templateList");
	ASSERT_FALSE(add.name.empty());
	ASSERT_FALSE(remove.name.empty());
	ASSERT_FALSE(has.name.empty());
	ASSERT_FALSE(list.name.empty());

	TempImageFile fileA("a"), fileB("b");
	const std::string uniqueA = "life_a_" + std::to_string(::getpid());
	const std::string uniqueB = "life_b_" + std::to_string(::getpid());

	// 登记成功；重名拒绝（显式 remove 后再登记）
	ASSERT_TRUE(add({ScriptValue::fromString(uniqueA), ScriptValue::fromString(fileA.path)}).asBool());
	ASSERT_FALSE(add({ScriptValue::fromString(uniqueA), ScriptValue::fromString(fileA.path)}).asBool());
	ASSERT_TRUE(add({ScriptValue::fromString(uniqueB), ScriptValue::fromString(fileB.path)}).asBool());

	ASSERT_TRUE(has({ScriptValue::fromString(uniqueA)}).asBool());
	ASSERT_TRUE(has({ScriptValue::fromString(uniqueB)}).asBool());
	ASSERT_FALSE(has({ScriptValue::fromString("nope")}).asBool());

	// 移除后再登记可成功；移除未知名 false
	ASSERT_TRUE(remove({ScriptValue::fromString(uniqueA)}).asBool());
	ASSERT_FALSE(remove({ScriptValue::fromString(uniqueA)}).asBool());
	ASSERT_FALSE(has({ScriptValue::fromString(uniqueA)}).asBool());
	ASSERT_TRUE(add({ScriptValue::fromString(uniqueA), ScriptValue::fromString(fileA.path)}).asBool());

	// list 全量含已登记项（其余用例的登记也可能在场，只断言包含与形状）
	ScriptValue all = list({});
	ASSERT_TRUE(all.isArray());
	bool foundA = false, foundB = false;
	for (size_t i = 0; i < all.size(); ++i) {
		const ScriptValue& entry = all.at(i);
		const ScriptValue* name = entry.get("name");
		const ScriptValue* path = entry.get("path");
		ASSERT_TRUE(name && name->isString());
		ASSERT_TRUE(path && path->isString());
		if (name->asString() == uniqueA) {
			foundA = true;
			EXPECT_EQ(path->asString(), fileA.path);
		}
		if (name->asString() == uniqueB) foundB = true;
	}
	EXPECT_TRUE(foundA);
	EXPECT_TRUE(foundB);

	// 清场（不污染其他用例的批量输出）
	ASSERT_TRUE(remove({ScriptValue::fromString(uniqueA)}).asBool());
	ASSERT_TRUE(remove({ScriptValue::fromString(uniqueB)}).asBool());
}

TEST(VisionModuleTest, TemplateAddRejectsMissingFile) {
	auto mod = createVisionModule();
	auto add = findFn(mod, "templateAdd");
	auto has = findFn(mod, "templateHas");
	ASSERT_FALSE(add.name.empty());
	ASSERT_FALSE(has.name.empty());

	// 坏路径在登记期拒绝（防拖到识别期才暴露）；参数不足拒绝
	const std::string ghost = "ghost_" + std::to_string(::getpid());
	ASSERT_FALSE(add({ScriptValue::fromString(ghost),
		ScriptValue::fromString("/nonexistent/dir/wingman_tpl_missing.png")}).asBool());
	ASSERT_FALSE(has({ScriptValue::fromString(ghost)}).asBool());
	ASSERT_FALSE(add({ScriptValue::fromString(ghost)}).asBool());
	ASSERT_FALSE(add({}).asBool());
}

// 批量识别：空登记 → 空数组；names 过滤跳过未知名；输出名序确定且条目
// 携带 name/found 键（found 值依截屏内容，不在此断言）
TEST(VisionModuleTest, TemplateFindAllShapeAndFiltering) {
	auto mod = createVisionModule();
	auto add = findFn(mod, "templateAdd");
	auto findAll = findFn(mod, "templateFindAll");
	ASSERT_FALSE(add.name.empty());
	ASSERT_FALSE(findAll.name.empty());

	// 空注册表：批量返回空数组
	ScriptValue empty = findAll({ScriptValue::fromArray({})});
	ASSERT_TRUE(empty.isArray());
	ASSERT_EQ(empty.size(), 0u);

	TempImageFile fileA("fa"), fileB("fb");
	const std::string nameA = "batch_a_" + std::to_string(::getpid());
	const std::string nameB = "batch_b_" + std::to_string(::getpid());
	// 逆序登记，断言输出名序（B 在 A 前）
	ASSERT_TRUE(add({ScriptValue::fromString(nameB), ScriptValue::fromString(fileB.path)}).asBool());
	ASSERT_TRUE(add({ScriptValue::fromString(nameA), ScriptValue::fromString(fileA.path)}).asBool());

	ScriptValue all = findAll({});
	ASSERT_TRUE(all.isArray());
	ASSERT_EQ(all.size(), 2u);
	ASSERT_TRUE(all.at(0).isObject() && all.at(1).isObject());
	const ScriptValue* first = all.at(0).get("name");
	const ScriptValue* second = all.at(1).get("name");
	ASSERT_TRUE(first && first->isString());
	ASSERT_TRUE(second && second->isString());
	EXPECT_LT(first->asString(), second->asString());  // 名序确定
	EXPECT_TRUE(all.at(0).get("found") && all.at(0).get("found")->isBool());
	EXPECT_TRUE(all.at(1).get("found") && all.at(1).get("found")->isBool());

	// names 过滤：未知名跳过，只回已登记项
	ScriptValue filtered = findAll({ScriptValue::fromArray({
		ScriptValue::fromString(nameA),
		ScriptValue::fromString("ghost_unknown")
	})});
	ASSERT_TRUE(filtered.isArray());
	ASSERT_EQ(filtered.size(), 1u);
	const ScriptValue* only = filtered.at(0).get("name");
	ASSERT_TRUE(only && only->isString());
	EXPECT_EQ(only->asString(), nameA);
	EXPECT_TRUE(filtered.at(0).get("found")->isBool());

	// 清场
	auto remove = findFn(mod, "templateRemove");
	ASSERT_TRUE(remove({ScriptValue::fromString(nameA)}).asBool());
	ASSERT_TRUE(remove({ScriptValue::fromString(nameB)}).asBool());
}
