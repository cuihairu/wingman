# Security Policy

## Supported Versions

当前 Croupier Dashboard 以下版本获得安全更新支持：

| Version | Supported          |
| ------- | ------------------ |
| 6.x     | :white_check_mark: |
| < 6.0   | :x:                |

## Reporting a Vulnerability

如果您发现安全漏洞，请通过以下方式负责任地披露：

1. **请勿** 在公开的 GitHub Issues 中报告安全漏洞
2. 发送邮件至项目维护者，或通过 [GitHub Security Advisories](https://github.com/cuihairu/croupier-dashboard/security/advisories/new) 提交报告
3. 请在报告中包含：
   - 漏洞描述
   - 复现步骤
   - 潜在影响
   - 如有可能，提供修复建议

## Response Timeline

- **确认收到**：48 小时内
- **初步评估**：7 个工作日内
- **修复发布**：视漏洞严重程度而定，高危漏洞优先处理

## Security Best Practices

使用 Croupier Dashboard 时，建议遵循以下安全实践：

- 始终使用最新稳定版本
- 定期更新前端依赖项（npm audit）
- 在生产环境中启用 HTTPS
- 配置合适的 CSP（Content Security Policy）
- 妥善保管 API 密钥和认证凭据

## Known Vulnerabilities

当前仍有 1 个低危开发依赖告警来自上游框架链路，暂无官方补丁版本。

### elliptic (低危 - GHSA-848j-6mx2-7j84)

- **影响**: 加密原语实现问题
- **路径**: `@umijs/max > umi > @umijs/bundler-webpack > node-libs-browser > crypto-browserify > browserify-sign/create-ecdh > elliptic`
- **状态**: `elliptic` npm 最新版本仍为 `6.6.1`，GitHub advisory 的 patched version 为 `<0.0.0`
- **缓解措施**: 该链路来自 Umi 构建工具的 Node polyfill，当前应用代码不直接调用 `elliptic`

### 修复措施

- 已移除仅用于旧脚手架/开发辅助的漏洞依赖链，包括 `mockjs`、`@ant-design/pro-cli` 和 `umi-presets-pro`。
- 已将安全版本约束迁移到 `package.json` 的 `pnpm.overrides`，确保 pnpm 生成 lockfile 时实际生效。
- 持续监控上游框架依赖更新；如上游发布补丁版本，应优先升级并重新运行依赖审计。
