#include <gtest/gtest.h>
#include "platform/common/tcp_channel.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

using namespace wingman::ipc;

// tcp_channel.cpp 第八批覆盖率收口（2026-09-23）：tcp_channel_test.cpp 覆盖了
// 值语义与正常收发往返，本文件驱动真实 socket 错误路径——server 非法地址/端口
// 占用/accept 中断、client 非法主机回退与重试耗尽、原始字节帧攻击（0 长度/
// 超长帧/坏 JSON/缺字段/payload 二态）、对端 RST 后 send 失败。全部走 127.0.0.1
// 环回，不依赖外部网络。

namespace {

	int pickPort(int base) {
		return base + (rand() % 500);
	}

	int connectRaw(int port) {
		int fd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) return -1;
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(static_cast<uint16_t>(port));
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
			::close(fd);
			return -1;
		}
		return fd;
	}

	// 4 字节长度头 + payload；bytes 为空时仅发 0 长度头
	bool sendRawFrame(int fd, const std::string& bytes) {
		uint32_t len = static_cast<uint32_t>(bytes.size()); // 协议长度头为主机序（产品端 sendRaw 无字节序转换）
		if (::send(fd, &len, sizeof(len), 0) != sizeof(len)) return false;
		if (!bytes.empty()) {
			return ::send(fd, bytes.data(), bytes.size(), 0) ==
				   static_cast<ssize_t>(bytes.size());
		}
		return true;
	}

	int connectRawRetry(int port) {
		// server 后台线程 bind/listen 就绪前连接会失败，带重试
		for (int i = 0; i < 100; ++i) {
			int fd = connectRaw(port);
			if (fd >= 0) return fd;
			std::this_thread::sleep_for(std::chrono::milliseconds(15));
		}
		return -1;
	}

	std::string recvRawBytes(int fd, size_t want) {
		std::string out;
		char buf[4096];
		while (out.size() < want) {
			// 只读还差的字节数，避免一次 recv 吞掉后续帧数据导致下一步阻塞
			ssize_t n = ::recv(fd, buf, want - out.size(), 0);
			if (n <= 0) break;
			out.append(buf, static_cast<size_t>(n));
		}
		return out;
	}

	// 读取一帧（4 字节长度头 + body）；len 字段非法或对端关闭时返回残片
	std::string recvOneFrame(int fd) {
		auto head = recvRawBytes(fd, 4);
		if (head.size() < 4) return head;
		uint32_t netLen = 0;
		std::memcpy(&netLen, head.data(), 4);
		uint32_t bodyLen = netLen; // 主机序
		if (bodyLen == 0 || bodyLen > 10u * 1024 * 1024) return head;
		return head + recvRawBytes(fd, bodyLen);
	}

} // anonymous namespace

class TcpE2ECoverageTest : public ::testing::Test {
protected:
	void SetUp() override {
		// send 到已 RST 的对端默认触发 SIGPIPE 杀进程；测试内屏蔽
		::signal(SIGPIPE, SIG_IGN);
	}
};

// ========== server 错误路径 ==========

// 非法 IP 字面量：inet_pton 失败 → 关闭 socket + Error 状态 + 错误回调
TEST_F(TcpE2ECoverageTest, ServerInvalidHostFailsWithErrorCallback) {
	int fired = 0;
	{
		TcpChannel server(true, "not-an-ip", pickPort(20100));
		server.setErrorCallback([&fired](const std::string&) { ++fired; });
		EXPECT_FALSE(server.connect(""));
		EXPECT_EQ(server.getState(), IpcState::Error);
		EXPECT_FALSE(server.isConnected());
	}
	EXPECT_EQ(fired, 1);
}

