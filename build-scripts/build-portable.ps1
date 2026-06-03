# Wingman 便携版打包脚本

$Version = "0.1.0"
$PortableDir = "portable\wingman"
$OutputZip = "wingman-portable-$Version.zip"
$RuntimeExe = "build-msvc-ninja-vcpkg\apps\runtime\wingman-runtime.exe"

Write-Host "=== Wingman 便携版打包 ===" -ForegroundColor Green

# 清理旧文件
if (Test-Path $PortableDir) {
    Remove-Item -Recurse -Force $PortableDir
}
if (Test-Path $OutputZip) {
    Remove-Item -Force $OutputZip
}

# 创建目录结构
New-Item -ItemType Directory -Force -Path $PortableDir | Out-Null
New-Item -ItemType Directory -Force -Path "$PortableDir\dist" | Out-Null
New-Item -ItemType Directory -Force -Path "$PortableDir\scripts\examples" | Out-Null
New-Item -ItemType Directory -Force -Path "$PortableDir\config" | Out-Null
New-Item -ItemType Directory -Force -Path "$PortableDir\logs" | Out-Null
New-Item -ItemType Directory -Force -Path "$PortableDir\cache" | Out-Null

# 复制主程序和 DLL
Write-Host "复制主程序..." -ForegroundColor Yellow
if (Test-Path $RuntimeExe) {
    Copy-Item -Path $RuntimeExe -Destination "$PortableDir\" -Force
    Copy-Item -Path "build-msvc-ninja-vcpkg\apps\runtime\*.dll" -Destination "$PortableDir\" -Force -ErrorAction SilentlyContinue
} else {
    Write-Host "错误: 找不到编译后的主程序，请先编译！" -ForegroundColor Red
    exit 1
}

# 复制 Dashboard
Write-Host "复制 Dashboard..." -ForegroundColor Yellow
if (Test-Path "orchestrator\dashboard\dist") {
    Copy-Item -Path "orchestrator\dashboard\dist\*" -Destination "$PortableDir\dist\" -Recurse -Force
}

# 复制示例脚本
Write-Host "复制示例脚本..." -ForegroundColor Yellow
if (Test-Path "scripts\examples") {
    Copy-Item -Path "scripts\examples\*.lua" -Destination "$PortableDir\scripts\examples\" -Force
}

# 复制配置文件
Write-Host "复制配置文件..." -ForegroundColor Yellow
if (Test-Path "apps\runtime\config\agent.toml") {
    Copy-Item -Path "apps\runtime\config\agent.toml" -Destination "$PortableDir\config\" -Force
}

# 复制文档
Write-Host "复制文档..." -ForegroundColor Yellow
if (Test-Path "README.md") { Copy-Item -Path "README.md" -Destination "$PortableDir\" -Force }
if (Test-Path "LICENSE") { Copy-Item -Path "LICENSE" -Destination "$PortableDir\" -Force }

# 创建启动脚本
@"
@echo off
REM Wingman 启动脚本

echo === Wingman 便携版 ===
echo.

REM 检查配置文件
if not exist "config\agent.toml" (
    echo 首次运行，创建默认配置...
    copy ".\config\agent.toml" "config\agent.toml" >nul
)

REM 启动主程序
start "" wingman-runtime.exe

echo Wingman 已启动！
echo.
pause
"@ | Out-File -FilePath "$PortableDir\启动 Wingman.bat" -Encoding ASCII

# 创建卸载脚本
@"
@echo off
REM Wingman 卸载脚本

echo === Wingman 卸载 ===
echo.
echo 此操作将删除便携版目录，是否继续？ (Y/N)
set /p confirm=

if /i "%confirm%"=="Y" (
    echo 正在删除...
    cd ..
    rmdir /s /q wingman
    echo 卸载完成！
) else (
    echo 已取消
)
pause
"@ | Out-File -FilePath "$PortableDir\卸载.bat" -Encoding ASCII

# 打包
Write-Host "创建 ZIP 压缩包..." -ForegroundColor Yellow
Compress-Archive -Path $PortableDir -DestinationPath $OutputZip -Force

# 清理临时文件
Remove-Item -Recurse -Force $PortableDir

Write-Host ""
Write-Host "=== 打包完成 ===" -ForegroundColor Green
Write-Host "输出文件: $OutputZip"
Write-Host "文件大小: $((Get-Item $OutputZip).Length / 1MB) MB"
