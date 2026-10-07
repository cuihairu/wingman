#include "wingman/vision_ai.hpp"

#include <algorithm>
#include <cstdlib>
#include <mutex>

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
		// OpenAI 兼容面：choices[0].message.content；容错：provider 直接回对象
		nlohmann::json content = root;
		if (root.contains("choices") && root["choices"].is_array() && !root["choices"].empty()) {
			content = root["choices"][0]["message"]["content"];
		}
		// content 两种形态：字符串（标准，模型回 JSON 文本，需剥围栏/杂文本）或
		// 对象（部分 provider 直接回结构化 content）
		nlohmann::json obj;
		if (content.is_string()) {
			const std::string text = content.get<std::string>();
			// 取首个 '{' 到末个 '}'
			const auto first = text.find('{');
			const auto last = text.rfind('}');
			if (first == std::string::npos || last == std::string::npos || last < first) {
				setError("vision-ai: no JSON object in model reply");
				return box;
			}
			obj = nlohmann::json::parse(text.substr(first, last - first + 1));
		} else if (content.is_object()) {
			obj = content;
		} else {
			setError("vision-ai: unexpected content type in response");
			return box;
		}
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
			const auto& b = obj["bbox_2d"];
			const double x1 = b[0].get<double>();
			const double y1 = b[1].get<double>();
			const double x2 = b[2].get<double>();
			const double y2 = b[3].get<double>();
			// 0–1000 归一 → 像素；帧尺寸为 0（无效帧）直接判失败
			if (frameWidth <= 0 || frameHeight <= 0) {
				setError("vision-ai: invalid frame size");
				box.found = false;
				return box;
			}
			const int px1 = static_cast<int>(x1 * frameWidth / 1000.0);
			const int py1 = static_cast<int>(y1 * frameHeight / 1000.0);
			const int px2 = static_cast<int>(x2 * frameWidth / 1000.0);
			const int py2 = static_cast<int>(y2 * frameHeight / 1000.0);
			box.x = std::max(0, std::min(px1, px2));
			box.y = std::max(0, std::min(py1, py2));
			box.w = std::abs(px2 - px1);
			box.h = std::abs(py2 - py1);
			if (box.w == 0 || box.h == 0) {
				setError("vision-ai: degenerate bbox (zero width/height)");
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
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		if (!s_configured) {
			setError("vision-ai: provider not configured (call vision.aiSetup first)");
			return box;
		}
		cfg = s_config;
	}

	// 截帧：注入优先（Android captureFrame 适配；锁外执行，避免截帧
	// JNI 往返期间持配置锁），未注入走桌面 Screen::capture 装配
	std::unique_ptr<Bitmap> frame;
	{
		FrameProvider provider;
		{
			std::lock_guard<std::mutex> lock(s_mutex);
			provider = s_frameProvider;
		}
		frame = provider ? provider(region)
		                 : (region.width > 0 && region.height > 0
		                        ? Screen::capture(region)
		                        : Screen::capture());
	}
	if (!frame || frame->getWidth() <= 0 || frame->getHeight() <= 0) {
		setError("vision-ai: screen capture failed");
		return box;
	}
	auto jpeg = encodeJpeg(*frame);
	if (jpeg.empty()) {
		setError("vision-ai: jpeg encode failed");
		return box;
	}

	HttpClient client;
	client.setDefaultHeader("Authorization", "Bearer " + cfg.apiKey);
	client.setDefaultTimeout(cfg.timeoutSeconds > 0 ? cfg.timeoutSeconds : 60);
	HttpOptions options;
	options.timeout = cfg.timeoutSeconds > 0 ? cfg.timeoutSeconds : 60;
	options.headers["Content-Type"] = "application/json";

	const std::string url = cfg.baseUrl + "/chat/completions";
	const std::string requestBody =
	    buildLocateRequestBody(cfg.model, desc, crypt::base64Encode(jpeg));
	const HttpResponse resp = client.post(url, requestBody, options);
	if (!resp.isSuccess()) {
		setError("vision-ai: provider http " + std::to_string(resp.statusCode)
		         + (resp.error.empty() ? "" : (": " + resp.error)));
		return box;
	}
	return parseLocateResponse(resp.body, frame->getWidth(), frame->getHeight());
}

} // namespace wingman
