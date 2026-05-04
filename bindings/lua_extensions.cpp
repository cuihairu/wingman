#include "lua_extensions.hpp"
#include "wingman/http.hpp"
#include "wingman/json.hpp"
#include "wingman/kvstore.hpp"

#include <memory>
#include <unordered_map>
#include <chrono>

namespace wingman::lua {

// 全局 KeyValueStore 实例
static std::unique_ptr<KeyValueStore> g_kvStore;
static std::unique_ptr<HttpClient> g_httpClient;

// 初始化/获取全局 KV Store
static KeyValueStore* getKVStore() {
    if (!g_kvStore) {
        g_kvStore = std::make_unique<KeyValueStore>();
    }
    return g_kvStore.get();
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
        if (lua_isinteger(L, -1)) {
            options.timeout = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "followRedirects");
        if (lua_isboolean(L, -1)) {
            options.followRedirects = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "maxRedirects");
        if (lua_isinteger(L, -1)) {
            options.maxRedirects = static_cast<size_t>(lua_tointeger(L, -1));
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
        if (lua_isinteger(L, -1)) {
            options.ttl = lua_tointeger(L, -1);
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

void registerHttpModule() {
    lua_State* L = reinterpret_cast<lua_State*>(g_httpClient.get());
    // 这个函数需要从外部调用，传入 lua_State
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
        case LUA_TNUMBER:
            if (lua_isinteger(L, index)) {
                result = JsonValue(lua_tointeger(L, index));
            } else {
                result = JsonValue(lua_tonumber(L, index));
            }
            break;
        case LUA_TSTRING:
            result = JsonValue(lua_tostring(L, index));
            break;
        case LUA_TTABLE: {
            // 检查是数组还是对象
            bool isArray = true;
            int maxIndex = 0;

            lua_pushnil(L);
            while (lua_next(L, index) != 0) {
                if (lua_type(L, -2) != LUA_TNUMBER || !lua_isinteger(L, -2)) {
                    isArray = false;
                    lua_pop(L, 1);
                    break;
                }
                int idx = lua_tointeger(L, -2);
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

void registerJsonModule() {
    // 类似 registerHttpModule，需要外部传入 lua_State
}

} // namespace json

// ============================================================================
// KV 存储模块
// ============================================================================

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

void registerKvModule() {
    // 类似其他模块，需要外部传入 lua_State
}

} // namespace kv

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
}

} // namespace wingman::lua
