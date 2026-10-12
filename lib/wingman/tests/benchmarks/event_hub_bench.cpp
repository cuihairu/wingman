// EventHub 吞吐基准：同步 exact-type 分发是系统事件面（桥接/触发/宏状态）
// 的公共路径，这里量 emit 端到端成本与订阅 churn。
// 进程级单例：各段之间 clear() 隔离。

#include "bench.hpp"

#include <wingman/event.hpp>

#include <nlohmann/json.hpp>

#include <string>

using namespace wingman;
using wingman::bench::Bench;

int main() {
    auto& hub = EventHub::instance();
    Bench b("event_hub");

    b.header("emit 到 1 订阅者（空负载）");
    hub.subscribe("bench.one", [](const EventMessage&) {});
    b.run("emit empty payload, 1 subscriber", [&] {
        hub.emit("bench.one");
    }, 100000);

    b.header("emit 到 10 订阅者（空负载）");
    for (int i = 0; i < 9; ++i) {
        hub.subscribe("bench.ten", [](const EventMessage&) {});
    }
    b.run("emit empty payload, 10 subscribers", [&] {
        hub.emit("bench.ten");
    }, 100000);

    b.header("emit 携带 1KB 负载");
    const std::string kb(1024, 'x');
    const nlohmann::json payload = {{"data", kb}, {"seq", 1}};
    hub.subscribe("bench.payload", [](const EventMessage&) {});
    b.run("emit 1KB payload, 1 subscriber", [&] {
        hub.emit("bench.payload", payload);
    }, 50000);

    b.header("订阅注册/注销 churn");
    b.run("subscribe + unsubscribe cycle", [&] {
        const auto id = hub.subscribe("bench.churn", [](const EventMessage&) {});
        hub.unsubscribe(id);
    }, 20000);

    hub.clear();
    std::cout << "\ndone\n";
    return 0;
}
