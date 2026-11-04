---
title: 配置开关效果
---

> [English](../../guide/config-effects.md) | **中文**

# 配置开关效果

本页展示 Runtime GUI 设置页各开关的 **关闭 / 开启** 视觉状态与切换动画。截图与
GIF 由可重跑流水线 `apps/gui/scripts/capture-config-gifs.mjs` 生成；设置页布局或
开关语义变化时重跑并提交产物 diff，兼作视觉回归。

## 前置

- GUI dev server 运行中：`cd apps/gui && pnpm dev`（默认 `http://localhost:5173`）
- `playwright-core` 已安装（`pnpm add -D playwright-core`）、`~/.cache/ms-playwright`
  下有本地 Chromium、GIF 合成需 `magick` 或 `ffmpeg`

## 重跑

```bash
cd apps/gui
node scripts/capture-config-gifs.mjs
```

## 开关

### 自动重连

GUI 与 Runtime 的 IPC 连接断开后自动重试。

![自动重连 关闭](/config-effects/auto-reconnect-off.png)
![自动重连 开启](/config-effects/auto-reconnect-on.png)
![自动重连 切换](/config-effects/auto-reconnect.gif)

### 启动后自动加载当前配置脚本

GUI 启动后自动加载并运行当前激活配置的脚本。

![自动启动 关闭](/config-effects/auto-start-off.png)
![自动启动 开启](/config-effects/auto-start-on.png)
![自动启动 切换](/config-effects/auto-start.gif)

### 启动时最小化到托盘

GUI 启动后最小化到系统托盘。

![最小化到托盘 关闭](/config-effects/minimize-to-tray-off.png)
![最小化到托盘 开启](/config-effects/minimize-to-tray-on.png)
![最小化到托盘 切换](/config-effects/minimize-to-tray.gif)

### Challenge 注册鉴权

开启后，远程注册走 HMAC 挑战应答流程，注册令牌明文不再经过网络。

![Challenge 注册鉴权 关闭](/config-effects/challenge-auth-off.png)
![Challenge 注册鉴权 开启](/config-effects/challenge-auth-on.png)
![Challenge 注册鉴权 切换](/config-effects/challenge-auth.gif)