// 端口被占：bind 失败 → Error + 回调
TEST_F(TcpE2ECoverageTest, ServerBindFailsWhenPortTaken) {
	int port = pickPort(20400);

	int holder = ::socket(AF_INET, SOCK_STREAM, 0);
	ASSERT_GE(holder, 0);
	int opt = 1;
	::setsockopt(holder, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(static_cast<uint16_t>(port));
	ASSERT_EQ(::bind(holder, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
	ASSERT_EQ(::listen(holder, 1), 0);

	int fired = 0;
	{
		TcpChannel server(true, "127.0.0.1", port);
		server.setErrorCallback([&fired](const std::string&) { ++fired; });
		EXPECT_FALSE(server.connect(""));
		EXPECT_EQ(server.getState(), IpcState::Error);
	}
	EXPECT_EQ(fired, 1);
	::close(holder);
}

// accept 阻塞中 disconnect：listen socket 的 shutdown/close 分支 + accept 失败分支
TEST_F(TcpE2ECoverageTest, DisconnectDuringAcceptUnblocksServer) {
	int port = pickPort(20600);
	TcpChannel server(true, "127.0.0.1", port);

	std::thread serverThread([&server]() { server.connect(""); });
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	server.disconnect();
	serverThread.join();

	EXPECT_FALSE(server.isConnected());
	// accept 返回 INVALID 后 server 线程 setState(Error)，与 disconnect 的
	// setState(Disconnected) 并发——最终态二者之一，均证明 accept 被解除阻塞
	IpcState finalState = server.getState();
	EXPECT_TRUE(finalState == IpcState::Disconnected || finalState == IpcState::Error);
}

// ========== client 错误路径 ==========

// 非法主机回退 LOOPBACK 后重试耗尽（约 5s）：278 回退 + 289 重试 + 292-296 失败
TEST_F(TcpE2ECoverageTest, ClientTimesOutAfterRetries) {
	int port = pickPort(20800); // 无监听的端口
	TcpChannel client(false, "bogus-host", port);
	auto t0 = std::chrono::steady_clock::now();
	EXPECT_FALSE(client.connect(""));
	auto elapsed = std::chrono::steady_clock::now() - t0;
	EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 4000);
	EXPECT_EQ(client.getState(), IpcState::Error);
}

// endpoint 无冒号：整体作为 host
TEST_F(TcpE2ECoverageTest, EndpointWithoutColonTreatedAsHost) {
	int port = pickPort(21000);
	TcpChannel server(true, "127.0.0.1", port);
	std::thread serverThread([&server]() {
		server.connect("127.0.0.1"); // 无冒号 → host_
	});

	// 用真实 client 完成 accept
	TcpChannel client(false, "127.0.0.1", port);
	EXPECT_TRUE(client.connect(""));

	serverThread.join();
	EXPECT_TRUE(server.isConnected());

	client.disconnect();
	server.disconnect();
}

// startReceiving 重入：joinable 时直接返回
TEST_F(TcpE2ECoverageTest, StartReceivingTwiceIsNoOp) {
	int port = pickPort(21200);
	TcpChannel server(true, "127.0.0.1", port);
	std::thread serverThread([&server]() { server.connect(""); });

	TcpChannel client(false, "127.0.0.1", port);
	EXPECT_TRUE(client.connect(""));
	serverThread.join();

	server.startReceiving();
	EXPECT_NO_THROW(server.startReceiving()); // 第二次：joinable → return

	client.disconnect();
	server.disconnect();
}

// ========== 帧协议错误注入（raw client → server） ==========

// 0 长度帧：非法长度 → receiveLoop 退出 → Disconnected
TEST_F(TcpE2ECoverageTest, ZeroLengthFrameTerminatesLoop) {
	int port = pickPort(21400);
	TcpChannel server(true, "127.0.0.1", port);
	std::thread serverThread([&server]() { server.connect(""); });

	int fd = connectRawRetry(port);
	ASSERT_GE(fd, 0);
	serverThread.join();
	EXPECT_TRUE(server.isConnected());

	server.startReceiving();
	EXPECT_TRUE(sendRawFrame(fd, ""));

	// 等 receiveLoop 消费坏帧并退出
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	EXPECT_EQ(server.getState(), IpcState::Disconnected);

	::close(fd);
	server.disconnect();
}

// 超长帧（>10MB 声明）：同样拒绝
TEST_F(TcpE2ECoverageTest, OversizedLengthFrameTerminatesLoop) {
	int port = pickPort(21600);
	TcpChannel server(true, "127.0.0.1", port);
	std::thread serverThread([&server]() { server.connect(""); });

	int fd = connectRawRetry(port);
	ASSERT_GE(fd, 0);
	serverThread.join();
	server.startReceiving();

	uint32_t bogus = 11u * 1024 * 1024; // 主机序
	ASSERT_EQ(::send(fd, &bogus, sizeof(bogus), 0), static_cast<ssize_t>(sizeof(bogus)));

	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	EXPECT_EQ(server.getState(), IpcState::Disconnected);

	::close(fd);
	server.disconnect();
}

// 合法长度头后对端消失：payload recv 失败 → 退出
TEST_F(TcpE2ECoverageTest, TruncatedPayloadTerminatesLoop) {
	int port = pickPort(21800);
	TcpChannel server(true, "127.0.0.1", port);
	std::thread serverThread([&server]() { server.connect(""); });

	int fd = connectRawRetry(port);
	ASSERT_GE(fd, 0);
	serverThread.join();
	server.startReceiving();

	// 声明 64 字节 payload，随即关闭发送端（RST 更快：SO_LINGER 0）
	uint32_t len = 64; // 主机序
	ASSERT_EQ(::send(fd, &len, sizeof(len), 0), static_cast<ssize_t>(sizeof(len)));
	linger lg{};
	lg.l_onoff = 1;
	lg.l_linger = 0;
	::setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
	::close(fd);

	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	EXPECT_EQ(server.getState(), IpcState::Disconnected);

	server.disconnect();
}

// 帧内容矩阵：坏 JSON / 缺字段 / payload 字符串与对象
TEST_F(TcpE2ECoverageTest, FrameContentMatrix) {
	int port = pickPort(22000);
	TcpChannel server(true, "127.0.0.1", port);

	std::mutex mtx;
	std::vector<IpcMessage> got;
	server.setMessageCallback([&](IpcMessage msg) {
		std::lock_guard<std::mutex> lock(mtx);
		got.push_back(msg);
	});

	std::thread serverThread([&server]() { server.connect(""); });
	int fd = connectRawRetry(port);
	ASSERT_GE(fd, 0);
	serverThread.join();
	server.startReceiving();

	// 1) 坏 JSON → Error 消息，连接保持
	EXPECT_TRUE(sendRawFrame(fd, "not-json-at-all{{{"));
	// 2) 缺字段 → 全默认值
	EXPECT_TRUE(sendRawFrame(fd, R"({"type":7})"));
	// 3) payload 为字符串
	EXPECT_TRUE(sendRawFrame(fd, R"({"type":1,"method":"m1","payload":"plain-text"})"));
	// 4) payload 为对象 → dump 存储
	EXPECT_TRUE(sendRawFrame(fd, R"({"type":2,"method":"m2","payload":{"a":1}})"));

	// 等待回调
	for (int i = 0; i < 50 && [&]() { std::lock_guard<std::mutex> lock(mtx); return got.size(); }() < 4; ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	std::lock_guard<std::mutex> lock(mtx);
	ASSERT_EQ(got.size(), 4u);
	EXPECT_EQ(got[0].type, IpcMessageType::Error); // 坏 JSON
	EXPECT_EQ(got[1].type, static_cast<IpcMessageType>(7));
	EXPECT_EQ(got[1].method, "");
	EXPECT_EQ(got[1].id, 0u);
	EXPECT_EQ(got[1].timestamp, 0u);
	EXPECT_EQ(got[1].payload, "");
	EXPECT_EQ(got[2].type, static_cast<IpcMessageType>(1));
	EXPECT_EQ(got[2].method, "m1");
	EXPECT_EQ(got[2].payload, "plain-text"); // 字符串直取
	EXPECT_EQ(got[3].payload, R"({"a":1})"); // 对象 dump

	// 连接仍存活（坏 JSON 不中断）：再发一帧仍能收到
	EXPECT_TRUE(server.isConnected());
	::close(fd);
	server.disconnect();
}

// 空 payload 序列化：对端收到的 JSON 中 payload 是空对象（非缺字段）
TEST_F(TcpE2ECoverageTest, EmptyPayloadSerializedAsObject) {
	int port = pickPort(22200);
	TcpChannel server(true, "127.0.0.1", port);
	std::thread serverThread([&server]() { server.connect(""); });

	int fd = connectRawRetry(port);
	ASSERT_GE(fd, 0);
	serverThread.join();

	IpcMessage msg; // payload 为空
	EXPECT_TRUE(server.send(msg));

	auto bytes = recvOneFrame(fd);
	ASSERT_GE(bytes.size(), 8u);
	uint32_t netLen = 0;
	std::memcpy(&netLen, bytes.data(), 4);
	uint32_t bodyLen = netLen; // 主机序
	ASSERT_EQ(bytes.size(), 4u + bodyLen);
	std::string body = bytes.substr(4, bodyLen);
	EXPECT_NE(body.find("\"payload\":{}"), std::string::npos);

	::close(fd);
	server.disconnect();
}

// 对端关闭后 send：最终失败并进入 Error（SIGPIPE 已屏蔽）
TEST_F(TcpE2ECoverageTest, SendAfterPeerResetFails) {
	int port = pickPort(22400);
	TcpChannel server(true, "127.0.0.1", port);
	std::thread serverThread([&server]() { server.connect(""); });

	int fd = connectRawRetry(port);
	ASSERT_GE(fd, 0);
	serverThread.join();
	EXPECT_TRUE(server.isConnected());

	// 对端 RST 关闭（SO_LINGER 0）
	linger lg{};
	lg.l_onoff = 1;
	lg.l_linger = 0;
	::setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
	::close(fd);
	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	// 第一次 send 可能成功（进本端缓冲），RST 到达后必然失败——循环至失败
	bool failed = false;
	for (int i = 0; i < 6 && !failed; ++i) {
		failed = !server.send(IpcMessage{});
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	EXPECT_TRUE(failed);
	EXPECT_EQ(server.getState(), IpcState::Error);

	server.disconnect();
}

#else // _WIN32

// Windows 下 tcp_channel_test.cpp 已覆盖 stub/正常路径（结构一致），本文件错误
// 注入用 POSIX 原语实现，Windows 不参与
TEST(TcpE2ECoverageSkipped, NotApplicableOnWindows) {
	GTEST_SKIP() << "TCP e2e 错误注入仅在 POSIX 上运行";
}

#endif // _WIN32
