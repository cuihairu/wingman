#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "screen.hpp"

namespace wingman {

/// AI 视觉识别 provider 配置（vision 模块 aiSetup 的 C++ 面）。
///
/// 端点约定为 OpenAI 兼容 chat.completions（云端 API 与本地推理服务如
/// Ollama/vLLM 的 /v1 同形），baseUrl 填到 /v1 为止。apiKey 为解密后的
/// 明文，仅驻内存——本类不落盘、不打日志（凭据加密面见 vision.md：
/// 配置侧用 crypto.encryptAES 密文保存，aiSetup 传入密文与口令在此解密）。
struct VisionAiConfig {
	std::string baseUrl;
	std::string model;
	std::string apiKey;
	int timeoutSeconds = 60;
};

/// 本地 ONNX 检测 provider 配置（vision 模块 aiSetupLocal 的 C++ 面）。
///
/// WINGMAN_ENABLE_ML 构建下生效：模型为本仓 ml 模块 ModelEngine 可加载的
/// ONNX 检测模型（YOLOv5/v8 导出约定，ml.hpp detectObjects）；labels 为
/// classId→名称表，desc 匹配按双向大小写不敏感子串。未构建 ML 时
/// setupLocal 恒失败（绑定层返回 false，不降级不静默切 HTTP）。
struct VisionAiLocalConfig {
	std::string modelPath;
	std::vector<std::string> labels;
	float minConfidence = 0.5f;
};

/// 一次定位结果：bbox 为帧像素坐标（x/y 左上角，w/h 尺寸）。
struct VisionAiBox {
	bool found = false;
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	double confidence = 0.0;
	std::string label;
};

/// 批量元素识别单项（坐标与置信度语义同 VisionAiBox，无 found 位——
/// 整批是否为空由 elements() 返回序列本身表达）。
struct VisionAiElement {
	std::string label;
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	double confidence = 0.0;
};

/// AI 视觉识别（WINGMAN_ENABLE_VISION 构建下可用，stub 构建返回未构建错误）。
///
/// 链路：Screen::capture 截帧 → JPEG（质量 82，与远程截图同参数）→
/// base64 data URL → OpenAI 兼容 chat.completions → 模型返回
/// {"found":bool,"label":str,"bbox_2d":[x1,y1,x2,y2],"confidence":0-1}
/// （bbox_2d 为 0–1000 归一坐标，解析时按帧尺寸换算像素）。
/// 复用面：桌面 runtime 与 Android 租户（A 线）同协议接入。
class VisionAi {
public:
	/// 截帧注入（装配层，与配置数据面分离）：region 空=全屏。
	/// 默认不注入（空）→ 截帧走 Screen::capture；Android 租户在
	/// registerAndroidApis 注入 captureFrame 适配——NDK 下 Screen::
	/// capture 无装配（公共层平台宏冻结），同 provider 通路复用
	/// captureFrame 位图（android-agent-design「AI 视觉识别」节）。传 nullptr 复位。
	using FrameProvider = std::function<std::unique_ptr<Bitmap>(const Rect& region)>;

	/// 写入截帧提供者（进程级；与 setup 配置互不影响，reset 不清注入）。
	static void setFrameProvider(FrameProvider provider);

	/// 写入配置（进程级驻内存；重复 setup 整体覆盖）。
	static void setup(const VisionAiConfig& cfg);

	/// 切换到本地 ONNX 检测 provider（与 setup 的 HTTP provider 互斥，
	/// 后调用者生效；reset 双清）。模型加载失败返回 false 且不影响既有
	/// 配置。WINGMAN_ENABLE_ML 未构建时恒 false。
	static bool setupLocal(const VisionAiLocalConfig& cfg);

	/// 当前是否本地检测模式（setupLocal 成功后、setup/reset 前）。
	static bool isLocalMode();

	/// 清空配置（测试与「断开 provider」用；本地模式与 HTTP 配置双清）。
	static void reset();

	static bool isConfigured();

	/// 当前配置快照（apiKey 打码为 hasKey 标志，明文不出本接口）。
	static VisionAiConfig config();
	static bool hasApiKey();

	/// 构造 OpenAI 兼容定位请求体（纯函数，单测面）。
	/// prompt 约定模型只回 JSON 对象；坐标 0–1000 归一（Qwen-VL bbox_2d 风格），
	/// 与帧尺寸无关，换算只发生在解析侧。
	static std::string buildLocateRequestBody(const std::string& model,
	                                          const std::string& desc,
	                                          const std::string& jpegBase64);

	/// 解析 provider 响应（纯函数，单测面）。
	/// 容忍 ```json 围栏与前后杂文本；非法 JSON / 字段缺失 / 坐标越界
	/// 一律 found=false（识别失败与「没找到」在调用面同形，错误详情走
	/// lastError()）。
	static VisionAiBox parseLocateResponse(const std::string& responseBody,
	                                       int frameWidth,
	                                       int frameHeight);

	/// 完整闭环：截帧（region 空则全屏）→ provider → 像素 bbox。
	/// provider 未配置 / 未启用视觉构建 / HTTP 失败 / 解析失败：
	/// found=false 且 lastError() 给出可区分原因。
	static VisionAiBox locate(const std::string& desc, const Rect& region = Rect());

	/// 构造批量元素识别请求体（纯函数，单测面）。desc 空=列出全部可交互
	/// 元素，非空=筛选匹配描述的元素；协议约定模型只回 JSON 对象：
	/// {"found":bool,"elements":[{"label":str,"bbox_2d":[x1,y1,x2,y2],
	/// "confidence":0-1}]}（bbox_2d 同 locate 的 0–1000 归一）。
	static std::string buildElementsRequestBody(const std::string& model,
	                                            const std::string& desc,
	                                            const std::string& jpegBase64);

	/// 解析批量元素响应（纯函数，单测面）。容错口径同 parseLocateResponse
	/// （围栏/杂文本/choices 包裹/content 对象形态）；坏 JSON 或 found=false
	/// 返回空批；elements 内单项 bbox 缺失/退化只跳过该项不废整批。
	static std::vector<VisionAiElement> parseElementsResponse(const std::string& responseBody,
	                                                          int frameWidth,
	                                                          int frameHeight);

	/// 完整闭环：截帧（region 空则全屏）→ provider → 像素 bbox 列表。
	/// 失败语义同 locate：返回空批且 lastError() 给出可区分原因
	///（解析成功但屏幕无匹配元素属正常空批，lastError 为空）。
	static std::vector<VisionAiElement> elements(const std::string& desc,
	                                             const Rect& region = Rect());

	/// 最近一次 locate 失败原因（成功后清空）。
	static std::string lastError();
};

} // namespace wingman
