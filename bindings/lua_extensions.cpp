#include "lua_extensions.hpp"
#include "wingman/http.hpp"
#include "wingman/json.hpp"
#include "wingman/kvstore.hpp"

// 条件包含 workflow_orchestrator（仅在构建 server 模块时）
#ifdef WINGMAN_BUILD_SERVER
#include "wingman/server/workflow_orchestrator.hpp"
#include "wingman/server/orchestrator.hpp"
#else
// 存根类型定义（未构建 server 模块时）
namespace wingman::server {
    struct WorkflowOrchestrator;
    struct Orchestrator;
    struct TaskStep { std::string id; std::string name; std::string script; };
    struct Workflow {
        std::string id;
        enum Status { PENDING, RUNNING, COMPLETED, FAILED, CANCELLED };
        Status status = PENDING;
    };
    inline std::string workflowStatusToString(Workflow::Status s) { return "PENDING"; }
    inline std::string stepStatusToString(int s) { return "PENDING"; }
}
#endif

#include <memory>
#include <unordered_map>
#include <chrono>
#include <cmath>

namespace wingman::lua {

// 全局 KeyValueStore 实例
// TODO: Re-enable when kvstore.cpp is properly configured
// static std::unique_ptr<KeyValueStore> g_kvStore;
static std::unique_ptr<HttpClient> g_httpClient;

// 初始化/获取全局 KV Store
static KeyValueStore* getKVStore() {
    return nullptr;  // TODO: Implement when kvstore.cpp is enabled
    /*
    if (!g_kvStore) {
        g_kvStore = std::make_unique<KeyValueStore>();
    }
    return g_kvStore.get();
    */
}

// 初始化/获取全局 HTTP Client
static HttpClient* getHttpClient() {
    if (!g_httpClient) {
        g_httpClient = std::make_unique<HttpClient>();
    }
    return g_httpClient.get();
}

// ============================================================================
// 辅助函数
// ============================================================================

// 从栈获取 HttpOptions
static HttpOptions getHttpOptions(lua_State* L, int index) {
    HttpOptions options;
    if (lua_istable(L, index)) {
        lua_getfield(L, index, "timeout");
        if (lua_isnumber(L, -1)) {
            options.timeout = static_cast<int>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "followRedirects");
        if (lua_isboolean(L, -1)) {
            options.followRedirects = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "maxRedirects");
        if (lua_isnumber(L, -1)) {
            options.maxRedirects = static_cast<size_t>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "headers");
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                const char* key = lua_tostring(L, -2);
                const char* value = lua_tostring(L, -1);
                if (key && value) {
                    options.headers[key] = value;
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }
    return options;
}

// 将 HttpResponse 压入栈
static void pushHttpResponse(lua_State* L, const HttpResponse& response) {
    lua_newtable(L);

    lua_pushinteger(L, response.statusCode);
    lua_setfield(L, -2, "status");

    lua_pushstring(L, response.body.c_str());
    lua_setfield(L, -2, "body");

    lua_pushnumber(L, response.elapsed);
    lua_setfield(L, -2, "elapsed");

    if (!response.error.empty()) {
        lua_pushstring(L, response.error.c_str());
        lua_setfield(L, -2, "error");
    }

    lua_pushboolean(L, response.isSuccess());
    lua_setfield(L, -2, "success");

    // headers
    lua_newtable(L);
    for (const auto& [key, value] : response.headers) {
        lua_pushstring(L, value.c_str());
        lua_setfield(L, -2, key.c_str());
    }
    lua_setfield(L, -2, "headers");
}

// 从栈获取 KvOptions
static KvOptions getKvOptions(lua_State* L, int index) {
    KvOptions options;
    if (lua_istable(L, index)) {
        lua_getfield(L, index, "ttl");
        if (lua_isnumber(L, -1)) {
            options.ttl = static_cast<int64_t>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "nx");
        if (lua_isboolean(L, -1)) {
            options.nx = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "xx");
        if (lua_isboolean(L, -1)) {
            options.xx = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);
    }
    return options;
}

// ============================================================================
// HTTP 模块
// ============================================================================

namespace http {

int get(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    HttpOptions options = getHttpOptions(L, 2);

    HttpResponse response = getHttpClient()->get(url, options);
    pushHttpResponse(L, response);
    return 1;
}

int post(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    const char* body = luaL_optstring(L, 2, "");
    HttpOptions options = getHttpOptions(L, 3);

    HttpResponse response = getHttpClient()->post(url, body, options);
    pushHttpResponse(L, response);
    return 1;
}

int postForm(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);

    std::unordered_map<std::string, std::string> fields;
    if (lua_istable(L, 2)) {
        lua_pushnil(L);
        while (lua_next(L, 2) != 0) {
            const char* key = lua_tostring(L, -2);
            const char* value = lua_tostring(L, -1);
            if (key && value) {
                fields[key] = value;
            }
            lua_pop(L, 1);
        }
    }

    HttpOptions options = getHttpOptions(L, 3);
    HttpResponse response = getHttpClient()->postForm(url, fields, options);
    pushHttpResponse(L, response);
    return 1;
}

int put(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    const char* body = luaL_optstring(L, 2, "");
    HttpOptions options = getHttpOptions(L, 3);

    HttpResponse response = getHttpClient()->put(url, body, options);
    pushHttpResponse(L, response);
    return 1;
}

int del(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    HttpOptions options = getHttpOptions(L, 2);

    HttpResponse response = getHttpClient()->del(url, options);
    pushHttpResponse(L, response);
    return 1;
}

} // namespace http

// ============================================================================
// JSON 模块
// ============================================================================

namespace json {

// 将 JsonValue 转换为 Lua 值
static void pushJsonValue(lua_State* L, const JsonValue& value) {
    switch (value.type()) {
        case JsonType::Null:
            lua_pushnil(L);
            break;
        case JsonType::Boolean:
            lua_pushboolean(L, value.asBool());
            break;
        case JsonType::Number:
            if (value.asString().find('.') != std::string::npos) {
                lua_pushnumber(L, value.asDouble());
            } else {
                lua_pushinteger(L, value.asInt64());
            }
            break;
        case JsonType::String:
            lua_pushstring(L, value.asString().c_str());
            break;
        case JsonType::Array: {
            lua_newtable(L);
            for (size_t i = 0; i < value.size(); ++i) {
                pushJsonValue(L, value.at(i));
                lua_rawseti(L, -2, static_cast<int>(i + 1));
            }
            break;
        }
        case JsonType::Object: {
            lua_newtable(L);
            auto keys = value.keys();
            for (const auto& key : keys) {
                pushJsonValue(L, value.get(key));
                lua_setfield(L, -2, key.c_str());
            }
            break;
        }
    }
}

// 将 Lua 值转换为 JsonValue
static JsonValue luaToJson(lua_State* L, int index) {
    JsonValue result;

    int type = lua_type(L, index);
    switch (type) {
        case LUA_TNIL:
            result = JsonValue(nullptr);
            break;
        case LUA_TBOOLEAN:
            result = JsonValue(lua_toboolean(L, index) != 0);
            break;
        case LUA_TNUMBER: {
            // Lua 5.1 compatible: check if value is an integer
            double num = lua_tonumber(L, index);
            double intPart;
            if (modf(num, &intPart) == 0.0) {
                result = JsonValue(static_cast<int64_t>(num));
            } else {
                result = JsonValue(num);
            }
            break;
        }
        case LUA_TSTRING:
            result = JsonValue(lua_tostring(L, index));
            break;
        case LUA_TTABLE: {
            // 检查是数组还是对象
            bool isArray = true;
            int maxIndex = 0;

            lua_pushnil(L);
            while (lua_next(L, index) != 0) {
                if (lua_type(L, -2) != LUA_TNUMBER) {
                    isArray = false;
                    lua_pop(L, 1);
                    break;
                }
                double num = lua_tonumber(L, -2);
                double intPart;
                if (modf(num, &intPart) != 0.0) {
                    isArray = false;
                    lua_pop(L, 1);
                    break;
                }
                int idx = static_cast<int>(num);
                if (idx < 1 || idx > INT_MAX - 1) {
                    isArray = false;
                    lua_pop(L, 1);
                    break;
                }
                if (idx > maxIndex) maxIndex = idx;
                lua_pop(L, 1);
            }

            if (isArray && maxIndex > 0) {
                result = JsonValue(nullptr);
                for (int i = 1; i <= maxIndex; ++i) {
                    lua_rawgeti(L, index, i);
                    if (lua_isnil(L, -1)) {
                        result.push(JsonValue(nullptr));
                    } else {
                        result.push(luaToJson(L, -1));
                    }
                    lua_pop(L, 1);
                }
            } else {
                result = JsonValue(nullptr);
                lua_pushnil(L);
                while (lua_next(L, index) != 0) {
                    if (lua_type(L, -2) == LUA_TSTRING) {
                        const char* key = lua_tostring(L, -2);
                        result.set(key, luaToJson(L, -1));
                    }
                    lua_pop(L, 1);
                }
            }
            break;
        }
        default:
            result = JsonValue(nullptr);
            break;
    }

    return result;
}

int decode(lua_State* L) {
    const char* jsonStr = luaL_checkstring(L, 1);
    JsonValue value = JsonValue::parse(jsonStr);
    pushJsonValue(L, value);
    return 1;
}

int encode(lua_State* L) {
    JsonValue json = luaToJson(L, 1);
    int indent = luaL_optinteger(L, 2, -1);
    std::string result = json.dump(indent);
    lua_pushstring(L, result.c_str());
    return 1;
}

int null(lua_State* L) {
    lua_pushnil(L);
    return 1;
}

} // namespace json

// ============================================================================
// Orchestration 模块
// ============================================================================

#ifdef WINGMAN_BUILD_SERVER
namespace orchestration {

// 全局 Workflow Orchestrator 实例
static wingman::server::WorkflowOrchestrator* g_workflowOrchestrator = nullptr;

// 设置全局 Orchestrator
void setOrchestrator(wingman::server::WorkflowOrchestrator* orchestrator) {
    g_workflowOrchestrator = orchestrator;
}

// 获取全局 Orchestrator
static wingman::server::WorkflowOrchestrator* getOrchestrator() {
    return g_workflowOrchestrator;
}

// 从 Lua 表构建 TaskStep
static wingman::server::TaskStep parseTaskStep(lua_State* L, int index) {
    wingman::server::TaskStep step;

    if (!lua_istable(L, index)) {
        return step;
    }

    lua_getfield(L, index, "id");
    if (lua_isstring(L, -1)) step.id = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "name");
    if (lua_isstring(L, -1)) step.name = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "script");
    if (lua_isstring(L, -1)) step.script = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "timeout");
    if (lua_isinteger(L, -1)) step.timeout_seconds = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "parameters");
    if (lua_isstring(L, -1)) step.parameters = lua_tostring(L, -1);
    lua_pop(L, 1);

    // 解析 workers 数组
    lua_getfield(L, index, "workers");
    if (lua_istable(L, -1)) {
        int len = lua_objlen(L, -1);
        for (int i = 1; i <= len; i++) {
            lua_rawgeti(L, -1, i);
            if (lua_isstring(L, -1)) {
                step.workers.push_back(lua_tostring(L, -1));
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    // 解析 depends_on 数组
    lua_getfield(L, index, "depends_on");
    if (lua_istable(L, -1)) {
        int len = lua_objlen(L, -1);
        for (int i = 1; i <= len; i++) {
            lua_rawgeti(L, -1, i);
            if (lua_isstring(L, -1)) {
                step.depends_on.push_back(lua_tostring(L, -1));
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    return step;
}

#else
// 存根实现（未构建 server 模块时）
struct WorkflowOrchestrator;
static void* g_orchestrator = nullptr;

void setOrchestrator(void* orchestrator) {
    g_orchestrator = orchestrator;
}

static void* getOrchestrator() {
    return g_orchestrator;
}

// 存根类型
struct TaskStep { };
struct Workflow { };
#endif

// 从 Lua 表构建 TaskStep
static wingman::server::TaskStep parseTaskStep(lua_State* L, int index) {
    wingman::server::TaskStep step;

    if (!lua_istable(L, index)) {
        return step;
    }

    lua_getfield(L, index, "id");
    if (lua_isstring(L, -1)) step.id = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "name");
    if (lua_isstring(L, -1)) step.name = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "script");
    if (lua_isstring(L, -1)) step.script = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "timeout_seconds");
    if (lua_isnumber(L, -1)) step.timeoutSeconds = static_cast<int>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    // 解析 workers 数组
    lua_getfield(L, index, "workers");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_isstring(L, -1)) {
                step.workers.push_back(lua_tostring(L, -1));
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    // 解析 depends_on 数组
    lua_getfield(L, index, "depends_on");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_isstring(L, -1)) {
                step.dependsOn.push_back(lua_tostring(L, -1));
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    // 解析 parameters
    lua_getfield(L, index, "parameters");
    if (!lua_isnil(L, -1)) {
        JsonValue params = luaToJson(L, -1);
        step.parameters = nlohmann::json::parse(params.dump());
    }
    lua_pop(L, 1);

    return step;
}

// 从 Lua 表构建 Workflow
static wingman::server::Workflow parseWorkflow(lua_State* L, int index) {
    wingman::server::Workflow workflow;

    if (!lua_istable(L, index)) {
        return workflow;
    }

    lua_getfield(L, index, "id");
    if (lua_isstring(L, -1)) workflow.id = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "name");
    if (lua_isstring(L, -1)) workflow.name = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "description");
    if (lua_isstring(L, -1)) workflow.description = lua_tostring(L, -1);
    lua_pop(L, 1);

    // 解析 shared_context
    lua_getfield(L, index, "shared_context");
    if (!lua_isnil(L, -1)) {
        JsonValue context = luaToJson(L, -1);
        workflow.sharedContext = nlohmann::json::parse(context.dump());
    }
    lua_pop(L, 1);

    // 解析 steps 数组
    lua_getfield(L, index, "steps");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_istable(L, -1)) {
                workflow.steps.push_back(parseTaskStep(L, -1));
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    return workflow;
}

