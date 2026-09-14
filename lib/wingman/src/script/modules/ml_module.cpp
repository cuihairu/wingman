#include "ml_module.hpp"
#include "module_helpers.hpp"
#include "wingman/ml.hpp"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace wingman {
namespace script {
namespace modules {

namespace {

// modelId -> ModelEngine 实例（ID 句柄式，支持同时加载多个模型）。
std::unordered_map<std::string, std::unique_ptr<ModelEngine>> g_models;
std::mutex g_mutex;
std::atomic<uint64_t> g_nextId{1};

ModelEngine* findModel(const std::string& id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_models.find(id);
    return it != g_models.end() ? it->second.get() : nullptr;
}

ScriptValue tensorShapeToArray(const TensorShape& shape) {
    return tensorShapeToScriptValue(shape);
}

ScriptValue ioInfoToArray(const std::vector<std::pair<std::string, TensorShape>>& info) {
    std::vector<ScriptValue> arr;
    arr.reserve(info.size());
    for (const auto& [name, shape] : info) {
        std::unordered_map<std::string, ScriptValue> obj;
        obj["name"] = ScriptValue::fromString(name);
        obj["shape"] = tensorShapeToArray(shape);
        arr.push_back(ScriptValue::fromObject(std::move(obj)));
    }
    return ScriptValue::fromArray(std::move(arr));
}

} // namespace

void cleanupMlModule() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_models.clear();
}

ModuleDescriptor createMlModule() {
    ModuleDescriptor mod;
    mod.name = "ml";

    // providers() -> string[]
    mod.functions.push_back({"providers", [](const std::vector<ScriptValue>&) -> ScriptValue {
        const auto eps = ModelEngine::getAvailableExecutionProviders();
        std::vector<ScriptValue> arr;
        arr.reserve(eps.size());
        for (const auto& ep : eps) {
            arr.push_back(ScriptValue::fromString(ep));
        }
        return ScriptValue::fromArray(std::move(arr));
    }, "() -> string[]"});

    // loadModel(path, ep?) -> modelId | nil
    mod.functions.push_back({"loadModel", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty() || !args[0].isString()) return ScriptValue::null();
        const std::string path = args[0].asString();
        const std::string ep = args.size() > 1 && args[1].isString() ? args[1].asString() : std::string("cpu");

        auto engine = std::make_unique<ModelEngine>();
        if (!engine->loadModel(path, ep)) {
            // 未启用 WINGMAN_ENABLE_ML（走 ml_stub）或模型加载失败时返回 nil
            return ScriptValue::null();
        }
        const auto id = "ml-" + std::to_string(g_nextId.fetch_add(1));
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_models[id] = std::move(engine);
        }
        return ScriptValue::fromString(id);
    }, "path:string, ep?:string=\"cpu\" -> modelId:string | nil"});

    // unload(modelId) -> bool
    mod.functions.push_back({"unload", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.empty()) return ScriptValue::fromBool(false);
        std::lock_guard<std::mutex> lock(g_mutex);
        return ScriptValue::fromBool(g_models.erase(args[0].asString()) > 0);
    }, "modelId:string -> bool"});

    // isLoaded(modelId) -> bool
    mod.functions.push_back({"isLoaded", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* e = !args.empty() ? findModel(args[0].asString()) : nullptr;
        return ScriptValue::fromBool(e && e->isModelLoaded());
    }, "modelId:string -> bool"});

    // inputs(modelId) -> [{name, shape:[int]}]
    mod.functions.push_back({"inputs", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* e = !args.empty() ? findModel(args[0].asString()) : nullptr;
        if (!e) return ScriptValue::fromArray({});
        return ioInfoToArray(e->getInputInfo());
    }, "modelId:string -> array"});

    // outputs(modelId) -> [{name, shape:[int]}]
    mod.functions.push_back({"outputs", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto* e = !args.empty() ? findModel(args[0].asString()) : nullptr;
        if (!e) return ScriptValue::fromArray({});
        return ioInfoToArray(e->getOutputInfo());
    }, "modelId:string -> array"});

    // run(modelId, inputs) -> {success, error, outputs:[{name, shape, data}], timeMs}
    // inputs 每项为 {name, data:[number], shape?:[int], dtype?:"float32"}；
    // 失败（模型不存在、参数不合法、未启用 WINGMAN_ENABLE_ML 走 ml_stub、引擎报错）时
    // 统一返回 success=false + error，脚本侧只需检查 result.success。
    mod.functions.push_back({"run", [](const std::vector<ScriptValue>& args) -> ScriptValue {
        auto fail = [](const std::string& msg) {
            return ScriptValue::fromObject({
                {"success", ScriptValue::fromBool(false)},
                {"error", ScriptValue::fromString(msg)},
                {"outputs", ScriptValue::fromArray({})},
                {"timeMs", ScriptValue::fromFloat(0.0)}
            });
        };
        if (args.size() < 2 || !args[0].isString() || !args[1].isArray()) {
            return fail("usage: run(modelId: string, inputs: [{name, data, shape?, dtype?}])");
        }

        // 先解析张量参数（纯数据变换），再检查模型是否已加载
        std::map<std::string, TensorData> inputs;
        for (const auto& spec : args[1].arrayVal) {
            TensorData tensor;
            std::string error;
            if (!tensorFromScriptValue(spec, tensor, error)) {
                return fail(error);
            }
            const auto* name = spec.get("name");
            if (!name || !name->isString()) {
                return fail("tensor input requires string 'name'");
            }
            inputs[name->asString()] = std::move(tensor);
        }

        auto* e = findModel(args[0].asString());
        if (!e || !e->isModelLoaded()) {
            return fail("model not found or not loaded: " + args[0].asString());
        }

        const auto result = e->run(inputs);
        std::vector<ScriptValue> outputs;
        outputs.reserve(result.outputs.size());
        for (const auto& out : result.outputs) {
            outputs.push_back(modelOutputToScriptValue(out));
        }
        return ScriptValue::fromObject({
            {"success", ScriptValue::fromBool(result.success)},
            {"error", ScriptValue::fromString(result.error)},
            {"outputs", ScriptValue::fromArray(std::move(outputs))},
            {"timeMs", ScriptValue::fromFloat(result.inferenceTimeMs)}
        });
    }, "modelId:string, inputs:[{name, data, shape?, dtype?}] -> {success, error, outputs, timeMs}"});

    return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
