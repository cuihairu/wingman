#include "wingman/vision_ai.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <optional>

#include <nlohmann/json.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include "wingman/crypt.hpp"
#include "wingman/http.hpp"

namespace wingman {
namespace {

std::mutex s_mutex;
VisionAiConfig s_config;
bool s_configured = false;
VisionAi::FrameProvider s_frameProvider;
std::string s_lastError;

constexpr int kJpegQuality = 82; // 与远程截图（android_screenshot.cpp）同参数

void setError(const std::string& msg) {
	s_lastError = msg;
}

/// Bitmap(BGRA) → JPEG 字节（bitmapToMat 同款换序，vision.cpp:20）。
std::vector<uint8_t> encodeJpeg(const Bitmap& bitmap) {
	cv::Mat rgba(bitmap.getHeight(), bitmap.getWidth(), CV_8UC4,
	             const_cast<uint8_t*>(bitmap.getData()), bitmap.getWidth() * 4);
	cv::Mat bgr;
	cv::cvtColor(rgba, bgr, cv::COLOR_BGRA2BGR);
	std::vector<uint8_t> out;
	cv::imencode(".jpg", bgr, out, {cv::IMWRITE_JPEG_QUALITY, kJpegQuality});
	return out;
}

/// provider 响应 → 模型回包 JSON 对象（locate/elements 共用容错抽取）：
/// OpenAI 信封 choices[0].message.content 或裸对象；content 为字符串时剥
/// 围栏/杂文本取首 '{' 到末 '}'。失败置 lastError 并返回 nullopt。
std::optional<nlohmann::json> extractReplyObject(const nlohmann::json& root) {
	nlohmann::json content = root;
	if (root.contains("choices") && root["choices"].is_array() && !root["choices"].empty()) {
		content = root["choices"][0]["message"]["content"];
	}
	if (content.is_string()) {
		const std::string text = content.get<std::string>();
		const auto first = text.find('{');
		const auto last = text.rfind('}');
		if (first == std::string::npos || last == std::string::npos || last < first) {
			setError("vision-ai: no JSON object in model reply");
			return std::nullopt;
		}
		return nlohmann::json::parse(text.substr(first, last - first + 1));
	}
	if (content.is_object()) {
		return content;
	}
	setError("vision-ai: unexpected content type in response");
	return std::nullopt;
}

/// 闭环共享段①：配置检查 + 截帧（注入优先，锁外执行）+ JPEG 编码。
/// 任一步失败置 lastError 返回 false。
bool prepareProviderFrame(const Rect& region,
                          VisionAiConfig& outCfg,
                          std::unique_ptr<Bitmap>& outFrame,
                          std::vector<uint8_t>& outJpeg) {
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		if (!s_configured) {
			setError("vision-ai: provider not configured (call vision.aiSetup first)");
			return false;
		}
		outCfg = s_config;
	}
	{
		VisionAi::FrameProvider provider;
		{
			std::lock_guard<std::mutex> lock(s_mutex);
			provider = s_frameProvider;
		}
		outFrame = provider ? provider(region)
		                    : (region.width > 0 && region.height > 0
		                           ? Screen::capture(region)
		                           : Screen::capture());
	}
	if (!outFrame || outFrame->getWidth() <= 0 || outFrame->getHeight() <= 0) {
		setError("vision-ai: screen capture failed");
		return false;
	}
	outJpeg = encodeJpeg(*outFrame);
	if (outJpeg.empty()) {
		setError("vision-ai: jpeg encode failed");
		return false;
	}
	return true;
}

/// 闭环共享段②：POST {baseUrl}/chat/completions（Authorization/超时取配置）。
HttpResponse postChatCompletions(const VisionAiConfig& cfg, const std::string& requestBody) {
	HttpClient client;
	client.setDefaultHeader("Authorization", "Bearer " + cfg.apiKey);
	client.setDefaultTimeout(cfg.timeoutSeconds > 0 ? cfg.timeoutSeconds : 60);
	HttpOptions options;
	options.timeout = cfg.timeoutSeconds > 0 ? cfg.timeoutSeconds : 60;
	options.headers["Content-Type"] = "application/json";
	const std::string url = cfg.baseUrl + "/chat/completions";
	return client.post(url, requestBody, options);
}

/// 0–1000 归一 bbox → 像素（x/y 取左上角，w/h 非负）。帧尺寸非法或退化
/// bbox（零宽高）返回 false。
bool normalizedBboxToPixels(const nlohmann::json& b, int frameWidth, int frameHeight,
                            int& outX, int& outY, int& outW, int& outH) {
	if (frameWidth <= 0 || frameHeight <= 0) {
		return false;
	}
	const double x1 = b[0].get<double>();
	const double y1 = b[1].get<double>();
	const double x2 = b[2].get<double>();
	const double y2 = b[3].get<double>();
	const int px1 = static_cast<int>(x1 * frameWidth / 1000.0);
	const int py1 = static_cast<int>(y1 * frameHeight / 1000.0);
	const int px2 = static_cast<int>(x2 * frameWidth / 1000.0);
	const int py2 = static_cast<int>(y2 * frameHeight / 1000.0);
	outX = std::max(0, std::min(px1, px2));
	outY = std::max(0, std::min(py1, py2));
	outW = std::abs(px2 - px1);
	outH = std::abs(py2 - py1);
	return outW > 0 && outH > 0;
}

} // namespace

