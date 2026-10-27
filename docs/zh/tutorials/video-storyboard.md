> [English](../../tutorials/video-storyboard.md) | **中文**

# 视频教程系列脚本

ROADMAP M8.2「视频教程」的前期交付物：六集分镜脚本 + 逐字旁白（可直接照读）。
实拍录制需在真实 Windows 环境操作 GUI 与浏览器，由人工完成；本文保证录制者
不需要再理解产品即可开机录。

## 系列约定

- 受众：第一次接触 Wingman 的使用者（不假设会 Go/C++ 开发）。
- 每集 ≤ 8 分钟，一集只讲一条主线；操作失误直接剪掉，不回滚重演。
- 旁白用简短陈述句，无营销话术；术语与 docs/guide 一致（Runtime / GUI /
  中控 / Agent / 触发器 / 宏）。
- 录制规格：1920×1080、60fps；鼠标移动放慢，点击处停留 ≥1s；全程 100% 缩放。
- 片头统一：logo 3s + 本集标题；片尾统一：下一集预告 5s + 文档链接字幕。
- 文件命名：`ep01-install-first-run.mp4`（小写连字符），字幕随视频同发 SRT。
- 演示脚本一律用 examples/lua_scripts/ 下已有文件（hello.lua、color_detect.lua
  等），录制时不要临场手打新脚本。

## 剧集总览

| 集 | 文件名 | 主线 | 对应文档 | 时长目标 |
|----|--------|------|----------|----------|
| 1 | ep01-install-first-run | 安装、启动、跑通第一个脚本 | docs/guide/getting-started.md | 6 min |
| 2 | ep02-runtime-gui-tour | Runtime GUI 各页面功能 | docs/guide/runtime-gui.md | 7 min |
| 3 | ep03-connect-orchestrator | Agent 出站注册到中控 | docs/guide/getting-started.md「运行模式」 | 5 min |
| 4 | ep04-first-script | 用示例脚本做像素检测 | docs/guide/script-development.md | 6 min |
| 5 | ep05-triggers-macros | 触发器配置与宏录制回放 | docs/guide/runtime-gui.md、docs/guides/triggers.md | 7 min |
| 6 | ep06-dashboard-remote | Dashboard 下发脚本与远程接管 | docs/guide/dashboard.md | 7 min |

## EP01 安装与首次启动

前置：干净 Windows 10/11 虚拟机，已下载 Release 产物（安装包与便携 ZIP 各一份）。

| # | 画面 | 操作 | 旁白（逐字） |
|---|------|------|--------------|
| 1 | 桌面 | 打开 Releases 页 | 「Wingman 提供两种发行形态：安装包和便携版。」 |
| 2 | 下载目录 | 双击 InnoSetup 安装包，默认下一步走完 | 「安装包装到 Program Files；便携版解压即用。」 |
| 3 | 开始菜单 | 启动 Runtime | 「Runtime 是本机执行器，首次启动会生成默认 agent.toml。」 |
| 4 | Runtime 日志 | 指出 outbound 连接尝试日志 | 「Runtime 只主动向外连接中控，本机不开控制端口。」 |
| 5 | 命令行 | `wingman-agent.exe script examples\lua_scripts\hello.lua` | 「命令行可以直接跑 Lua 脚本，不需要中控。」 |
| 6 | 输出窗口 | 展示屏幕尺寸输出 | 「hello.lua 调了内置 screen 模块，打印当前屏幕分辨率。」 |
| 7 | 便携 ZIP | 解压到 D 盘，运行同一个脚本 | 「便携版与安装版行为一致，适合 U 盘携带。」 |

### EP01 命令行镜头的机器侧预采（零注入实测配方）

EP01 第 5-6 镜（`wingman-agent.exe script ...hello.lua` 及其输出）可机器侧预采，
不必人工录：全流程只用进程创建与文件操作，不点不敲，约 2 分钟出片。
以下为 2026-10-20 三次探针 + 两轮试录实测出的配方。

**前置自检（缺一即停）**

- `wingman-agent.exe` 就位：下载公开 nightly 资产
  `wingman-agent-<date>-nightly-<sha>-windows-x64.zip`（zip 根下即
  `wingman-agent.exe` + `VERSION.txt`），不 clone、不构建。
- **桌面必须处于解锁态**：机器空闲自动锁屏，锁屏期间 gdigrab 只能采到锁屏画面
  —— 规格全达标但内容不可用，这是两次废片的唯一原因。判据：
  `Get-Process LockApp` 非空即已锁定。
- ffmpeg 可用（PATH 或本机已装的 bin 路径）。
- 1920×1080、100% 缩放、60fps，对齐系列约定。

**执行（按序）**

1. 门控探针：`Get-Process LockApp,LogonUI` 均无进程才继续（UNLOCKED）；
   有任一进程即锁定，停止并先解锁，不要尝试解锁动作本身。
2. 起录（后台）：`ffmpeg -f gdigrab -framerate 60 -i desktop -c:v libx264 -preset veryfast -pix_fmt yuv420p <out>.mp4`
3. 等 3 秒，进程创建拉起最大化窗口并预载两条命令：
   `powershell -NoExit -WindowStyle Maximized -Command "Set-Location <repo>; <wingman-agent.exe> script examples\lua_scripts\hello.lua; <wingman-agent.exe> script examples\lua_scripts\color_detect.lua"`
4. 等 25 秒（两条命令输出滚完），向自己起的 ffmpeg 进程 stdin 写 `q` 停录。
5. 验收：`ffprobe` 时长 ≥25s、分辨率 1920×1080；抽一帧确认画面是终端窗口
   而非锁屏或空桌面。

