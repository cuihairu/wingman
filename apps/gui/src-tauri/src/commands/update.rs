use crate::ipc::client::IPC_QUICK_TIMEOUT;
use crate::models::{UpdateActionResult, UpdateStatus};
use crate::state::AppState;
use serde::de::DeserializeOwned;
use serde_json::{json, Value};

/// 读当前更新状态（update.status）。轻量状态查询，短超时。
#[tauri::command]
pub async fn get_update_status(
    state: tauri::State<'_, AppState>,
) -> Result<UpdateStatus, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client
        .send_with_timeout("update.status", json!({}), IPC_QUICK_TIMEOUT)
        .await?;
    deserialize(&response, "update.status")
}

/// 触发一次即时更新检查（update.check）。真正执行在 runtime worker 线程，
/// 结果稍后经 update.status_changed 事件回投。
#[tauri::command]
pub async fn check_update(
    state: tauri::State<'_, AppState>,
) -> Result<UpdateActionResult, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client
        .send_with_timeout("update.check", json!({}), IPC_QUICK_TIMEOUT)
        .await?;
    deserialize(&response, "update.check")
}

/// 触发下载当前候选（update.download），下载完成后经事件回投下载路径。
#[tauri::command]
pub async fn download_update(
    state: tauri::State<'_, AppState>,
) -> Result<UpdateActionResult, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client
        .send_with_timeout("update.download", json!({}), IPC_QUICK_TIMEOUT)
        .await?;
    deserialize(&response, "update.download")
}

fn deserialize<T: DeserializeOwned>(
    response: &Value,
    method: &str,
) -> Result<T, String> {
    if !response["data"]["success"].as_bool().unwrap_or(false) {
        return Err(response["data"]["error"]
            .as_str()
            .unwrap_or(method)
            .to_string());
    }
    serde_json::from_value(response["data"]["result"].clone()).map_err(|e| e.to_string())
}
