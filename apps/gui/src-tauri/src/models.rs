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
