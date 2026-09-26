/**
 * PythonScriptEngine 基本生命周期测试（真实嵌入式 CPython）。
 *
 * core_tests 侧已有「ScriptManager 驱动 .py 脚本 + stdout/stderr 路由到输出
 * 回调」的用例；这里补引擎自身的面：工厂注册、执行、callFunction 的参数/
 * 返回值 marshal、globals 读写、错误上报、沙箱 import 封禁。
 * 与 Lua 引擎用例对齐的口径：不 mock 解释器，跑真代码。
 */
#include <gtest/gtest.h>
#include <pybind11/embed.h>
#include <chrono>
#include <fstream>
#include <future>
#include <memory>

#include "wingman/python/python_script_engine.hpp"
#include "wingman/script/script_engine_factory.hpp"

namespace py = pybind11;
using wingman::script::EngineConfig;
using wingman::script::ScriptEngineFactory;
using wingman::script::ScriptValue;
using wingman::python::PythonScriptEngine;

namespace {

// 进程级解释器：整个可执行文件只起一次，engine.initialize 里的
// Py_Initialize 对已初始化解释器是幂等的。pybind11 3.0 的
// scoped_interpreter 构造完成时即已释放 GIL（实测：构造后再
// PyEval_SaveThread 会 fatal），引擎契约「公共方法返回后调用线程不持有
// GIL」天然成立，工作线程用例（PythonThreadingTest）依赖这一点
class PythonInterpreterEnvironment : public ::testing::Environment {
public:
	void SetUp() override {
		interpreter_ = std::make_unique<py::scoped_interpreter>();
	}
	void TearDown() override {
		if (interpreter_) {
			// Py_Finalize 要求调用线程持有 GIL：先取回再析构。
			// finalize 之后解释器已死，不能再 PyGILState_Release——
			// 故意不配对释放，进程随后即退出
			PyGILState_STATE state = PyGILState_Ensure();
			(void)state;
			interpreter_.reset();
		}
	}

private:
	std::unique_ptr<py::scoped_interpreter> interpreter_;
};

class PythonEngineTest : public ::testing::Test {
protected:
	void SetUp() override {
		wingman::python::registerPythonEngine();
		auto created = ScriptEngineFactory::instance().createEngine("python");
		ASSERT_NE(created, nullptr);
		engine_ = std::unique_ptr<PythonScriptEngine>(
			static_cast<PythonScriptEngine*>(created.release()));
		ASSERT_TRUE(engine_->initialize(EngineConfig{}));
	}

	void TearDown() override {
		if (engine_) {
			engine_->shutdown();
		}
	}

	std::unique_ptr<PythonScriptEngine> engine_;
};

} // namespace

// ========== 元信息 ==========

TEST_F(PythonEngineTest, LanguageMetadata) {
	EXPECT_EQ(engine_->getLanguageName(), "python");
	const auto exts = engine_->getSupportedExtensions();
	ASSERT_EQ(exts.size(), 1u);
	EXPECT_EQ(exts[0], ".py");
}

// ========== 执行 ==========

TEST_F(PythonEngineTest, ExecuteStringRunsCode) {
	EXPECT_TRUE(engine_->executeString("value = 6 * 7"));
	EXPECT_TRUE(engine_->getLastError().empty());
}

TEST_F(PythonEngineTest, SyntaxErrorReportsFalseAndLastError) {
	EXPECT_FALSE(engine_->executeString("def broken(:\n    pass"));
	EXPECT_FALSE(engine_->getLastError().empty());
	// 出错后引擎仍可继续执行（不崩、不留毒）
	EXPECT_TRUE(engine_->executeString("ok = True"));
}

TEST_F(PythonEngineTest, GlobalsRoundTrip) {
	ScriptValue in = ScriptValue::fromString("hello");
	engine_->setGlobal("greeting", in);

	auto out = engine_->getGlobal("greeting");
	EXPECT_TRUE(out.isString());
	EXPECT_EQ(out.strVal, "hello");

	// 脚本侧改 globals，C++ 侧读回
	EXPECT_TRUE(engine_->executeString("greeting = greeting + ' world'"));
	auto updated = engine_->getGlobal("greeting");
	EXPECT_EQ(updated.strVal, "hello world");

	// 不存在的 global 返回 null 而非报错
	EXPECT_TRUE(engine_->getGlobal("no_such_global").isNull());
}

// ========== 函数调用（参数与返回值走完整 marshal 链路） ==========

