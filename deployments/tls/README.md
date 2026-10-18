# Agent TCP 链路 TLS 终结（形态 A）

`docker-compose.yml` + `nginx-stream-tls.conf` 给 agent 接入链路（runtime ⇄
Go server 的自定义裸 TCP 8888）叠加一层 TLS：nginx `stream{}` 在 9443 终结
TLS、四层透传到 `wingman-server:8888`。选型与定案见
`docs/agent-token-auth-design.md` §6.3（形态 A：部署形态，零代码；形态 B：
帧协议原生 TLS + 证书 PIN，公网生产前置条件，待立项）。

## 保护边界（先读这个）

- **防**：链路上的被动嗅探——脚本内容、命令载荷、上报事件明文过网的问题。
- **不防**：主动 MITM。agent 侧不校验证书（帧协议无 SNI/校验钩子，形态 B
  才引入证书 PIN）。token 侧已有 challenge-response 兜底：开启
  `[remote] challenge_auth` 后 token 本身 HMAC 化不过网，MITM 拿不到
  token，但命令/脚本内容可被替换——内网/演示部署可接受，公网生产等形态 B。
- Dashboard 的 HTTPS 与本文件无关（Cloudflare/反代层解决）。

## 文件

| 文件 | 说明 |
|------|------|
| `docker-compose.yml` | 起 nginx（官方镜像 stream 模块静态编译，无需额外模块包） |
| `nginx-stream-tls.conf` | `stream{}` TLS 终结 + 转发；upstream 默认指演示栈的 `wingman-server:8888` |
| `certs/`（不入库） | `agent-tls.crt` / `agent-tls.key`，自签或 CA 均可 |

## 证书

自签（无域名，agent 不校验证书所以 CN 随意）：

```bash
mkdir -p certs && openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
  -subj "/CN=wingman-agent-gw" -keyout certs/agent-tls.key -out certs/agent-tls.crt
```

有域名且想给形态 B 预留正规证书：caddy（`tls` 指令，DNS challenge 免开
80）或 certbot 签发后把证书放进 `certs/` 同名替换即可。

## 接线

**演示栈（deployments/docker/）**：本 compose 加入同一 external
`wingman-net`，不改动原栈。宿主直连 8888 的旧入口与 9443 TLS 入口并存。

**独立部署的 Go server（非 compose）**：把 `nginx-stream-tls.conf` 的
upstream 行改成实际地址（如 `server 10.0.0.5:8888;`），compose 里去掉
`networks` 两段，`--network host` 或照常映射端口均可；不想用 compose 的话
stunnel 同理（`accept = 9443`, `connect = host:8888`, cert/key 同对）。

**runtime 侧（桌面）**：AgentConfig TOML `[remote] server_port` 从 8888 改
9443（`server_ip` 不变，指 TLS 网关所在机器）；GUI「远程注册配置」同理。

**Android**：configJson 连接地址端口同改。

改完重启 runtime/agent，Dashboard Agents 页看到 agent 重新上线即通。

## 验证记录

2026-10-19 本机 e2e（Linux，非 CI 门禁）：本地构建 Go server 起 agent
listener `127.0.0.1:18888`，nginx:1.27-alpine 按本目录配置（upstream 改指
18888）监听 `:28888` TLS（自签证书）；裸字节按 C++ MessageHeader 16B 布局
（`orchestrator/server/integration/protocol_test.go` 同款）经 `openssl
s_client` 发 `agent.register`（Notify 帧）与 `agent.heartbeat`，均收到
`register_ack` / 事件侧 ack——TLS 终结 + 帧透传全链路字节级兼容确认。

## 已知限制

- 单网关无高可用（与演示栈同级形态；形态 B 立项时一并考虑）。
- `proxy_timeout 2h`：长驻连接按 agent 30s 心跳续着，不会触超时；若改小
  心跳间隔无需动本层。
- 证书轮换：替换 `certs/` 下文件后 `docker compose restart agent-tls`。