void VisionAi::setFrameProvider(FrameProvider provider) {
	std::lock_guard<std::mutex> lock(s_mutex);
	s_frameProvider = std::move(provider);
}

void VisionAi::setup(const VisionAiConfig& cfg) {
	std::lock_guard<std::mutex> lock(s_mutex);
	s_config = cfg;
	s_configured = true;
}

void VisionAi::reset() {
	std::lock_guard<std::mutex> lock(s_mutex);
	s_config = VisionAiConfig{};
	s_configured = false;
	s_lastError.clear();
}

bool VisionAi::isConfigured() {
	std::lock_guard<std::mutex> lock(s_mutex);
	return s_configured;
}

VisionAiConfig VisionAi::config() {
	std::lock_guard<std::mutex> lock(s_mutex);
	VisionAiConfig masked = s_config;
	masked.apiKey.clear(); // 明文不出本接口
	return masked;
}

bool VisionAi::hasApiKey() {
	std::lock_guard<std::mutex> lock(s_mutex);
	return !s_config.apiKey.empty();
}

std::string VisionAi::lastError() {
	std::lock_guard<std::mutex> lock(s_mutex);
	return s_lastError;
}

std::string VisionAi::buildLocateRequestBody(const std::string& model,
                                             const std::string& desc,
                                             const std::string& jpegBase64) {
	nlohmann::json prompt = nlohmann::json::array({
	    nlohmann::json{{"type", "text"},
	                   {"text", "You are a UI element locator for automation. "
	                            "Locate the following target on the screenshot: " +
	                               desc +
	                               ". Reply with ONLY a JSON object, no markdown fences, "
	                            "no extra text: {\"found\": <bool>, \"label\": <string>, "
	                            "\"bbox_2d\": [x1, y1, x2, y2], \"confidence\": <0-1>}. "
	                            "All coordinates are normalized to 0-1000 relative to the "
	                            "image, origin at top-left. If the target is absent, "
	                            "return found=false and zeros."}},
	    nlohmann::json{{"type", "image_url"},
	                   {"image_url",
	                    nlohmann::json{{"url", "data:image/jpeg;base64," + jpegBase64}}}},
	});
	nlohmann::json body = {
	    {"model", model},
	    {"messages", nlohmann::json::array({nlohmann::json{{"role", "user"}, {"content", prompt}}})},
	    {"temperature", 0},
	};
	return body.dump();
}

VisionAiBox VisionAi::parseLocateResponse(const std::string& responseBody,
                                          int frameWidth,
                                          int frameHeight) {
	VisionAiBox box;
	setError("");
	try {
		auto root = nlohmann::json::parse(responseBody);
		const auto objOpt = extractReplyObject(root);
		if (!objOpt) {
			return box;
		}
		const nlohmann::json& obj = *objOpt;
		box.found = obj.value("found", false);
		box.label = obj.value("label", "");
		box.confidence = obj.value("confidence", 0.0);
		if (box.found) {
			if (!obj.contains("bbox_2d") || !obj["bbox_2d"].is_array()
			    || obj["bbox_2d"].size() != 4) {
				setError("vision-ai: found=true but bbox_2d missing/invalid");
				box.found = false;
				return box;
			}
			// 0–1000 归一 → 像素；帧尺寸非法或退化 bbox 判失败
			if (!normalizedBboxToPixels(obj["bbox_2d"], frameWidth, frameHeight,
			                            box.x, box.y, box.w, box.h)) {
				if (frameWidth <= 0 || frameHeight <= 0) {
					setError("vision-ai: invalid frame size");
				} else {
					setError("vision-ai: degenerate bbox (zero width/height)");
				}
				box.found = false;
			}
		}
	} catch (const nlohmann::json::exception& e) {
		box = VisionAiBox{};
		setError(std::string("vision-ai: json parse failed: ") + e.what());
	}
	return box;
}

