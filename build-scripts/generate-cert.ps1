# Wingman 自签名证书生成脚本
# 用于开发和小规模分发

$CertName = "Wingman"
$OrgName = "Wingman Open Source"
$ValidYears = 5
$Password = "Wingman2024"

Write-Host "=== 生成 Wingman 自签名证书 ===" -ForegroundColor Green

# 检查管理员权限
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "需要管理员权限运行此脚本！" -ForegroundColor Red
    Write-Host "请右键点击 PowerShell，选择'以管理员身份运行'" -ForegroundColor Yellow
    exit 1
}

# 创建证书
Write-Host "创建自签名证书..." -ForegroundColor Yellow

try {
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject "CN=$CertName, O=$OrgName, C=US" `
        -KeyUsage DigitalSignature `
        -KeyUsageProperty All `
        -KeyLength 2048 `
        -CertStoreLocation "Cert:\LocalMachine\My" `
        -NotAfter (Get-Date).AddYears($ValidYears) `
        -FriendlyName $CertName

    $thumbprint = $cert.Thumbprint
    Write-Host "证书已创建，指纹: $thumbprint" -ForegroundColor Green

    # 导出为 PFX 文件
    Write-Host "导出 PFX 文件..." -ForegroundColor Yellow
    $pwd = ConvertTo-SecureString -String $Password -Force -AsPlainText
    $pfxPath = "setup\wingman-cert.pfx"
    Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $pwd -Force
    Write-Host "PFX 文件已保存到: $pfxPath" -ForegroundColor Green

    # 导出为 CER 文件（用于用户信任）
    Write-Host "导出 CER 文件..." -ForegroundColor Yellow
    $cerPath = "setup\wingman-cert.cer"
    Export-Certificate -Cert $cert -FilePath $cerPath -Force
    Write-Host "CER 文件已保存到: $cerPath" -ForegroundColor Green

    # 将证书安装到"受信任的根证书颁发机构"
    Write-Host "安装到受信任的根证书..." -ForegroundColor Yellow
    $rootStore = New-Object System.Security.Cryptography.X509Certificates.X509Store `
        "Root", "LocalMachine"
    $rootStore.Open("ReadWrite")
    $rootStore.Add($cert)
    $rootStore.Close()
    Write-Host "证书已安装到本地计算机" -ForegroundColor Green

    Write-Host ""
    Write-Host "=== 完成 ===" -ForegroundColor Green
    Write-Host "证书有效期限: $ValidYears 年"
    Write-Host ""
    Write-Host "使用说明:" -ForegroundColor Yellow
    Write-Host "1. 签名 EXE: .\setup\sign.bat"
    Write-Host "2. 用户首次运行需导入 wingman-cert.cer 到'受信任的根'"
    Write-Host ""
    Write-Host "注意: 自签名证书会导致 SmartScreen 警告，用户需点击'更多信息'→'仍要运行'"

} catch {
    Write-Host "错误: $_" -ForegroundColor Red
    exit 1
}
