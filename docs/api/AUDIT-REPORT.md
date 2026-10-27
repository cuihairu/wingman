> [中文](../zh/api/AUDIT-REPORT.md) | **English**

# API Audit Report

This document is lengthy (140 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [AUDIT-REPORT.md](../zh/api/AUDIT-REPORT.md) for the original.

- 0. 健康度概览
- 1. 关键查证结论（影响分类判断）
  - 1.1 Python 绑定层不做 snake_case 转换
  - 1.2 Python 引擎默认关闭且 CI 不验证
- 2. 分类标记说明
- 3. P0 严重问题
  - 3.1 文档描述的能力代码未实现（文档有/代码无）
  - 3.2 代码已实现但完全无文档（代码有/文档无）
  - 3.3 文档与代码是两套完全不同的 API（最严重）
- 4. P1 命名/签名问题
  - 4.1 Namespace 标题错误（文档内部自相矛盾）
  - 4.2 Python snake_case 别名系统性失效（§1.1）
  - 4.3 签名严重不符（按文档调用会出错）
- 5. P2 文档组织问题
- 6. 待决策项（影响多条，需产品定夺）
- 7. 一致性良好的模块（无需改动）