TEST_F(PythonEngineTest, CallFunctionScalars) {
	ASSERT_TRUE(engine_->executeString("def add(a, b):\n    return a + b\n"));

	ScriptValue result;
	ASSERT_TRUE(engine_->callFunction("add", {ScriptValue::fromInt(20), ScriptValue::fromInt(3)}, result));
	EXPECT_TRUE(result.isInt());
	EXPECT_EQ(result.intVal, 23);
}

TEST_F(PythonEngineTest, CallFunctionContainerRoundTrip) {
	ASSERT_TRUE(engine_->executeString(
		"def describe(items):\n"
		"    return {'count': len(items), 'first': items[0], 'tags': [str(i) for i in items]}\n"));

	ScriptValue arg = ScriptValue::fromArray(
		{ScriptValue::fromInt(1), ScriptValue::fromInt(2), ScriptValue::fromInt(3)});
	ScriptValue result;
	ASSERT_TRUE(engine_->callFunction("describe", {arg}, result));

	ASSERT_TRUE(result.isObject());
	EXPECT_EQ(result.get("count")->intVal, 3);
	EXPECT_EQ(result.get("first")->intVal, 1);
	ASSERT_TRUE(result.get("tags")->isArray());
	EXPECT_EQ(result.get("tags")->arrayVal[2].strVal, "3");
}

TEST_F(PythonEngineTest, CallFunctionMissingReportsError) {
	ScriptValue result;
	EXPECT_FALSE(engine_->callFunction("no_such_function", {}, result));
	EXPECT_NE(engine_->getLastError().find("Function not found"), std::string::npos);
}

TEST_F(PythonEngineTest, CallFunctionPythonExceptionReportsError) {
	ASSERT_TRUE(engine_->executeString("def explode():\n    raise RuntimeError('kaput')\n"));

	ScriptValue result;
	EXPECT_FALSE(engine_->callFunction("explode", {}, result));
	EXPECT_FALSE(engine_->getLastError().empty());
}

// ========== 沙箱 ==========

// 诊断+锁行为：沙箱引擎的 __builtins__ 必须是白名单 dict（getGlobal 走
// toScriptValue：dict → Object；若还是完整 builtins 模块则得到 Null）
TEST(PythonSandboxTest, SandboxedBuiltinsExposeWhitelistOnly) {
	PythonScriptEngine engine;
	EngineConfig cfg;
	cfg.sandboxed = true;
	ASSERT_TRUE(engine.initialize(cfg));

	auto bi = engine.getGlobal("__builtins__");
	ASSERT_TRUE(bi.isObject()) << "__builtins__ 不是白名单 dict——沙箱没有生效";
	EXPECT_EQ(bi.get("__import__"), nullptr);
	EXPECT_EQ(bi.get("open"), nullptr);
	EXPECT_NE(bi.get("len"), nullptr);
	EXPECT_NE(bi.get("print"), nullptr);
}

TEST(PythonSandboxTest, SandboxedEngineBlocksImportAndDangerousBuiltins) {
	PythonScriptEngine engine;
	EngineConfig cfg;
	cfg.sandboxed = true;
	ASSERT_TRUE(engine.initialize(cfg));

	// print/len 等白名单内建仍可用
	EXPECT_TRUE(engine.executeString("safe = len('abc')"));
	auto safe = engine.getGlobal("safe");
	EXPECT_EQ(safe.intVal, 3);

	// import 被封：builtins 里的 __import__ 已被剥掉
	EXPECT_FALSE(engine.executeString("import os"));
	EXPECT_FALSE(engine.getLastError().empty());

	// open 不在白名单
	EXPECT_FALSE(engine.executeString("f = open('/etc/passwd')"));
}

TEST(PythonSandboxTest, SandboxedEngineCannotImportOsSoEnvUnreachable) {
	PythonScriptEngine engine;
	EngineConfig cfg;
	cfg.sandboxed = true;
	cfg.env["WINGMAN_TEST_MARKER"] = "leaked";
	ASSERT_TRUE(engine.initialize(cfg));

	// 沙箱下 import 被整体封禁（__import__ 被剥），env 注入分支也被跳过，
	// 脚本侧没有任何路径摸到 os.environ
	EXPECT_FALSE(engine.executeString("import os"));
}

TEST(PythonEnvTest, NonSandboxedEngineInjectsEnv) {
	PythonScriptEngine engine;
	EngineConfig cfg;
	cfg.env["WINGMAN_TEST_MARKER"] = "present";
	ASSERT_TRUE(engine.initialize(cfg));

	EXPECT_TRUE(engine.executeString("import os"));
	EXPECT_TRUE(engine.executeString("marker = os.environ.get('WINGMAN_TEST_MARKER', '')"));
	auto marker = engine.getGlobal("marker");
	EXPECT_EQ(marker.strVal, "present");
}

