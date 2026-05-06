#pragma once

#include "lua_bindings.hpp"
#include "wingman/http.hpp"
#include "wingman/json.hpp"
#include "wingman/kvstore.hpp"

namespace wingman::server {
class WorkflowOrchestrator;
}

namespace wingman::lua {

// HTTP 模块
namespace http {

int get(lua_State* L);
int post(lua_State* L);
int postForm(lua_State* L);
int put(lua_State* L);
int del(lua_State* L);

} // namespace http

// JSON 模块
namespace json {

int decode(lua_State* L);
int encode(lua_State* L);
int null(lua_State* L);

} // namespace json

// KV 存储模块
namespace kv {

int set(lua_State* L);
int get(lua_State* L);
int del(lua_State* L);
int exists(lua_State* L);
int expire(lua_State* L);
int ttl(lua_State* L);
int incr(lua_State* L);

int hset(lua_State* L);
int hget(lua_State* L);
int hgetall(lua_State* L);
int hdel(lua_State* L);
int hexists(lua_State* L);
int hkeys(lua_State* L);

int lpush(lua_State* L);
int rpush(lua_State* L);
int lpop(lua_State* L);
int rpop(lua_State* L);
int llen(lua_State* L);
int lrange(lua_State* L);

} // namespace kv

// Orchestration 模块
namespace orchestration {

// 设置全局 Orchestrator 实例
void setOrchestrator(wingman::server::WorkflowOrchestrator* orchestrator);

int submit_workflow(lua_State* L);
int cancel_workflow(lua_State* L);
int get_workflow(lua_State* L);
int get_all_workflows(lua_State* L);
int get_next_task(lua_State* L);
int report_progress(lua_State* L);
int complete_task(lua_State* L);
int fail_task(lua_State* L);
int get_worker_statuses(lua_State* L);

} // namespace orchestration

// 注册扩展模块（需要 lua_State 参数）
void registerHttpModule(lua_State* L);
void registerJsonModule(lua_State* L);
void registerKvModule(lua_State* L);
void registerOrchestrationModule(lua_State* L);

} // namespace wingman::lua
