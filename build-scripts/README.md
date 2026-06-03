# Wingman 构建与安装脚本指南

## Windows 主构建入口

项目当前推荐的 Windows 构建链为 **MSVC + Ninja + vcpkg**：

```cmd
build-scripts\build-runtime-msvc-ninja.bat
```

如需仅配置不编译：

```cmd
build-scripts\configure-msvc-ninja.bat
```

其余 `configure.bat`、`build_configure.*` 为兼容保留脚本，`configure_minimal.bat` 为最小化功能构建脚本。

## 前置条件

1. 安装 [Inno Setup 6.x](http://www.jrsoftware.org/isdl.php)
2. 确保主程序已编译 (`build-msvc-ninja-vcpkg/apps/runtime/wingman-runtime.exe`)
3. 确保 Dashboard 已构建

## 构建步骤

### 1. 编译主程序

```bash
# 在项目根目录
build-scripts\build-runtime-msvc-ninja.bat
```

### 2. 构建 Dashboard

```bash
cd orchestrator/dashboard
npm install
npm run build
```

### 3. 生成安装包

双击 `wingman.iss` 文件，或在命令行运行：

```bash
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" wingman.iss
```

生成的安装包位于 `installer/wingman-setup-0.1.0.exe`

## 自定义

### 修改安装程序图标

将 `wingman.ico` 放在 `assets/` 目录。

### 修改向导图片

- `wizard-image.bmp` (164×314) - 左侧大图
- `wizard-small.bmp` (55×55) - 右上角小图

### 添加更多文件

在 `[Files]` 部分添加：

```ini
Source: "path\to\file"; DestDir: "{app}"; Flags: ignoreversion
```

## 便携版 ZIP

构建便携版（无需安装）：

```bash
build-scripts\build-portable.ps1
```
