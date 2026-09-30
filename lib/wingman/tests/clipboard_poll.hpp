#pragma once

// 剪贴板写后查询的轮询 helper（配合 ClipboardLockGuard 使用）。
//
// X11 后端（x11_clipboard.cpp）每次 setText/clear 都 fork 一个 xclip 并在
// 其 daemon 化后接管 CLIPBOARD selection——setText 返回 true 只表示子进程
// 已启动，所有权转移是异步的。写后立即查询（getText/isEmpty/hasText/…）
// 可能命中前一写入残留的旧 owner：全套件高负载下多次实测读回上一用例的
// 旧内容、clear 后 isEmpty 仍为 false 的偶发 flaky（本批与第六批均有记录）。
//
// 轮询等待谓词成立而非定值睡眠：命中即真的后端（Windows openClipboard
// 互斥、内存 stub）首次查询即返回，零等待。
//
// 断言方向纪律：写后/clear 后的断言一律正向等待终态（EXPECT_TRUE(waitFor
// 终态谓词)），不要写 EXPECT_FALSE(waitFor(pred))——后者要求首次读取即为
// 终态，等于与异步接管窗口竞争：接管未完成时旧 owner（或 clear 建立的空态）
// 仍在位，t=0 读取即命中相反状态而假红（2026-09-30 全量门禁
// ClipboardTest.IsEmpty 在 load ~27 下实测，同型 5 处一并收口）。

#include <chrono>
#include <thread>

namespace clipboard_test {

template <typename Pred>
bool waitFor(Pred&& pred, int attempts = 40) {
    for (int i = 0; i < attempts; ++i) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return pred(); // 最终值交给调用方断言报告差异
}

} // namespace clipboard_test
