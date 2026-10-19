use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScriptInfo {
    pub id: String,
    pub name: String,
    pub path: String,
    pub size: u64,
    pub is_running: bool,
    /// loaded / running / paused / stopped / error / unknown
    pub state: String,
    pub error: String,
    /// 最近一次加载时间（epoch 毫秒）
    pub loaded_at: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScriptStatus {
    pub script_id: String,
    pub status: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemStatus {
    pub server: String,
    pub version: String,
    pub uptime: u64,
    pub running_scripts: u64,
    pub paused: bool,
    /// runtime -> Go server 远程链路（旧 runtime 缺省时为 false）
    #[serde(default)]
    pub remote_connected: bool,
    /// 远程链路状态名（connected/connecting/reconnecting/disconnected/error/disabled）
    #[serde(default)]
    pub remote_state: String,
    /// runtime 视角：本地 IPC 客户端（本 GUI）是否在线
    #[serde(default)]
    pub ipc_client_connected: bool,
    /// runtime 运行模式（RunMode 数值）
    #[serde(default)]
    pub mode: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VersionInfo {
    pub server: String,
    pub version: String,
    pub build_date: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RuntimeInfo {
    pub paused: bool,
    pub uptime: u64,
    pub memory_usage: u64,
    pub cpu_usage: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScreenRegion {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScreenshotInfo {
    pub image: String,
    pub width: u32,
    pub height: u32,
    pub timestamp: u64,
    pub region: ScreenRegion,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MonitorInfo {
    pub id: i32,
    pub name: String,
    pub is_primary: bool,
    pub bounds: ScreenRegion,
}

/// 服务端下发的更新清单（release 元数据）。
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct UpdateManifest {
    pub version: String,
    pub url: String,
    pub sha256: String,
    pub size: u64,
    pub notes: String,
    pub mandatory: bool,
    /// 上传时的原始文件名（含扩展名，用于本地落盘命名）
    pub file_name: String,
}

/// 更新轮询器状态快照（update.status / check / download 的载荷）。
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default, rename_all = "camelCase")]
pub struct UpdateStatus {
    /// 更新功能是否启用（[update] enabled 且 base_url 非空）
    pub enabled: bool,
    pub available: bool,
    pub downloading: bool,
    pub downloaded: bool,
    pub local_version: String,
    pub candidate: UpdateManifest,
    /// 校验通过后的制品落盘路径（downloaded=true 时非空）
    pub download_path: String,
    /// 最近一次检查时间（epoch 毫秒；0 = 尚未检查）
    pub last_check_ms: u64,
    pub last_error: String,
}

/// update.check / update.download 的异步触发回执。
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct UpdateActionResult {
    /// 请求是否已被受理（未启用时为 false）
    pub started: bool,
    pub status: UpdateStatus,
}
