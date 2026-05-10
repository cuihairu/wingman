# Wingman 安装程序构建指南

## 前置条件

1. 安装 [Inno Setup 6.x](http://www.jrsoftware.org/isdl.php)
2. 确保主程序已编译 (`build/Release/wingman.exe`)
3. 确保 Dashboard 已构建 (`build/dist/`)

## 构建步骤

### 1. 编译主程序

```bash
# 在项目根目录
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
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
# 创建 portable 目录
mkdir portable/wingman

# 复制文件
xcopy /E /Y build\Release portable\wingman\
xcopy /E /Y build\dist portable\wingman\dist\
xcopy /E /Y examples\lua_scripts portable\wingman\examples\lua_scripts\

# 打包
powershell Compress-Archive -Path portable\wingman -DestinationPath wingman-portable-{version}.zip
```