**不要**尝试点击窗口聚焦或模拟键盘输入——那属于 GUI 注入，需在机器上获得
授权；上面的预载命令方式正是为免此授权。`wingman-agent.exe` 缺失是另一个
独立卡点，用公开 nightly 资产下载即可绕过，无需在机器上构建。

## EP02 Runtime GUI 总览

前置：EP01 环境；GUI 已随发行包带 Tauri 前端。

| # | 画面 | 操作 | 旁白（逐字） |
|---|------|------|--------------|
| 1 | GUI 启动 | 启动 GUI，等 IPC 连接徽标变绿 | 「GUI 通过本地 IPC 控制 Runtime，不走网络。」 |
| 2 | Dashboard 页 | 指出连接状态与运行统计 | 「仪表盘展示 Runtime 与中控的连接状态。」 |
| 3 | Scripts 页 | 启动、停止一个脚本 | 「脚本状态有三种：运行中、已停止、错误。」 |
| 4 | Scripts 页 | 指出脚本编辑入口 | 「编辑建议用 VS Code 加 EmmyLua，GUI 只做启动和监控。」 |
| 5 | Triggers 页 | 打开创建面板，不保存 | 「触发器在第 5 集详细演示。」 |
| 6 | Macros 页 | 录制 3 秒鼠标操作并回放 | 「宏回放是按录制序列回放，可以调速度和重复次数。」 |
| 7 | Screen 页 | 全屏截图 | 「屏幕工具支持全屏截图、区域选择和颜色拾取。」 |

## EP03 连接远程中控

前置：中控已部署（Server + Dashboard），账号已建。

| # | 画面 | 操作 | 旁白（逐字） |
|---|------|------|--------------|
| 1 | agent.toml | 编辑 server 地址与 token | 「agent.toml 指向中控地址；注册 token 在中控后台生成。」 |
| 2 | Runtime 重启 | 启动后看到注册成功日志 | 「注册成功后，中控的 Agent 列表里会多出这台机器。」 |
| 3 | Dashboard | 刷新 Agent 列表，查看标签 | 「中控下发操作，都经这条出站连接回传结果。」 |
| 4 | 网络面板 | 断开网络，看状态变化，再恢复 | 「连接断了 Runtime 会自动重连，本地脚本不受影响。」 |

## EP04 用示例脚本做像素检测

前置：EP02 环境；示例脚本随仓库自带。

| # | 画面 | 操作 | 旁白（逐字） |
|---|------|------|--------------|
| 1 | 文件管理器 | 打开 examples/lua_scripts/color_detect.lua | 「示例脚本都在 lua_scripts 目录，这一集用颜色检测。」 |
| 2 | 编辑器 | 指出 targetColor 与 region 参数 | 「脚本在一个区域内找红色像素，区域可以用屏幕坐标指定。」 |
| 3 | GUI Scripts | 把脚本放进脚本目录，在 Scripts 页选中启动 | 「脚本放进脚本目录后，Scripts 页就能直接启动。」 |
| 4 | 输出窗口 | 展示 findColor 的结果与匹配数量 | 「示例图省事用了全屏；实际范围越小越快，别用全屏。」 |
| 5 | Scripts 页 | 停止脚本，指出状态回灰色 | 「停止后状态回到已停止，日志保留。」 |

## EP05 触发器与宏

前置：EP04 环境。

| # | 画面 | 操作 | 旁白（逐字） |
|---|------|------|--------------|
| 1 | Triggers 页 | 新建触发器，条件选 time_elapsed | 「定时触发器按间隔触发，用来周期性执行操作。」 |
| 2 | Triggers 页 | 动作选 run_script，指定脚本 | 「动作可以是运行脚本、点击、按键、延时或打日志。」 |
| 3 | Triggers 页 | 填冷却时间并保存 | 「冷却时间避免条件连续命中时重复执行。」 |
| 4 | Triggers 页 | 新建 color_found 触发器 | 「颜色触发器检测到指定区域内的目标颜色才生效。」 |
| 5 | Triggers 页 | 禁用一个触发器，再看触发记录 | 「禁用不删配置，触发历史在记录里可查。」 |
| 6 | Macros 页 | 录制一次鼠标键盘流程并保存 | 「宏适合一次性流程，脚本适合要长期维护的自动化。」 |
| 7 | Macros 页 | 回放，速度 100% 改 300% 再回放 | 「回放速度在 10% 到 500% 之间调，重复次数 1 到 100。」 |

## EP06 Dashboard 与远程接管

前置：EP03 的中控 + 两台机器（A 操作端、B 受控端）。

| # | 画面 | 操作 | 旁白（逐字） |
|---|------|------|--------------|
| 1 | Dashboard | 从中控批量下发脚本到 B | 「中控能按标签批量执行脚本。」 |
| 2 | 远程桌面 | 申请接管 B 的票据 | 「接管需要 desktop:control 权限，只监看只需 view。」 |
| 3 | 远程桌面 | 在接管窗口里操作 B | 「像素面经 guacd 翻译，浏览器直接看，不需要装客户端。」 |
| 4 | 文件传输 | 在接管会话里上传一个文件 | 「文件传输走虚拟盘映射，操作有审计。」 |
| 5 | 审计报表 | 打开会话记录页 | 「每次接管都有会话记录，可以回查。」 |

## 录制与发布检查单

- [ ] 机器侧预采前跑门控探针（`Get-Process LockApp` 为空才录）——桌面锁定时只会录到锁屏画面。
- [ ] 每集录完先自查：旁白与画面是否同步、有无口误、敏感信息（token、内网地址）是否打码。
- [ ] SRT 字幕与视频同名，按 srt-template.md 规范切分与校验；术语拼写与 docs/guide 一致。
- [ ] 上传后把链接回填剧集总览表「文件名」列旁。
- [ ] 全部录制发布完成后，勾选 ROADMAP M8.2「视频教程」。