VisionAiBox VisionAi::locate(const std::string& desc, const Rect& region) {
	VisionAiBox box;
	setError("");
	VisionAiConfig cfg;
	std::unique_ptr<Bitmap> frame;
	std::vector<uint8_t> jpeg;
	if (!prepareProviderFrame(region, cfg, frame, jpeg)) {
		return box;
	}

	const std::string requestBody =
	    buildLocateRequestBody(cfg.model, desc, crypt::base64Encode(jpeg));
	const HttpResponse resp = postChatCompletions(cfg, requestBody);
	if (!resp.isSuccess()) {
		setError("vision-ai: provider http " + std::to_string(resp.statusCode)
		         + (resp.error.empty() ? "" : (": " + resp.error)));
		return box;
	}
	return parseLocateResponse(resp.body, frame->getWidth(), frame->getHeight());
}

std::string VisionAi::buildElementsRequestBody(const std::string& model,
                                               const std::string& desc,
                                               const std::string& jpegBase64) {
	const std::string task = desc.empty()
	    ? "List ALL interactive UI elements visible on the screenshot "
	      "(buttons, text fields, links, list items)."
	    : "Locate all UI elements on the screenshot matching: " + desc + ".";
	nlohmann::json prompt = nlohmann::json::array({
	    nlohmann::json{{"type", "text"},
	                   {"text", "You are a UI element detector for automation. " +
	                               task +
	                               " Reply with ONLY a JSON object, no markdown fences, "
	                            "no extra text: {\"found\": <bool>, \"elements\": "
	                            "[{\"label\": <string>, \"bbox_2d\": [x1, y1, x2, y2], "
	                            "\"confidence\": <0-1>}]}. All coordinates are normalized "
	                            "to 0-1000 relative to the image, origin at top-left. "
	                            "If nothing matches, return found=false and an empty "
	                            "elements list."}},
	    nlohmann::json{{"type", "image_url"},
	                   {"image_url",
	                    nlohmann::json{{"url", "data:image/jpeg;base64," + jpegBase64}}}},
	});
	nlohmann::json body = {
	    {"model", model},
	    {"messages", nlohmann::json::array({nlohmann::json{{"role", "user"}, {"content", prompt}}})},
	    {"temperature", 0},
	};
	return body.dump();
}

std::vector<VisionAiElement> VisionAi::parseElementsResponse(const std::string& responseBody,
                                                             int frameWidth,
                                                             int frameHeight) {
	std::vector<VisionAiElement> out;
	setError("");
	try {
		auto root = nlohmann::json::parse(responseBody);
		const auto objOpt = extractReplyObject(root);
		if (!objOpt) {
			return out;
		}
		const nlohmann::json& obj = *objOpt;
		if (!obj.value("found", false)) {
			return out; // 模型明示无匹配：正常空批（lastError 保持空）
		}
		if (!obj.contains("elements") || !obj["elements"].is_array()) {
			setError("vision-ai: found=true but elements missing/not array");
			return out;
		}
		for (const auto& item : obj["elements"]) {
			// 单项坏（非对象 / bbox 缺失或退化）跳过不废整批
			if (!item.is_object() || !item.contains("bbox_2d")
			    || !item["bbox_2d"].is_array() || item["bbox_2d"].size() != 4) {
				continue;
			}
			VisionAiElement elem;
			elem.label = item.value("label", "");
			elem.confidence = item.value("confidence", 0.0);
			if (!normalizedBboxToPixels(item["bbox_2d"], frameWidth, frameHeight,
			                            elem.x, elem.y, elem.w, elem.h)) {
				continue;
			}
			out.push_back(std::move(elem));
		}
	} catch (const nlohmann::json::exception& e) {
		out.clear();
		setError(std::string("vision-ai: json parse failed: ") + e.what());
	}
	return out;
}

std::vector<VisionAiElement> VisionAi::elements(const std::string& desc, const Rect& region) {
	std::vector<VisionAiElement> out;
	setError("");
	VisionAiConfig cfg;
	std::unique_ptr<Bitmap> frame;
	std::vector<uint8_t> jpeg;
	if (!prepareProviderFrame(region, cfg, frame, jpeg)) {
		return out;
	}

	const std::string requestBody =
	    buildElementsRequestBody(cfg.model, desc, crypt::base64Encode(jpeg));
	const HttpResponse resp = postChatCompletions(cfg, requestBody);
	if (!resp.isSuccess()) {
		setError("vision-ai: provider http " + std::to_string(resp.statusCode)
		         + (resp.error.empty() ? "" : (": " + resp.error)));
		return out;
	}
	return parseElementsResponse(resp.body, frame->getWidth(), frame->getHeight());
}

} // namespace wingman
