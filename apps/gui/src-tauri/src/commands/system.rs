use crate::models::{RuntimeInfo, SystemStatus, VersionInfo};
use crate::state::AppState;
use serde_json::json;
use std::time::SystemTime;

#[tauri::command]
pub async fn get_system_status(
    state: tauri::State<'_, AppState>,
) -> Result<SystemStatus, String> {
    let mut client = state.ws_client.lock().await;
    let response = client.send("system.getStatus", json!({})).await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            let uptime = response["data"]["result"]["uptime"].as_u64().unwrap_or(0);
            return Ok(SystemStatus {
                server: response["data"]["result"]["server"]
                    .as_str()
                    .unwrap_or("")
                    .to_string(),
                version: response["data"]["result"]["version"]
                    .as_str()
                    .unwrap_or("")
                    .to_string(),
                uptime,
                running_scripts: response["data"]["result"]["runningScripts"]
                    .as_u64()
                    .unwrap_or(0),
                paused: *state.paused.lock().await,
            });
        }
    }
    Err("Failed to get system status".to_string())
}

#[tauri::command]
pub async fn get_version(
    state: tauri::State<'_, AppState>,
) -> Result<VersionInfo, String> {
    let mut client = state.ws_client.lock().await;
    let response = client.send("system.getVersion", json!({})).await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            return Ok(VersionInfo {
                server: response["data"]["result"]["server"]
                    .as_str()
                    .unwrap_or("")
                    .to_string(),
                version: response["data"]["result"]["version"]
                    .as_str()
                    .unwrap_or("")
                    .to_string(),
                build_date: response["data"]["result"]["buildDate"]
                    .as_str()
                    .unwrap_or("")
                    .to_string(),
            });
        }
    }
    Err("Failed to get version".to_string())
}

#[tauri::command]
pub async fn get_runtime_info(state: tauri::State<'_, AppState>) -> Result<RuntimeInfo, String> {
    let paused = *state.paused.lock().await;
    let start_time = *state.start_time.lock().await;
    let uptime = if let Some(st) = start_time {
        st.duration_since(SystemTime::UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs()
    } else {
        0
    };

    Ok(RuntimeInfo {
        paused,
        uptime,
        memory_usage: 0,
        cpu_usage: 0.0,
    })
}

#[tauri::command]
pub async fn toggle_pause(state: tauri::State<'_, AppState>) -> Result<bool, String> {
    let mut paused = state.paused.lock().await;
    *paused = !*paused;
    Ok(*paused)
}

#[tauri::command]
pub async fn pause_all(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let mut paused = state.paused.lock().await;
    *paused = true;
    Ok(())
}

#[tauri::command]
pub async fn resume_all(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let mut paused = state.paused.lock().await;
    *paused = false;
    Ok(())
}

#[tauri::command]
pub async fn is_paused(state: tauri::State<'_, AppState>) -> bool {
    *state.paused.lock().await
}
