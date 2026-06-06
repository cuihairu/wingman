#include "wingman/script/iscript_engine.hpp"
#include "wingman/http.hpp"

namespace wingman {
namespace script {
namespace modules {

static HttpClient* getHttpClient() {
	static std::unique_ptr<HttpClient> client;
	if (!client) client = std::make_unique<HttpClient>();
	return client.get();
}

static HttpOptions toHttpOptions(const ScriptValue& v) {
	HttpOptions options;
	if (!v.isObject()) return options;
	auto* timeout = v.get("timeout");
	if (timeout) options.timeout = static_cast<int>(timeout->asInt());
	auto* followRedirects = v.get("followRedirects");
	if (followRedirects) options.followRedirects = followRedirects->asBool();
	auto* maxRedirects = v.get("maxRedirects");
	if (maxRedirects) options.maxRedirects = static_cast<int>(maxRedirects->asInt());
	auto* headers = v.get("headers");
	if (headers && headers->isObject()) {
		for (const auto& [key, val] : headers->objectVal) {
			options.headers[key] = val.asString();
		}
	}
	return options;
}

static ScriptValue fromHttpResponse(const HttpResponse& r) {
	auto obj = std::unordered_map<std::string, ScriptValue>{
		{"status", ScriptValue::fromInt(r.statusCode)},
		{"body", ScriptValue::fromString(r.body)},
		{"elapsed", ScriptValue::fromFloat(r.elapsed)},
		{"success", ScriptValue::fromBool(r.isSuccess())}
	};
	if (!r.error.empty()) {
		obj["error"] = ScriptValue::fromString(r.error);
	}
	std::unordered_map<std::string, ScriptValue> headers;
	for (const auto& [k, v] : r.headers) {
		headers[k] = ScriptValue::fromString(v);
	}
	obj["headers"] = ScriptValue::fromObject(std::move(headers));
	return ScriptValue::fromObject(std::move(obj));
}

ModuleDescriptor createHttpModule() {
	ModuleDescriptor mod;
	mod.name = "http";

	mod.functions.push_back({"get", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		HttpOptions options = args.size() > 1 ? toHttpOptions(args[1]) : HttpOptions();
		return fromHttpResponse(getHttpClient()->get(args[0].asString(), options));
	}, "url:string, options? -> {status,body,headers,...}"});

	mod.functions.push_back({"post", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string body = args.size() > 1 ? args[1].asString() : "";
		HttpOptions options = args.size() > 2 ? toHttpOptions(args[2]) : HttpOptions();
		return fromHttpResponse(getHttpClient()->post(args[0].asString(), body, options));
	}, "url:string, body:string?, options? -> {status,body,...}"});

	mod.functions.push_back({"put", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string body = args.size() > 1 ? args[1].asString() : "";
		HttpOptions options = args.size() > 2 ? toHttpOptions(args[2]) : HttpOptions();
		return fromHttpResponse(getHttpClient()->put(args[0].asString(), body, options));
	}, "url:string, body:string?, options? -> {status,body,...}"});

	mod.functions.push_back({"delete", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		HttpOptions options = args.size() > 1 ? toHttpOptions(args[1]) : HttpOptions();
		return fromHttpResponse(getHttpClient()->del(args[0].asString(), options));
	}, "url:string, options? -> {status,body,...}"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
