@echo off
REM Wingman EXE 签名脚本
REM 使用自签名证书签名编译后的程序

setlocal

echo === Wingman EXE 签名 ===

REM 设置路径
set CERT_FILE=setup\wingman-cert.pfx
set CERT_PASS=Wingman2024
set EXE_PATH=build-msvc-ninja-vcpkg\apps\runtime\wingman-runtime.exe

REM 检查证书文件
if not exist "%CERT_FILE%" (
    echo 错误: 证书文件不存在！
    echo 请先运行: .\setup\generate-cert.ps1
    pause
    exit /b 1
)

REM 检查 EXE 文件
if not exist "%EXE_PATH%" (
    echo 错误: 找不到编译后的程序！
    echo 请先编译项目: build-scripts\build-runtime-msvc-ninja.bat
    pause
    exit /b 1
)

echo 证书文件: %CERT_FILE%
echo 目标文件: %EXE_PATH%
echo.

REM 查找 SignTool
set SIGNTOOL=
for %%i in (
    "%ProgramFiles(x86)%\Windows Kits\10\bin\10.0.22000.0\x64\signtool.exe"
    "%ProgramFiles(x86)%\Windows Kits\10\bin\10.0.19041.0\x64\signtool.exe"
    "%ProgramFiles(x86)%\Windows Kits\10\bin\x64\signtool.exe"
    "%ProgramFiles(x86)%\Windows Kits\8.1\bin\x64\signtool.exe"
) do (
    if exist "%%~i" (
        set SIGNTOOL=%%~i
        goto :found
    )
)

echo 错误: 找不到 SignTool.exe！
echo 请安装 Windows SDK 或 Visual Studio
pause
exit /b 1

:found
echo 使用 SignTool: %SIGNTOOL%
echo.

REM 执行签名
echo 正在签名...
"%SIGNTOOL%" sign /f "%CERT_FILE%" /p "%CERT_PASS%" /fd sha256 /tr http://timestamp.digicert.com "%EXE_PATH%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo === 签名成功 ===
    echo 验证签名...
    "%SIGNTOOL%" verify /pa "%EXE_PATH%"
) else (
    echo.
    echo === 签名失败 ===
)

echo.
pause
