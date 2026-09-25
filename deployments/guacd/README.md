# guacd 部署（像素面）

`docker-compose.yml` 拉起 guacd 协议翻译守护进程（Apache Guacamole 1.5.5），
供 orchestrator 的 Guacamole 网关反代（`internal/handlers/guacamole.go`）。
设计文档：`docs/remote-gateway-guacamole-design.md`。

## 版本锁定：只用 1.5.x

官方 1.6.0 镜像（含 2026-02 的 latest 重建，Alpine 包 1.6.0-r3）存在 RDP
双崩溃（e2e gdb 实测，栈均出自 libguac.so.25）：

1. `guac_audio_assign_encoder` 空指针——镜像 libguac 未链接音频编码器
   （opus/flac），RDP 音频流（RDPSND）协商即崩；
2. `guac_user_supports_webp` 空指针——1.6.0 display 重构后显示线程探测
   WebP 能力时崩。

两者均无客户端侧规避参数，锁定 1.5.5 直至上游修复。网关侧已对 1.5.5 显式
`disable-audio`（见 `guacParamTable` 的 rdp 分支注释）。

## 使用

```bash
cd deployments/guacd
podman compose up -d    # 或 docker compose up -d
```

Go server 经 `WINGMAN_GUACD_ADDR`（默认 `127.0.0.1:4822`）连接。

## 阶段二：文件传输与录制（共享卷）

compose 里两个 bind 挂载对应网关注入的 connect 参数（设计 §15/§16）：

| 卷（容器内路径） | 用途 | server 侧环境变量 |
| --- | --- | --- |
| `./drive` → `/wingman-drive` | RDP 驱动器重定向（上传的文件出现在远端"计算机"共享盘） | `WINGMAN_GUACD_DRIVE_PATH=/wingman-drive` |
| `./recordings` → `/recordings` | 会话录像（`.mjs`，guacd 原生格式）写入 | `WINGMAN_GUACD_RECORDING_PATH=/recordings` |

- SSH 的文件传输走 SFTP 通道（`enable-sftp=true`），**不落 guacd 本地盘**，
  无需 drive 卷；
- 录像检索：server 侧把同一 `recordings` 目录挂到
  `WINGMAN_RECORDING_DIR`（例如宿主机同路径 `/var/lib/wingman/recordings`），
  经 `/api/remote/recordings` 提供 list/download/delete；
- `.mjs` 回放用官方 `guacenc` 离线转 mp4（浏览器内回放列为远期）；
- 录像安全默认：网关强制 `recording-include-keys=false`，**按键内容永不
  入录像**（口令不会以明文出现在录像里）。

## 安全约定

- 端口只绑 `127.0.0.1`：浏览器/远程客户端只连 Go server 的
  `/api/remote/guacamole` WS 端点，**不直连 guacd**（架构硬约束）；
- `GUACD_LOG_LEVEL=info`：debug 级别会打印 `connect` 指令参数（含 RDP/VNC
  口令），禁止调高；
- guacd 无状态：崩溃后重启即可恢复，连接票据重申请。

## 端到端联调（三协议链路测试）

`orchestrator/server/integration/guacd_e2e_test.go` 提供 SSH/VNC/RDP 三条
链路用例，目标端点栈见 `orchestrator/server/integration/testdata/`：

```bash
podman compose -f orchestrator/server/integration/testdata/guacd-e2e-compose.yml up -d
cd orchestrator/server
WINGMAN_GUACD_E2E=1 go test ./integration/ -run TestGuacdE2E -v
```
