# Security Policy

## Supported Versions

当前 `orchestrator/dashboard` 以仓库主分支的当前版本为准，不单独维护旧版本安全支持矩阵。

## Reporting a Vulnerability

如果你发现安全问题，请不要在公开 Issue 中披露细节。

建议至少提供以下信息：

- 漏洞描述
- 复现步骤
- 影响范围
- 修复建议或缓解方式

当前仓库未维护独立的历史前端安全披露入口；请按 Wingman 项目当前维护方式处理。

## Security Notes

- 前端依赖安全性以 `pnpm audit` 结果为准
- 服务端 JWT 密钥必须通过 `WINGMAN_JWT_SECRET` 配置，且长度不少于 32 个字符
- 生产环境应启用 HTTPS，并限制可信 CORS 来源
- `debugger` 相关接口当前要求登录且具备 `admin` 角色
