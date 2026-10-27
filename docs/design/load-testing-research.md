> [中文](../zh/design/load-testing-research.md) | **English**

# Load Testing Research

This document is lengthy (251 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [load-testing-research.md](../zh/design/load-testing-research.md) for the original.

- 1. 结论速览
- 2. 调研：加载协议做压测的机制
  - 2.1 KBEngine：entitydef 协议事实源 + bots 压测组件
  - 2.2 gRPC 反射与 protobuf 自描述
  - 2.3 自定义帧协议：pcap 回放为什么不够
  - 2.4 接入方式小结
- 3. 复杂度分级：六层拆解
- 4. 成熟框架横向参照
  - 4.1 五个框架对比
  - 4.2 分布式架构三种形态
  - 4.3 指标汇聚的硬口径
- 5. wingman 现状：可复用资产与缺口
  - 5.1 可复用资产
  - 5.2 缺口（对照六层）
- 6. 目标形态评估与分期
  - 6.1 两级目标形态
  - 6.2 架构约束（不可触碰）
  - 6.3 分期路线与工作量
- 7. 拍板建议
- 8. 参考资料