// ========== 生命周期 ==========

TEST(PythonLifecycleTest, CallAfterShutdownIsSafeNoop) {
	auto engine = std::make_unique<PythonScriptEngine>();
	ASSERT_TRUE(engine->initialize(EngineConfig{}));
	engine->shutdown();

	// shutdown 后调用必须安全返回失败，而不是段错误
	EXPECT_FALSE(engine->executeString("x = 1"));
	ScriptValue out;
	EXPECT_FALSE(engine->callFunction("f", {}, out));
	EXPECT_TRUE(engine->getGlobal("x").isNull());
	engine->shutdown();  // 二次 shutdown 幂等
}

// ========== 多线程执行（ScriptManager 的真实用法） ==========
//
// 诊断锚点：主线程各调用返回后 GIL 是否被释放（PyGILState_Check）
TEST(PythonThreadingTest, DiagGilReleasedAfterEngineCalls) {
	PythonScriptEngine engine;
	ASSERT_TRUE(engine.initialize(EngineConfig{}));
	RecordProperty("gil_after_init", PyGILState_Check() ? "HELD" : "released");
	EXPECT_FALSE(PyGILState_Check()) << "initialize() 返回后主线程仍持有 GIL";

	EXPECT_TRUE(engine.executeString("diag = 1"));
	EXPECT_FALSE(PyGILState_Check()) << "executeString() 返回后主线程仍持有 GIL";
}

//
// ScriptManager 在 detached std::thread 里跑 executeFile/executeString，
// 引擎初始化却发生在主线程——pybind11 的 gil_scoped_acquire 依赖单个
// 全局 internals.tstate，跨线程交替持锁的路径必须实测。用
// future+wait_for 包住：死锁表现为超时红，而不是挂死整个测试进程。

namespace {
// 在工作线程跑 fn，等最多 timeout 秒；ready=true 表示 fn 已返回（没死锁）
template <typename Fn>
bool runOnWorkerWithTimeout(Fn&& fn, std::chrono::seconds timeout) {
	// 故意泄漏 future：std::async 的 future 析构会等线程结束，死锁场景下
	// dtor 自己也会死锁；测试进程随后即退出，泄漏无碍
	auto* fut = new std::future<std::invoke_result_t<Fn>>(
		std::async(std::launch::async, std::forward<Fn>(fn)));
	return fut->wait_for(timeout) == std::future_status::ready;
}
} // namespace

TEST(PythonThreadingTest, ExecuteStringOnWorkerThreadAfterMainInit) {
	PythonScriptEngine engine;
	ASSERT_TRUE(engine.initialize(EngineConfig{}));
	EXPECT_TRUE(engine.executeString("warm = 1"));  // 主线程先建好一切

	EXPECT_TRUE(runOnWorkerWithTimeout(
		[&] { return engine.executeString("value = 6 * 7"); },
		std::chrono::seconds(10)))
		<< "工作线程 executeString 死锁";
	EXPECT_EQ(engine.getGlobal("value").intVal, 42);
}

TEST(PythonThreadingTest, ExecuteFileOnWorkerThread) {
	PythonScriptEngine engine;
	ASSERT_TRUE(engine.initialize(EngineConfig{}));

	std::string path = std::string(::testing::TempDir()) + "threaded.py";
	{
		std::ofstream f(path);
		f << "result = 21 * 2\n";
	}

	EXPECT_TRUE(runOnWorkerWithTimeout(
		[&] { return engine.executeFile(path); },
		std::chrono::seconds(10)))
		<< "工作线程 executeFile 死锁";
	EXPECT_EQ(engine.getGlobal("result").intVal, 42);
}

TEST(PythonThreadingTest, CallFunctionOnWorkerThread) {
	PythonScriptEngine engine;
	ASSERT_TRUE(engine.initialize(EngineConfig{}));
	ASSERT_TRUE(engine.executeString("def scale(n):\n    return n * 3\n"));

	ScriptValue out;
	EXPECT_TRUE(runOnWorkerWithTimeout(
		[&] {
			ScriptValue r;
			bool ok = engine.callFunction("scale", {ScriptValue::fromInt(5)}, r);
			if (ok) out = r;
			return ok;
		},
		std::chrono::seconds(10)))
		<< "工作线程 callFunction 死锁";
	EXPECT_EQ(out.intVal, 15);
}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	::testing::AddGlobalTestEnvironment(new PythonInterpreterEnvironment());
	return RUN_ALL_TESTS();
}
