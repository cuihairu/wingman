// VisionAi（AI 视觉识别 provider）单元测试：请求构造/响应解析纯函数 +
// 配置状态机。provider 网络路径依赖真实端点（OpenAI 兼容 / 本地 Ollama），
// 不在门禁内——locate 仅覆盖未配置分支。
//
// 本文件在 WINGMAN_HAS_OPENCV 树编译（tests/CMakeLists.txt 条件段）；
// stub 构建（无 OpenCV）下 VisionAi::parseLocateResponse 恒返回空 box，
// 下列解析断言不成立。

#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "wingman/vision_ai.hpp"

namespace {

using wingman::Bitmap;
using wingman::Rect;
using wingman::VisionAi;
using wingman::VisionAiBox;
using wingman::VisionAiConfig;

// OpenAI 兼容响应外壳：choices[0].message.content 携带模型 JSON 文本。
std::string openAiEnvelope(const std::string& content) {
	return R"({"choices":[{"message":{"role":"assistant","content":)" + content
	       + R"(}}],"usage":{"total_tokens":42}})";
}

class VisionAiConfigEnv : public ::testing::Test {
protected:
	void TearDown() override { VisionAi::reset(); }
};

TEST_F(VisionAiConfigEnv, SetupAndStatusRoundTrip) {
	EXPECT_FALSE(VisionAi::isConfigured());
	EXPECT_FALSE(VisionAi::hasApiKey());

	VisionAiConfig cfg;
	cfg.baseUrl = "http://127.0.0.1:11434/v1";
	cfg.model = "qwen2.5-vl:7b";
	cfg.apiKey = "sk-test-secret";
	cfg.timeoutSeconds = 45;
	VisionAi::setup(cfg);

	EXPECT_TRUE(VisionAi::isConfigured());
	EXPECT_TRUE(VisionAi::hasApiKey());
	// config() 打码：明文不出查询接口
	EXPECT_EQ(VisionAi::config().baseUrl, cfg.baseUrl);
	EXPECT_EQ(VisionAi::config().model, cfg.model);
	EXPECT_EQ(VisionAi::config().timeoutSeconds, 45);
	EXPECT_TRUE(VisionAi::config().apiKey.empty());

	VisionAi::reset();
	EXPECT_FALSE(VisionAi::isConfigured());
	EXPECT_FALSE(VisionAi::hasApiKey());
}

TEST_F(VisionAiConfigEnv, SetupOverwriteReplacesWholeConfig) {
	VisionAiConfig first;
	first.baseUrl = "http://a/v1";
	first.model = "m1";
	first.apiKey = "k1";
	VisionAi::setup(first);
	VisionAiConfig second;
	second.baseUrl = "http://b/v1";
	second.model = "m2";
	VisionAi::setup(second); // 无 key 的整体覆盖 → hasKey 归 false
	EXPECT_TRUE(VisionAi::isConfigured());
	EXPECT_FALSE(VisionAi::hasApiKey());
	EXPECT_EQ(VisionAi::config().model, "m2");
}

TEST(VisionAiLocateProtocol, BuildLocateRequestBodyShape) {
	const std::string body = VisionAi::buildLocateRequestBody(
	    "qwen2.5-vl:7b", "登录按钮", "QUJD");
	ASSERT_FALSE(body.empty());
	// 结构断言：模型名、data URL、目标描述、坐标约定关键词都在请求里
	EXPECT_NE(body.find("\"model\":\"qwen2.5-vl:7b\""), std::string::npos);
	EXPECT_NE(body.find("data:image/jpeg;base64,QUJD"), std::string::npos);
	EXPECT_NE(body.find("登录按钮"), std::string::npos);
	EXPECT_NE(body.find("bbox_2d"), std::string::npos);
	EXPECT_NE(body.find("\"temperature\":0"), std::string::npos);
	// 合法 JSON 且 messages 是单条 user 消息（解析侧可回读）
	auto root = nlohmann::json::parse(body);
	ASSERT_TRUE(root.contains("messages"));
	ASSERT_EQ(root["messages"].size(), 1u);
	EXPECT_EQ(root["messages"][0]["role"], "user");
}

TEST(VisionAiLocateProtocol, ParseOpenAiEnvelopeCenterBox) {
	// 0-1000 归一 [250,200,750,600] → 1000x500 帧像素 [250,100,500,200]
	const std::string resp = openAiEnvelope(
	    R"("{\"found\":true,\"label\":\"登录\",\"bbox_2d\":[250,200,750,600],\"confidence\":0.87}")");
	const VisionAiBox box = VisionAi::parseLocateResponse(resp, 1000, 500);
	ASSERT_TRUE(box.found);
	EXPECT_EQ(box.x, 250);
	EXPECT_EQ(box.y, 100);
	EXPECT_EQ(box.w, 500);
	EXPECT_EQ(box.h, 200);
	EXPECT_DOUBLE_EQ(box.confidence, 0.87);
	EXPECT_EQ(box.label, "登录");
	EXPECT_TRUE(VisionAi::lastError().empty());
}

TEST(VisionAiLocateProtocol, ParsePlainObjectWithoutEnvelope) {
	// provider 直接回对象（非 OpenAI 包装）也要能解析
	const std::string resp =
	    R"({"found":true,"label":"ok","bbox_2d":[0,0,1000,1000],"confidence":0.5})";
	const VisionAiBox box = VisionAi::parseLocateResponse(resp, 800, 600);
	ASSERT_TRUE(box.found);
	EXPECT_EQ(box.x, 0);
	EXPECT_EQ(box.y, 0);
	EXPECT_EQ(box.w, 800);
	EXPECT_EQ(box.h, 600);
}

TEST(VisionAiLocateProtocol, ParseStripsMarkdownFenceAndProse) {
	const std::string content =
	    "```json\n" + std::string(R"({"found":true,"label":"btn","bbox_2d":[100,100,300,300],"confidence":0.9})")
	    + "\n```\n以上是识别结果。";
	// content 是模型回的 JSON 文本（含围栏+尾随中文杂文本），dump() 产物是完整
	// JSON 字符串字面量（自带引号），整体嵌进 envelope 的 "content": 位
	const std::string payload = nlohmann::json(content).dump();
	const VisionAiBox box = VisionAi::parseLocateResponse(openAiEnvelope(payload), 1000, 1000);
	ASSERT_TRUE(box.found);
	EXPECT_EQ(box.x, 100);
	EXPECT_EQ(box.w, 200);
}

TEST(VisionAiLocateProtocol, ParseNotFound) {
	const std::string resp = openAiEnvelope(
	    R"({"found":false,"label":"","bbox_2d":[0,0,0,0],"confidence":0.0})");
	const VisionAiBox box = VisionAi::parseLocateResponse(resp, 1000, 500);
	EXPECT_FALSE(box.found);
	EXPECT_TRUE(VisionAi::lastError().empty());
}

TEST(VisionAiLocateProtocol, ParseRejectsBadJson) {
	const VisionAiBox box = VisionAi::parseLocateResponse("not json at all", 1000, 500);
	EXPECT_FALSE(box.found);
	EXPECT_FALSE(VisionAi::lastError().empty());
}

TEST(VisionAiLocateProtocol, ParseRejectsFoundWithoutBbox) {
	const std::string resp = openAiEnvelope(R"({"found":true,"label":"x","confidence":0.9})");
	const VisionAiBox box = VisionAi::parseLocateResponse(resp, 1000, 500);
	EXPECT_FALSE(box.found);
	EXPECT_NE(VisionAi::lastError().find("bbox_2d"), std::string::npos);
}

TEST(VisionAiLocateProtocol, ParseRejectsDegenerateBox) {
	const std::string resp = openAiEnvelope(
	    R"({"found":true,"label":"x","bbox_2d":[500,500,500,500],"confidence":0.9})");
	const VisionAiBox box = VisionAi::parseLocateResponse(resp, 1000, 500);
	EXPECT_FALSE(box.found);
	EXPECT_NE(VisionAi::lastError().find("degenerate"), std::string::npos);
}

TEST(VisionAiLocateProtocol, ParseRejectsInvalidFrameSize) {
	const std::string resp = openAiEnvelope(
	    R"({"found":true,"label":"x","bbox_2d":[0,0,100,100],"confidence":0.9})");
	const VisionAiBox box = VisionAi::parseLocateResponse(resp, 0, 0);
	EXPECT_FALSE(box.found);
	EXPECT_NE(VisionAi::lastError().find("frame size"), std::string::npos);
}

TEST(VisionAiLocateProtocol, LocateWithoutConfigFailsWithHint) {
	VisionAi::reset();
	const VisionAiBox box = VisionAi::locate("登录按钮");
	EXPECT_FALSE(box.found);
	EXPECT_NE(VisionAi::lastError().find("aiSetup"), std::string::npos);
}

// ===== 截帧注入（Android captureFrame 适配面）=====
// 注入为装配层状态、reset 不清——每个用例自带复位 guard 防跨用例污染

class VisionAiFrameProviderEnv : public ::testing::Test {
protected:
	void SetUp() override { VisionAi::reset(); }
	void TearDown() override {
		VisionAi::setFrameProvider(nullptr);
		VisionAi::reset();
	}
};

TEST_F(VisionAiFrameProviderEnv, InjectedProviderDrivesLocateCapture) {
	VisionAiConfig cfg;
	// 端口 1 立即拒绝：截帧链路验证不外呼
	cfg.baseUrl = "http://127.0.0.1:1";
	cfg.model = "test-model";
	cfg.apiKey = "sk-test";
	cfg.timeoutSeconds = 2;
	VisionAi::setup(cfg);

	int calls = 0;
	int seenW = -1;
	int seenH = -1;
	VisionAi::setFrameProvider([&](const Rect& region) {
		++calls;
		seenW = region.width;
		seenH = region.height;
		return std::make_unique<Bitmap>(4, 2);
	});

	const VisionAiBox box = VisionAi::locate("按钮", Rect(1, 2, 3, 4));
	EXPECT_EQ(calls, 1);
	// region 原样透传给注入提供者（空 region=全屏语义在提供者侧）
	EXPECT_EQ(seenW, 3);
	EXPECT_EQ(seenH, 4);
	// 注入帧尺寸进入解析链路；HTTP 拒绝 → found=false + 可区分错误
	EXPECT_FALSE(box.found);
	EXPECT_FALSE(VisionAi::lastError().empty());
}

TEST_F(VisionAiFrameProviderEnv, ProviderNotCalledWithoutConfig) {
	int calls = 0;
	VisionAi::setFrameProvider([&](const Rect&) {
		++calls;
		return std::make_unique<Bitmap>(4, 2);
	});
	const VisionAiBox box = VisionAi::locate("按钮");
	EXPECT_FALSE(box.found);
	EXPECT_EQ(calls, 0); // 未配置在截帧前短路
	EXPECT_NE(VisionAi::lastError().find("aiSetup"), std::string::npos);
}

} // namespace
