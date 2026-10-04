#include <gtest/gtest.h>
#include "test_helpers.hpp" // TempDirFixture：进程专属临时目录，TearDown 递归清理
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include <string>

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

namespace {
const ModuleDescriptor& getModule(const std::string& name) {
    static std::vector<ModuleDescriptor> mods = getAllModules();
    for (auto& mod : mods) {
        if (mod.name == name) return mod;
    }
    return {};
}

const ModuleDescriptor::FunctionEntry* findFunction(const ModuleDescriptor& mod, const std::string& name) {
    for (const auto& f : mod.functions) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

ScriptValue call(const std::string& fn, std::vector<ScriptValue> args = {}) {
    const auto* f = findFunction(getModule("file"), fn);
    EXPECT_NE(f, nullptr) << "missing file." << fn;
    return f ? (*f)(args) : ScriptValue::null();
}

std::string p(const fs::path& dir, const std::string& name = "") {
    return name.empty() ? dir.string() : (dir / name).string();
}
} // anonymous namespace

// ========== 注册与签名 ==========

TEST(FileModuleTest, RegisteredInAllModules) {
    const auto& mod = getModule("file");
    ASSERT_FALSE(mod.name.empty());
    // todo P1-4 钉定：read/write/append/exists/isFile/isDir/size/move/copy/
    // remove/removeAll/mkdir/listDir 十三个函数全部注册
    for (const char* fn : {"read", "write", "append", "exists", "isFile", "isDir",
                           "size", "move", "copy", "remove", "removeAll", "mkdir",
                           "listDir"}) {
        EXPECT_NE(findFunction(mod, fn), nullptr) << fn;
    }
}

// ========== read / write / append ==========

TEST_F(TempDirFixture, WriteReadRoundtripBinarySafe) {
    const std::string content = "line1\n中文行\x00尾" + std::string("\x01\xff", 2);
    EXPECT_TRUE(call("write", {ScriptValue::fromString(p(tempDir(), "a.bin")),
                               ScriptValue::fromString(content)})
                    .asBool());
    auto got = call("read", {ScriptValue::fromString(p(tempDir(), "a.bin"))});
    ASSERT_TRUE(got.isString());
    EXPECT_EQ(got.asString(), content);
}

TEST_F(TempDirFixture, ReadNonexistentReturnsNull) {
    EXPECT_TRUE(call("read", {ScriptValue::fromString(p(tempDir(), "nope.txt"))}).isNull());
    // 参数非法同样 null
    EXPECT_TRUE(call("read").isNull());
}

TEST_F(TempDirFixture, WriteDoesNotAutoCreateParentDirs) {
    EXPECT_FALSE(call("write", {ScriptValue::fromString(p(tempDir() / "missing" / "a.txt")),
                                ScriptValue::fromString("x")})
                     .asBool());
    EXPECT_FALSE(call("exists", {ScriptValue::fromString(p(tempDir() / "missing" / "a.txt"))}).asBool());
}

TEST_F(TempDirFixture, AppendCreatesAndExtends) {
    const auto path = p(tempDir(), "log.txt");
    EXPECT_TRUE(call("append", {ScriptValue::fromString(path), ScriptValue::fromString("a")}).asBool());
    EXPECT_TRUE(call("append", {ScriptValue::fromString(path), ScriptValue::fromString("b")}).asBool());
    EXPECT_EQ(call("read", {ScriptValue::fromString(path)}).asString(), "ab");
}

TEST_F(TempDirFixture, WriteTruncatesExistingContent) {
    const auto path = p(tempDir(), "t.txt");
    call("write", {ScriptValue::fromString(path), ScriptValue::fromString("long-old-content")});
    call("write", {ScriptValue::fromString(path), ScriptValue::fromString("new")});
    EXPECT_EQ(call("read", {ScriptValue::fromString(path)}).asString(), "new");
}

// ========== exists / isFile / isDir / size ==========

TEST_F(TempDirFixture, ExistsIsFileIsDir) {
    const auto file = p(tempDir(), "f.txt");
    const auto dir = p(tempDir(), "d");
    call("write", {ScriptValue::fromString(file), ScriptValue::fromString("x")});
    call("mkdir", {ScriptValue::fromString(dir)});
    EXPECT_TRUE(call("exists", {ScriptValue::fromString(file)}).asBool());
    EXPECT_TRUE(call("exists", {ScriptValue::fromString(dir)}).asBool());
    EXPECT_TRUE(call("isFile", {ScriptValue::fromString(file)}).asBool());
    EXPECT_FALSE(call("isFile", {ScriptValue::fromString(dir)}).asBool());
    EXPECT_TRUE(call("isDir", {ScriptValue::fromString(dir)}).asBool());
    EXPECT_FALSE(call("isDir", {ScriptValue::fromString(file)}).asBool());
    EXPECT_FALSE(call("exists", {ScriptValue::fromString(p(tempDir(), "gone"))}).asBool());
}

TEST_F(TempDirFixture, SizeKnownAndMissing) {
    const auto file = p(tempDir(), "s.txt");
    call("write", {ScriptValue::fromString(file), ScriptValue::fromString("12345")});
    EXPECT_EQ(call("size", {ScriptValue::fromString(file)}).asInt(), 5);
    // 不存在 → nil（0 是合法大小，不能与缺失混淆）
    EXPECT_TRUE(call("size", {ScriptValue::fromString(p(tempDir(), "gone"))}).isNull());
}

// ========== move / copy ==========

TEST_F(TempDirFixture, MoveFileWithinDir) {
    const auto src = p(tempDir(), "m1.txt");
    call("write", {ScriptValue::fromString(src), ScriptValue::fromString("data")});
    EXPECT_TRUE(call("move", {ScriptValue::fromString(src),
                              ScriptValue::fromString(p(tempDir(), "m2.txt"))})
                    .asBool());
    EXPECT_FALSE(call("exists", {ScriptValue::fromString(src)}).asBool());
    EXPECT_EQ(call("read", {ScriptValue::fromString(p(tempDir(), "m2.txt"))}).asString(), "data");
}

TEST_F(TempDirFixture, MoveOntoExistingTargetSucceeds) {
    const auto src = p(tempDir(), "src.txt");
    const auto dst = p(tempDir(), "dst.txt");
    call("write", {ScriptValue::fromString(src), ScriptValue::fromString("new")});
    call("write", {ScriptValue::fromString(dst), ScriptValue::fromString("old")});
    // POSIX rename 直接覆盖；Windows rename 失败后走 copy+remove 回退，殊途同归
    EXPECT_TRUE(call("move", {ScriptValue::fromString(src), ScriptValue::fromString(dst)}).asBool());
    EXPECT_EQ(call("read", {ScriptValue::fromString(dst)}).asString(), "new");
}

TEST_F(TempDirFixture, MoveMissingSourceFails) {
    EXPECT_FALSE(call("move", {ScriptValue::fromString(p(tempDir(), "gone")),
                               ScriptValue::fromString(p(tempDir(), "x"))})
                     .asBool());
}

TEST_F(TempDirFixture, CopyFileOverwriteAndDirectoryRecursive) {
    const auto src = p(tempDir(), "c1.txt");
    call("write", {ScriptValue::fromString(src), ScriptValue::fromString("v1")});
    EXPECT_TRUE(call("copy", {ScriptValue::fromString(src),
                              ScriptValue::fromString(p(tempDir(), "c2.txt"))})
                    .asBool());
    // 覆盖已有目标
    call("write", {ScriptValue::fromString(src), ScriptValue::fromString("v2")});
    EXPECT_TRUE(call("copy", {ScriptValue::fromString(src),
                              ScriptValue::fromString(p(tempDir(), "c2.txt"))})
                    .asBool());
    EXPECT_EQ(call("read", {ScriptValue::fromString(p(tempDir(), "c2.txt"))}).asString(), "v2");

    // 目录递归拷贝
    const auto srcDir = tempDir() / "tree";
    call("mkdir", {ScriptValue::fromString(p(srcDir, "sub"))});
    call("write", {ScriptValue::fromString(p(srcDir / "sub", "leaf.txt")),
                   ScriptValue::fromString("leaf")});
    EXPECT_TRUE(call("copy", {ScriptValue::fromString(srcDir.string()),
                              ScriptValue::fromString((tempDir() / "tree-copy").string())})
                    .asBool());
    EXPECT_EQ(call("read", {ScriptValue::fromString(p(tempDir() / "tree-copy" / "sub" / "leaf.txt"))})
                  .asString(),
              "leaf");
}

// ========== remove / removeAll / mkdir ==========

TEST_F(TempDirFixture, RemoveFileButNotMissing) {
    const auto file = p(tempDir(), "r.txt");
    call("write", {ScriptValue::fromString(file), ScriptValue::fromString("x")});
    EXPECT_TRUE(call("remove", {ScriptValue::fromString(file)}).asBool());
    // 不存在 → false（区别于 removeAll 的幂等 true）
    EXPECT_FALSE(call("remove", {ScriptValue::fromString(file)}).asBool());
}

TEST_F(TempDirFixture, RemoveAllRecursiveAndIdempotent) {
    const auto dir = tempDir() / "nest";
    call("mkdir", {ScriptValue::fromString(p(dir / "a", "b"))});
    call("write", {ScriptValue::fromString(p(dir / "a" / "b", "leaf.txt")),
                   ScriptValue::fromString("x")});
    EXPECT_TRUE(call("removeAll", {ScriptValue::fromString(dir.string())}).asBool());
    // 幂等：路径已不存在仍返回 true
    EXPECT_TRUE(call("removeAll", {ScriptValue::fromString(dir.string())}).asBool());
}

TEST_F(TempDirFixture, MkdirRecursiveAndIdempotent) {
    const auto deep = p(tempDir() / "x" / "y", "z");
    EXPECT_TRUE(call("mkdir", {ScriptValue::fromString(deep)}).asBool());
    EXPECT_TRUE(call("isDir", {ScriptValue::fromString(deep)}).asBool());
    // 已存在返回 true
    EXPECT_TRUE(call("mkdir", {ScriptValue::fromString(deep)}).asBool());
}

// ========== listDir ==========

TEST_F(TempDirFixture, ListDirSortedNamesOnly) {
    for (const char* n : {"b.txt", "a.txt", "c"}) {
        call("write", {ScriptValue::fromString(p(tempDir(), n)), ScriptValue::fromString("x")});
    }
    auto got = call("listDir", {ScriptValue::fromString(tempDir().string())});
    ASSERT_TRUE(got.isArray());
    ASSERT_EQ(got.size(), 3u);
    EXPECT_EQ(got.at(0).asString(), "a.txt");
    EXPECT_EQ(got.at(1).asString(), "b.txt");
    EXPECT_EQ(got.at(2).asString(), "c");
    // 目录路径 → nil；非法参数 → nil
    EXPECT_TRUE(call("listDir", {ScriptValue::fromString(p(tempDir(), "a.txt"))}).isNull());
    EXPECT_TRUE(call("listDir").isNull());
}
