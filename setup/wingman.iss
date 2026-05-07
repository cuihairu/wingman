; Wingman 安装程序脚本
; 用于生成 Windows 安装包

#define AppName "Wingman"
#define AppVersion "0.1.0"
#define AppPublisher "Wingman"
#define AppExeName "wingman.exe"
#define AppPublisherUrl "https://github.com/cuihairu/wingman"
#define AppSupportUrl "https://github.com/cuihairu/wingman/issues"

[Setup]
; 应用程序基本信息
AppId={{A5B6C7D8-9E0F-4A3B-8C7D-1E2F3A4B5C6D}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppPublisherUrl}
AppSupportURL={#AppSupportUrl}
AppUpdatesURL={#AppPublisherUrl}
AppCopyright=Copyright (C) 2024 {#AppPublisher}

; 默认安装目录
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}

; 输出文件配置
OutputDir=installer
OutputBaseFilename=wingman-setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes

; 安装程序配置
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
MinVersion=10.0.14393  ; Windows 10 1607+
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#AppExeName}
WizardStyle=modern
WizardImageFile=installer\wizard-image.bmp
WizardSmallImageFile=installer\wizard-small.bmp
SetupIconFile=assets\wingman.ico

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Languages\English.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 6.1
Name: "autostart"; Description: "开机自动启动"; GroupDescription: "其他设置:"; Flags: unchecked

[Files]
; 主程序
Source: "build\Release\wingman.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\Release\*.dll"; DestDir: "{app}"; Flags: ignoreversion

; Dashboard 前端
Source: "build\dist\*"; DestDir: "{app}\dist"; Flags: ignoreversion recursesubdirs createallsubdirs

; 脚本示例
Source: "scripts\examples\*.lua"; DestDir: "{app}\scripts\examples"; Flags: ignoreversion recursesubdirs createallsubdirs

; 配置文件
Source: "config\*.json"; DestDir: "{app}\config"; Flags: ignoreversion recursesubdirs createallsubdirs

; 文档
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion

; 可选：依赖库
Source: "C:\Users\cui\vcpkg\installed\x64-windows\bin\*.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: vcrt

[Dirs]
Name: "{app}\logs"
Name: "{app}\scripts"
Name: "{app}\config"
Name: "{app}\cache"

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
; 安装后运行
Filename: "{app}\{#AppExeName}"; Description: "启动 {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 卸载时删除用户数据目录
Type: filesandordirs; Name: "{app}\logs"
Type: filesandordirs; Name: "{app}\cache"

[Registry]
; 开机自启动
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#AppName}"; ValueData: """{app}\{#AppExeName}"""; Tasks: autostart; Flags: uninsdeletevalue

[Code]
// 安装前检查
function InitializeSetup: Boolean;
var
  Version: TWindowsVersion;
begin
  // 检查 Windows 版本
  GetWindowsVersionEx(Version);
  if Version.Major < 10 then
  begin
    MsgBox('Wingman 需要 Windows 10 或更高版本。', mbError, MB_OK);
    Result := False;
    Exit;
  end;

  Result := True;
end;

// 安装后提示
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    // 创建配置目录
    if not DirExists(ExpandConstant('{userappdata}\{#AppName}')) then
      CreateDir(ExpandConstant('{userappdata}\{#AppName}'));

    // 复制默认配置
    if FileExists(ExpandConstant('{app}\config\default.json')) then
      FileCopy(ExpandConstant('{app}\config\default.json'),
               ExpandConstant('{userappdata}\{#AppName}\config.json'),
               False);
  end;
end;