// 将 TaskStep 转换为 Lua 表
static void pushTaskStep(lua_State* L, const wingman::server::TaskStep& step) {
    lua_newtable(L);

    lua_pushstring(L, step.id.c_str());
    lua_setfield(L, -2, "id");

    lua_pushstring(L, step.name.c_str());
    lua_setfield(L, -2, "name");

    lua_pushstring(L, step.script.c_str());
    lua_setfield(L, -2, "script");

    lua_pushinteger(L, step.timeoutSeconds);
    lua_setfield(L, -2, "timeout_seconds");

    // workers 数组
    lua_newtable(L);
    for (size_t i = 0; i < step.workers.size(); ++i) {
        lua_pushstring(L, step.workers[i].c_str());
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    lua_setfield(L, -2, "workers");

    // depends_on 数组
    lua_newtable(L);
    for (size_t i = 0; i < step.dependsOn.size(); ++i) {
        lua_pushstring(L, step.dependsOn[i].c_str());
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    lua_setfield(L, -2, "depends_on");

    // parameters
    if (!step.parameters.empty()) {
        std::string paramsStr = step.parameters.dump();
        JsonValue params = JsonValue::parse(paramsStr);
        pushJsonValue(L, params);
        lua_setfield(L, -2, "parameters");
    }
}

// 将 Workflow 转换为 Lua 表
static void pushWorkflow(lua_State* L, const wingman::server::Workflow& workflow) {
    lua_newtable(L);

    lua_pushstring(L, workflow.id.c_str());
    lua_setfield(L, -2, "id");

    lua_pushstring(L, workflow.name.c_str());
    lua_setfield(L, -2, "name");

    lua_pushstring(L, workflow.description.c_str());
    lua_setfield(L, -2, "description");

    lua_pushstring(L, wingman::server::workflowStatusToString(workflow.status).c_str());
    lua_setfield(L, -2, "status");

    lua_pushinteger(L, workflow.createdTime);
    lua_setfield(L, -2, "created_time");

    lua_pushinteger(L, workflow.startTime);
    lua_setfield(L, -2, "start_time");

    lua_pushinteger(L, workflow.endTime);
    lua_setfield(L, -2, "end_time");

    // shared_context
    if (!workflow.sharedContext.empty()) {
        std::string contextStr = workflow.sharedContext.dump();
        JsonValue context = JsonValue::parse(contextStr);
        pushJsonValue(L, context);
        lua_setfield(L, -2, "shared_context");
    }

    // steps 数组
    lua_newtable(L);
    for (size_t i = 0; i < workflow.steps.size(); ++i) {
        pushTaskStep(L, workflow.steps[i]);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    lua_setfield(L, -2, "steps");
}

// API: submit_workflow(workflow_table) -> workflow_id
int submit_workflow(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushnil(L);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    if (!lua_istable(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "Expected workflow table");
        return 2;
    }

    try {
        wingman::server::Workflow workflow = parseWorkflow(L, 1);
        std::string workflowId = orchestrator->submitWorkflow(workflow);

        lua_pushstring(L, workflowId.c_str());
        return 1;
    } catch (const std::exception& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

// API: cancel_workflow(workflow_id) -> success
int cancel_workflow(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    const char* workflowId = luaL_checkstring(L, 1);
    bool cancelled = orchestrator->cancelWorkflow(workflowId);

    lua_pushboolean(L, cancelled);
    return 1;
}

// API: get_workflow(workflow_id) -> workflow_table or nil
int get_workflow(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushnil(L);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    const char* workflowId = luaL_checkstring(L, 1);
    auto workflow = orchestrator->getWorkflow(workflowId);

    if (!workflow) {
        lua_pushnil(L);
        return 1;
    }

    pushWorkflow(L, *workflow);
    return 1;
}

// API: get_all_workflows() -> array of workflow_table
int get_all_workflows(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushnil(L);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    auto workflows = orchestrator->getAllWorkflows();

    lua_newtable(L);
    for (size_t i = 0; i < workflows.size(); ++i) {
        pushWorkflow(L, workflows[i]);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

    return 1;
}

// API: get_next_task(worker_id, workflow_id) -> task_table or nil
int get_next_task(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushnil(L);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    const char* workerId = luaL_checkstring(L, 1);
    const char* workflowId = luaL_checkstring(L, 2);

    auto task = orchestrator->getNextTask(workerId, workflowId);

    if (!task) {
        lua_pushnil(L);
        return 1;
    }

    pushTaskStep(L, *task);
    return 1;
}

// API: report_progress(worker_id, workflow_id, step_id, progress_table)
int report_progress(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    const char* workerId = luaL_checkstring(L, 1);
    const char* workflowId = luaL_checkstring(L, 2);
    const char* stepId = luaL_checkstring(L, 3);

    nlohmann::json progress;
    if (!lua_isnil(L, 4)) {
        JsonValue progressJson = luaToJson(L, 4);
        progress = nlohmann::json::parse(progressJson.dump());
    }

    bool reported = orchestrator->reportProgress(workerId, workflowId, stepId, progress);

    lua_pushboolean(L, reported);
    return 1;
}

// API: complete_task(worker_id, workflow_id, step_id, result_table, success)
int complete_task(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    const char* workerId = luaL_checkstring(L, 1);
    const char* workflowId = luaL_checkstring(L, 2);
    const char* stepId = luaL_checkstring(L, 3);

    nlohmann::json result;
    if (!lua_isnil(L, 4)) {
        JsonValue resultJson = luaToJson(L, 4);
        result = nlohmann::json::parse(resultJson.dump());
    }

    bool success = true;
    if (!lua_isnil(L, 5)) {
        success = lua_toboolean(L, 5) != 0;
    }

    bool completed = orchestrator->completeTask(workerId, workflowId, stepId, success, result);

    lua_pushboolean(L, completed);
    return 1;
}

// API: fail_task(worker_id, workflow_id, step_id, error_message)
int fail_task(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    const char* workerId = luaL_checkstring(L, 1);
    const char* workflowId = luaL_checkstring(L, 2);
    const char* stepId = luaL_checkstring(L, 3);
    const char* error = luaL_optstring(L, 4, "Unknown error");

    bool failed = orchestrator->failTask(workerId, workflowId, stepId, error);

    lua_pushboolean(L, failed);
    return 1;
}

// API: get_worker_statuses(workflow_id) -> array of worker_status_table
int get_worker_statuses(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushnil(L);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    const char* workflowId = luaL_checkstring(L, 1);
    auto statuses = orchestrator->getWorkerStatuses(workflowId);

    lua_newtable(L);
    for (size_t i = 0; i < statuses.size(); ++i) {
        const auto& status = statuses[i];
        lua_newtable(L);

        lua_pushstring(L, status.workerId.c_str());
        lua_setfield(L, -2, "worker_id");

        lua_pushstring(L, status.stepId.c_str());
        lua_setfield(L, -2, "step_id");

        lua_pushstring(L, wingman::server::stepStatusToString(status.status).c_str());
        lua_setfield(L, -2, "status");

        lua_pushstring(L, status.message.c_str());
        lua_setfield(L, -2, "message");

        lua_pushinteger(L, status.startTime);
        lua_setfield(L, -2, "start_time");

        lua_pushinteger(L, status.endTime);
        lua_setfield(L, -2, "end_time");

        // progress
        if (!status.progress.empty()) {
            std::string progressStr = status.progress.dump();
            JsonValue progressJson = JsonValue::parse(progressStr);
            pushJsonValue(L, progressJson);
            lua_setfield(L, -2, "progress");
        }

        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

    return 1;
}

void registerOrchestrationModule(lua_State* L);

} // namespace orchestration

#endif // WINGMAN_BUILD_SERVER
// ============================================================================
// KV 存储模块
// ============================================================================

// TODO: Re-enable when kvstore.cpp is properly configured
#if 0

namespace kv {

// 字符串操作
int set(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    const char* value = luaL_checkstring(L, 2);
    KvOptions options = getKvOptions(L, 3);

    getKVStore()->set(key, value, options);
    return 0;
}

int get(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    std::string value = getKVStore()->get(key);
    lua_pushstring(L, value.c_str());
    return 1;
}

int del(lua_State* L) {
    if (lua_isstring(L, 1)) {
        const char* key = lua_tostring(L, 1);
        getKVStore()->del(key);
    } else if (lua_istable(L, 1)) {
        std::vector<std::string> keys;
        lua_pushnil(L);
        while (lua_next(L, 1) != 0) {
            if (lua_isstring(L, -1)) {
                keys.push_back(lua_tostring(L, -1));
            }
            lua_pop(L, 1);
        }
        getKVStore()->del(keys);
    }
    return 0;
}

int exists(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    bool result = getKVStore()->exists(key);
    lua_pushboolean(L, result);
    return 1;
}

int expire(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    int64_t seconds = luaL_checkinteger(L, 2);
    getKVStore()->expire(key, seconds);
    return 0;
}

int ttl(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    int64_t result = getKVStore()->ttl(key);
    lua_pushinteger(L, result);
    return 1;
}

int incr(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    int64_t delta = luaL_optinteger(L, 2, 1);
    int64_t result = getKVStore()->incr(key, delta);
    lua_pushinteger(L, result);
    return 1;
}

// Hash 操作
int hset(lua_State* L) {
    const char* hash = luaL_checkstring(L, 1);
    const char* field = luaL_checkstring(L, 2);
    const char* value = luaL_checkstring(L, 3);
    getKVStore()->hset(hash, field, value);
    return 0;
}

int hget(lua_State* L) {
    const char* hash = luaL_checkstring(L, 1);
    const char* field = luaL_checkstring(L, 2);
    std::string value = getKVStore()->hget(hash, field);
    lua_pushstring(L, value.c_str());
    return 1;
}

int hgetall(lua_State* L) {
    const char* hash = luaL_checkstring(L, 1);
    HashFields fields = getKVStore()->hgetall(hash);

    lua_newtable(L);
    for (const auto& [field, value] : fields) {
        lua_pushstring(L, value.c_str());
        lua_setfield(L, -2, field.c_str());
    }
    return 1;
}

int hdel(lua_State* L) {
    const char* hash = luaL_checkstring(L, 1);
    const char* field = luaL_checkstring(L, 2);
    getKVStore()->hdel(hash, field);
    return 0;
}

int hexists(lua_State* L) {
    const char* hash = luaL_checkstring(L, 1);
    const char* field = luaL_checkstring(L, 2);
    bool result = getKVStore()->hexists(hash, field);
    lua_pushboolean(L, result);
    return 1;
}

int hkeys(lua_State* L) {
    const char* hash = luaL_checkstring(L, 1);
    std::vector<std::string> keys = getKVStore()->hkeys(hash);

    lua_newtable(L);
    for (size_t i = 0; i < keys.size(); ++i) {
        lua_pushstring(L, keys[i].c_str());
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    return 1;
}

// List 操作
int lpush(lua_State* L) {
    const char* list = luaL_checkstring(L, 1);
    const char* value = luaL_checkstring(L, 2);
    getKVStore()->lpush(list, value);
    return 0;
}

int rpush(lua_State* L) {
    const char* list = luaL_checkstring(L, 1);
    const char* value = luaL_checkstring(L, 2);
    getKVStore()->rpush(list, value);
    return 0;
}

int lpop(lua_State* L) {
    const char* list = luaL_checkstring(L, 1);
    std::string value = getKVStore()->lpop(list);
    lua_pushstring(L, value.c_str());
    return 1;
}

int rpop(lua_State* L) {
    const char* list = luaL_checkstring(L, 1);
    std::string value = getKVStore()->rpop(list);
    lua_pushstring(L, value.c_str());
    return 1;
}

int llen(lua_State* L) {
    const char* list = luaL_checkstring(L, 1);
    size_t len = getKVStore()->llen(list);
    lua_pushinteger(L, len);
    return 1;
}

int lrange(lua_State* L) {
    const char* list = luaL_checkstring(L, 1);
    int64_t start = luaL_checkinteger(L, 2);
    int64_t stop = luaL_checkinteger(L, 3);
    ListValues values = getKVStore()->lrange(list, start, stop);

    lua_newtable(L);
    for (size_t i = 0; i < values.size(); ++i) {
        lua_pushstring(L, values[i].c_str());
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    return 1;
}

void registerKvModule(lua_State* L);

} // namespace kv

#endif


// ============================================================================
// 模块注册函数（需要 lua_State 参数）
// ============================================================================

void registerHttpModule(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, http::get);
    lua_setfield(L, -2, "get");

    lua_pushcfunction(L, http::post);
    lua_setfield(L, -2, "post");

    lua_pushcfunction(L, http::postForm);
    lua_setfield(L, -2, "postForm");

    lua_pushcfunction(L, http::put);
    lua_setfield(L, -2, "put");

    lua_pushcfunction(L, http::del);
    lua_setfield(L, -2, "del");

    lua_setglobal(L, "http");
}

void registerJsonModule(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, json::decode);
    lua_setfield(L, -2, "decode");

    lua_pushcfunction(L, json::encode);
    lua_setfield(L, -2, "encode");

    lua_pushcfunction(L, json::null);
    lua_setfield(L, -2, "null");

    lua_setglobal(L, "json");
}

void registerKvModule(lua_State* L) {
    // TODO: Re-enable when kvstore.cpp is properly configured
    (void)L;
    /*
    lua_newtable(L);

    lua_pushcfunction(L, kv::set);
    lua_setfield(L, -2, "set");

    lua_pushcfunction(L, kv::get);
    lua_setfield(L, -2, "get");

    lua_pushcfunction(L, kv::del);
    lua_setfield(L, -2, "del");

    lua_pushcfunction(L, kv::exists);
    lua_setfield(L, -2, "exists");

    lua_pushcfunction(L, kv::expire);
    lua_setfield(L, -2, "expire");

    lua_pushcfunction(L, kv::ttl);
    lua_setfield(L, -2, "ttl");

    lua_pushcfunction(L, kv::incr);
    lua_setfield(L, -2, "incr");

    lua_pushcfunction(L, kv::hset);
    lua_setfield(L, -2, "hset");

    lua_pushcfunction(L, kv::hget);
    lua_setfield(L, -2, "hget");

    lua_pushcfunction(L, kv::hgetall);
    lua_setfield(L, -2, "hgetall");

    lua_pushcfunction(L, kv::hdel);
    lua_setfield(L, -2, "hdel");

    lua_pushcfunction(L, kv::hexists);
    lua_setfield(L, -2, "hexists");

    lua_pushcfunction(L, kv::hkeys);
    lua_setfield(L, -2, "hkeys");

    lua_pushcfunction(L, kv::lpush);
    lua_setfield(L, -2, "lpush");

    lua_pushcfunction(L, kv::rpush);
    lua_setfield(L, -2, "rpush");

    lua_pushcfunction(L, kv::lpop);
    lua_setfield(L, -2, "lpop");

    lua_pushcfunction(L, kv::rpop);
    lua_setfield(L, -2, "rpop");

    lua_pushcfunction(L, kv::llen);
    lua_setfield(L, -2, "llen");

    lua_pushcfunction(L, kv::lrange);
    lua_setfield(L, -2, "lrange");

    lua_setglobal(L, "kv");
    */
}

void registerOrchestrationModule(lua_State* L) {
    using namespace orchestration;

    lua_newtable(L);

    lua_pushcfunction(L, orchestration::submit_workflow);
    lua_setfield(L, -2, "submit_workflow");

    lua_pushcfunction(L, orchestration::cancel_workflow);
    lua_setfield(L, -2, "cancel_workflow");

    lua_pushcfunction(L, orchestration::get_workflow);
    lua_setfield(L, -2, "get_workflow");

    lua_pushcfunction(L, orchestration::get_all_workflows);
    lua_setfield(L, -2, "get_all_workflows");

    lua_pushcfunction(L, orchestration::get_next_task);
    lua_setfield(L, -2, "get_next_task");

    lua_pushcfunction(L, orchestration::report_progress);
    lua_setfield(L, -2, "report_progress");

    lua_pushcfunction(L, orchestration::complete_task);
    lua_setfield(L, -2, "complete_task");

    lua_pushcfunction(L, orchestration::fail_task);
    lua_setfield(L, -2, "fail_task");

    lua_pushcfunction(L, orchestration::get_worker_statuses);
    lua_setfield(L, -2, "get_worker_statuses");

    lua_setglobal(L, "orchestration");
}

// ============================================================================
// Team 模块 - 队伍协调
// ============================================================================

#ifdef WINGMAN_BUILD_SERVER
namespace team {

// 全局 Orchestrator 实例
static wingman::server::Orchestrator* g_orchestrator = nullptr;

// 保存当前脚本的身份信息
static std::string g_myClientId;
static std::string g_myUsername;
static std::string g_myTeamId;

// 设置全局 Orchestrator
void setOrchestrator(wingman::server::Orchestrator* orchestrator) {
    g_orchestrator = orchestrator;
}

// 获取全局 Orchestrator
static wingman::server::Orchestrator* getOrchestrator() {
    return g_orchestrator;
}

// API: team.join(team_id) -> success
int join(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    const char* teamId = luaL_checkstring(L, 1);

    // 使用已保存的 clientId
    if (g_myClientId.empty()) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Client not registered. Call team.register() first.");
        return 2;
    }

    // 调用 Orchestrator 加入队伍
    bool success = orchestrator->joinTeam(g_myClientId, teamId);
    if (success) {
        g_myTeamId = teamId;
    }

    lua_pushboolean(L, success);
    return 1;
}

// API: team.register(username) -> client_id
int reg(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushnil(L);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    const char* username = luaL_optstring(L, 1, "");

    // 注册客户端
    g_myClientId = orchestrator->registerClient();
    g_myUsername = username;

    // 发送心跳更新用户名
    orchestrator->heartbeat(g_myClientId, "logged_in", "", g_myUsername);

    lua_pushstring(L, g_myClientId.c_str());
    return 1;
}

// API: team.leave() -> success
int leave(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    if (g_myClientId.empty()) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Not registered");
        return 2;
    }

    bool success = orchestrator->leaveTeam(g_myClientId);
    if (success) {
        g_myTeamId.clear();
    }

    lua_pushboolean(L, success);
    return 1;
}

// API: team.send(action, data) -> success
int send(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Orchestrator not initialized");
        return 2;
    }

    if (g_myTeamId.empty()) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Not in a team");
        return 2;
    }

    const char* action = luaL_checkstring(L, 1);

    // 构建数据 JSON
    std::string data;
    if (!lua_isnone(L, 2)) {
        JsonValue dataJson = luaToJson(L, 2);
        data = dataJson.dump();
    }

    bool success = orchestrator->sendToTeamSession(g_myTeamId, action, g_myClientId, data);

    lua_pushboolean(L, success);
    return 1;
}

// API: team.poll() -> array of messages or nil
int poll(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushnil(L);
        return 1;
    }

    if (g_myClientId.empty()) {
        lua_pushnil(L);
        return 1;
    }

    auto messages = orchestrator->receiveFromTeamSession(g_myClientId);

    if (messages.empty()) {
        lua_pushnil(L);
        return 1;
    }

    // 转换为 Lua 数组
    lua_newtable(L);
    for (size_t i = 0; i < messages.size(); ++i) {
        const auto& msg = messages[i];

        lua_newtable(L);
        lua_pushstring(L, msg.messageId.c_str());
        lua_setfield(L, -2, "id");
        lua_pushstring(L, msg.action.c_str());
        lua_setfield(L, -2, "action");
        lua_pushstring(L, msg.fromClientId.c_str());
        lua_setfield(L, -2, "from");
        lua_pushstring(L, msg.fromUsername.c_str());
        lua_setfield(L, -2, "username");

        // 解析 data
        if (!msg.data.empty()) {
            try {
                JsonValue dataJson = JsonValue::parse(msg.data);
                pushJsonValue(L, dataJson);
                lua_setfield(L, -2, "data");
            } catch (...) {
                lua_pushstring(L, msg.data.c_str());
                lua_setfield(L, -2, "data");
            }
        }

        lua_pushinteger(L, msg.timestamp);
        lua_setfield(L, -2, "timestamp");

        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

    return 1;
}

// API: team.members() -> array of {id, username}
int members(lua_State* L) {
    auto* orchestrator = getOrchestrator();
    if (!orchestrator) {
        lua_pushnil(L);
        return 1;
    }

    if (g_myTeamId.empty()) {
        lua_pushnil(L);
        return 1;
    }

    auto session = orchestrator->getTeamSession(g_myTeamId);
    if (!session.isActive) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    for (size_t i = 0; i < session.members.size(); ++i) {
        const auto& memberId = session.members[i];

        // 获取成员信息
        auto clientInfo = orchestrator->getClientInfo(memberId);

        lua_newtable(L);
        lua_pushstring(L, memberId.c_str());
        lua_setfield(L, -2, "id");
        lua_pushstring(L, clientInfo.username.c_str());
        lua_setfield(L, -2, "username");

        if (memberId == session.leaderId) {
            lua_pushboolean(L, 1);
            lua_setfield(L, -2, "is_leader");
        }

        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

    return 1;
}

// API: team.info() -> {team_id, my_id, my_username, member_count}
int info(lua_State* L) {
    lua_newtable(L);

    lua_pushstring(L, g_myTeamId.c_str());
    lua_setfield(L, -2, "team_id");

    lua_pushstring(L, g_myClientId.c_str());
    lua_setfield(L, -2, "my_id");

    lua_pushstring(L, g_myUsername.c_str());
    lua_setfield(L, -2, "my_username");

    auto* orchestrator = getOrchestrator();
    if (orchestrator && !g_myTeamId.empty()) {
        auto session = orchestrator->getTeamSession(g_myTeamId);
        if (session.isActive) {
            lua_pushinteger(L, session.members.size());
            lua_setfield(L, -2, "member_count");

            lua_pushinteger(L, session.getUnreadCount(g_myClientId));
            lua_setfield(L, -2, "unread_count");
        }
    }

    return 1;
}

void registerTeamModule(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, team::reg);
    lua_setfield(L, -2, "register");

    lua_pushcfunction(L, team::join);
    lua_setfield(L, -2, "join");

    lua_pushcfunction(L, team::leave);
    lua_setfield(L, -2, "leave");

    lua_pushcfunction(L, team::send);
    lua_setfield(L, -2, "send");

    lua_pushcfunction(L, team::poll);
    lua_setfield(L, -2, "poll");

    lua_pushcfunction(L, team::members);
    lua_setfield(L, -2, "members");

    lua_pushcfunction(L, team::info);
    lua_setfield(L, -2, "info");

    lua_setglobal(L, "team");
}

} // namespace team

#else
// 存根实现（未构建 server 模块时）
namespace team {
static void* g_orchestrator = nullptr;
void setOrchestrator(void* orchestrator) { g_orchestrator = orchestrator; }
void registerTeamModule(lua_State* L) { (void)L; }
} // namespace team
#endif // WINGMAN_BUILD_SERVER

} // namespace wingman::lua
