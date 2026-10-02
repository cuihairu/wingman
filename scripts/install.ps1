<#
.SYNOPSIS
    Wingman Agent one-click installer (Windows PowerShell).

.DESCRIPTION
    Installs the wingman-agent binary for this CPU architecture from GitHub
    Releases. Anonymous access — no GitHub login required. Re-running upgrades
    in place (idempotent).

    One-liner:
        irm https://raw.githubusercontent.com/cuihairu/wingman/main/scripts/install.ps1 | iex

    With options:
        & ([scriptblock]::Create((irm https://raw.githubusercontent.com/cuihairu/wingman/main/scripts/install.ps1))) -Version nightly -Service

.PARAMETER Prefix
    Install directory (default: %LOCALAPPDATA%\Programs\Wingman\bin)

.PARAMETER Version
    Pin a release tag (default: latest release shipping an agent build;
    stable preferred over nightly prereleases)

.PARAMETER Arch
    Override architecture detection (x64 / arm64, for testing)

.PARAMETER Token
    GitHub API token — only raises the API rate limit; the binary download
    itself uses anonymous release asset URLs

.PARAMETER NoPath
    Do not add the install directory to the user PATH

.PARAMETER Service
    Register the agent as a Windows service (requires an elevated shell;
    note: services run in Session 0 without access to the interactive
    desktop — suited to headless servers, not desktop automation)
#>
[CmdletBinding()]
param(
    [string]$Prefix = '',
    [string]$Version = '',
    [string]$Arch = '',
    [string]$Token = $(if ($env:WINGMAN_INSTALL_TOKEN) { $env:WINGMAN_INSTALL_TOKEN } else { $env:GITHUB_TOKEN }),
    [switch]$NoPath,
    [switch]$Service
)

$ErrorActionPreference = 'Stop'
$Repo = if ($env:WINGMAN_INSTALL_REPO) { $env:WINGMAN_INSTALL_REPO } else { 'cuihairu/wingman' }
$ApiBase = "https://api.github.com/repos/$Repo"

function Write-Info([string]$Message) { Write-Host "[wingman-install] $Message" }
function Stop-Install([string]$Message) { throw "[wingman-install] ERROR: $Message" }

# ---------- OS / architecture detection ----------
# Windows PowerShell 5.1 (Desktop edition) has no $IsWindows — it is Windows by definition
$onWindows = if ($PSVersionTable.PSEdition -eq 'Core') { [bool]$IsWindows } else { $true }
if (-not $onWindows) {
    Stop-Install "This script targets Windows. On Linux/macOS use:
  curl -fsSL https://raw.githubusercontent.com/$Repo/main/scripts/install.sh | bash"
}
# Prefix 默认值惰性求值：param 绑定阶段 $env:LOCALAPPDATA 在非 Windows 会话
# 为 null，Join-Path 会先于 OS 防御分支抛错
if (-not $Prefix) {
    if (-not $env:LOCALAPPDATA) {
        Stop-Install "LOCALAPPDATA is not set; pass -Prefix <dir> explicitly"
    }
    $Prefix = Join-Path $env:LOCALAPPDATA 'Programs\Wingman\bin'
}
# Windows PowerShell 5.1 defaults can negotiate only TLS 1.0; GitHub requires 1.2+
if ($PSVersionTable.PSEdition -eq 'Desktop') {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
}

if (-not $Arch) {
    switch ($env:PROCESSOR_ARCHITECTURE) {
        'AMD64' { $Arch = 'x64' }
        'ARM64' { $Arch = 'arm64' }
        'x86' {
            Stop-Install "Unsupported CPU architecture: '$env:PROCESSOR_ARCHITECTURE'.
The agent build matrix currently covers x64 and arm64 only.
If you believe this architecture should be supported, file an issue at
https://github.com/$Repo/issues."
        }
        default {
            Stop-Install "Unsupported CPU architecture: '$($env:PROCESSOR_ARCHITECTURE)'.
Supported: AMD64 (x64), ARM64 (arm64).
If you believe this architecture should be supported, file an issue at
https://github.com/$Repo/issues."
        }
    }
}
if ($Arch -notin @('x64', 'arm64')) {
    Stop-Install "Unsupported architecture override: '$Arch' (supported: x64, arm64)"
}

$AssetPattern = "^wingman-agent-.+-windows-$Arch\.zip$"
Write-Info "Detected: windows-$Arch (PROCESSOR_ARCHITECTURE=$env:PROCESSOR_ARCHITECTURE)"

# ---------- Resolve the download URL (anonymous GitHub API) ----------
function Get-ApiHeaders {
    $headers = @{ 'User-Agent' = 'wingman-install.ps1'; 'Accept' = 'application/vnd.github+json' }
    if ($Token) { $headers['Authorization'] = "Bearer $Token" }
    $headers
}

# 查询走 Invoke-WebRequest + ConvertFrom-Json：pwsh 7.4 的 Invoke-RestMethod
# 对 JSON 数组响应返回嵌套数组（@(irm) 只收 1 个元素），5.1 又是平铺展开，
# IWR+ConvertFrom-Json 在两版行为一致
try {
    $headers = Get-ApiHeaders
    if ($Version) {
        Write-Info "Looking up release '$Version' ..."
        $resp = Invoke-WebRequest -Uri "$ApiBase/releases/tags/$Version" -Headers $headers -UseBasicParsing
        $release = $resp.Content | ConvertFrom-Json
        $asset = @($release.assets) | Where-Object { $_.name -match $AssetPattern } | Select-Object -First 1
        if (-not $asset) {
            Stop-Install "Release '$Version' has no asset matching $AssetPattern.
Check the assets at https://github.com/$Repo/releases/tag/$Version"
        }
    }
    else {
        Write-Info "Looking up the latest release with an agent build for windows-$Arch ..."
        # Release list is newest-first: pick the first release that ships an
        # agent asset for this platform (stable releases win; before stable
        # ships agent builds this naturally falls through to nightly)
        $resp = Invoke-WebRequest -Uri "$ApiBase/releases?per_page=30" -Headers $headers -UseBasicParsing
        $releases = @($resp.Content | ConvertFrom-Json)
        $asset = $null
        foreach ($rel in $releases) {
            if ($rel.draft) { continue }
            $hit = @($rel.assets) | Where-Object { $_.name -match $AssetPattern } | Select-Object -First 1
            if ($hit) { $asset = $hit; break }
        }
        if (-not $asset) {
            Stop-Install "No release in $Repo provides wingman-agent for windows-$Arch.
The build matrix covers: linux/macos/windows x x64/arm64. If this platform
should be supported, file an issue at https://github.com/$Repo/issues."
        }
    }
}
catch {
    $status = $null
    if ($_.Exception.Response) { $status = [int]$_.Exception.Response.StatusCode }
    if ($status -eq 403) {
        Stop-Install "GitHub API returned 403 (rate limited). Anonymous quota is 60 req/hour;
retry later or pass -Token <GITHUB_TOKEN>."
    }
    if ($status -eq 404) {
        Stop-Install "Release '$Version' not found in $Repo (HTTP 404)."
    }
    Stop-Install "Failed to query the GitHub API: $($_.Exception.Message)"
}

# browser_download_url: https://github.com/<owner>/<repo>/releases/download/<tag>/<asset>
$urlParts = $asset.browser_download_url -split '/'
$Tag = $urlParts[[array]::IndexOf($urlParts, 'download') + 1]
$DownloadUrl = $asset.browser_download_url
Write-Info "Selected: $Tag -> $($asset.name)"

# ---------- Download (anonymous release asset URL) ----------
$TempDir = Join-Path ([System.IO.Path]::GetTempPath()) ("wingman-install-" + [System.IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Path $TempDir | Out-Null
try {
    Write-Info "Downloading $DownloadUrl"
    $zipPath = Join-Path $TempDir 'asset.zip'
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $zipPath -UseBasicParsing

    # ---------- Extract ----------
    $extractDir = Join-Path $TempDir 'extract'
    Expand-Archive -Path $zipPath -DestinationPath $extractDir -Force
    $binSrc = Get-ChildItem -Path $extractDir -Recurse -Filter 'wingman-agent.exe' -File | Select-Object -First 1
    if (-not $binSrc) {
        Stop-Install "No 'wingman-agent.exe' found inside $($asset.name) (unexpected package layout)"
    }

    # ---------- Install (re-run = upgrade) ----------
    New-Item -ItemType Directory -Force -Path $Prefix | Out-Null
    $dest = Join-Path $Prefix 'wingman-agent.exe'

    $oldVersion = ''
    if (Test-Path $dest) {
        try { $oldVersion = (& $dest --version 2>$null | Select-Object -First 1) } catch { $oldVersion = '' }
    }

    Copy-Item -Path $binSrc.FullName -Destination $dest -Force

    # ---------- PATH (idempotent, user-level) ----------
    if (-not $NoPath) {
        $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
        if (-not $userPath) { $userPath = '' }
        $entries = @($userPath -split ';' | Where-Object { $_ })
        if ($Prefix -notin $entries) {
            $newPath = (@($userPath.TrimEnd(';')) + $Prefix) -join ';'
            if ($newPath.Length -lt 2047) {
                [Environment]::SetEnvironmentVariable('Path', $newPath, 'User')
                Write-Info "Added $Prefix to the user PATH (effective in new shells)"
            }
            else {
                Write-Info "NOTE: user PATH is full ($($newPath.Length) chars) — add manually: $Prefix"
            }
        }
    }

    # ---------- Verify ----------
    $newVersion = ''
    try { $newVersion = (& $dest --version 2>$null | Select-Object -First 1) } catch { $newVersion = '' }
    if (-not $newVersion) {
        Stop-Install "Installed binary at $dest failed to run (--version produced no output).
If this is an x86 machine with an arm64 build (or vice versa), re-run with -Arch."
    }

    if ($oldVersion -and $oldVersion -ne $newVersion) {
        Write-Info "Upgraded: $oldVersion -> $newVersion"
    }
    else {
        Write-Info "Installed: $newVersion"
    }
    Write-Info "Location: $dest"

    # ---------- Optional Windows service ----------
    if ($Service) {
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        $principal = [Security.Principal.WindowsPrincipal]$identity
        if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
            Stop-Install "Registering a Windows service requires an elevated shell.
Re-run PowerShell as Administrator with: ...install.ps1 -Service"
        }

        Write-Info "NOTE: services run in Session 0 without access to the interactive
       desktop — screen capture and input injection will not work under the
       service account. Use -Service only for headless/server deployments;
       for desktop automation start the agent manually or via Task Scheduler."

        $existing = Get-Service -Name 'wingman-agent' -ErrorAction SilentlyContinue
        if ($existing) {
            if ($existing.Status -eq 'Running') { Stop-Service -Name 'wingman-agent' -Force -ErrorAction SilentlyContinue }
            & sc.exe delete 'wingman-agent' | Out-Null
            Start-Sleep -Seconds 1
        }

        $binPath = '"{0}" start' -f $dest
        New-Service -Name 'wingman-agent' `
            -DisplayName 'Wingman Agent' `
            -Description 'Wingman game automation agent (outbound connector to the Go server)' `
            -BinaryPathName $binPath `
            -StartupType Automatic | Out-Null
        Start-Service -Name 'wingman-agent'
        Write-Info "Service registered: wingman-agent (started)"
        Write-Info "  status: Get-Service wingman-agent"
        Write-Info "  stop:   Stop-Service wingman-agent"
    }

    Write-Info 'Done.'
}
finally {
    Remove-Item -Recurse -Force -Path $TempDir -ErrorAction SilentlyContinue
}
