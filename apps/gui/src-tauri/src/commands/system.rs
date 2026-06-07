use crate::models::{RuntimeInfo, SystemStatus, VersionInfo};
use crate::state::AppState;
use serde_json::json;

#[tauri::command]
pub async fn get_system_status(
    state: tauri::State<'_, AppState>,
) -> Result<SystemStatus, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client.send("system.getStatus", json!({})).await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            let uptime = response["data"]["result"]["uptime"].as_u64().unwrap_or(0);
            let paused = response["data"]["result"]["paused"].as_bool().unwrap_or(false);
            *state.paused.lock().await = paused;
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
                paused,
            });
        }
    }
    Err("Failed to get system status".to_string())
}

#[tauri::command]
pub async fn get_version(
    state: tauri::State<'_, AppState>,
) -> Result<VersionInfo, String> {
    let mut client = state.ipc_client.lock().await;
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
    let paused = {
        let mut client = state.ipc_client.lock().await;
        if client.connected {
            let response = client.send("system.isPaused", json!({})).await?;
            response["data"]["result"]["paused"].as_bool().unwrap_or(false)
        } else {
            *state.paused.lock().await
        }
    };
    let start_time = *state.start_time.lock().await;
    let uptime = if let Some(st) = start_time {
        st.elapsed()
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
    let mut client = state.ipc_client.lock().await;
    let response = client.send("system.togglePause", json!({})).await?;

    let paused = response["data"]["result"]["paused"].as_bool().unwrap_or(false);
    *state.paused.lock().await = paused;
    Ok(paused)
}

#[tauri::command]
pub async fn pause_all(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let mut client = state.ipc_client.lock().await;
    client.send("system.pauseAll", json!({})).await?;
    *state.paused.lock().await = true;
    Ok(())
}

#[tauri::command]
pub async fn resume_all(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let mut client = state.ipc_client.lock().await;
    client.send("system.resumeAll", json!({})).await?;
    *state.paused.lock().await = false;
    Ok(())
}

#[tauri::command]
pub async fn stop_all(state: tauri::State<'_, AppState>) -> Result<u64, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client.send("system.stopAll", json!({})).await?;
    *state.paused.lock().await = false;
    Ok(response["data"]["result"]["stoppedScripts"].as_u64().unwrap_or(0))
}

#[tauri::command]
pub async fn is_paused(state: tauri::State<'_, AppState>) -> Result<bool, String> {
    let mut client = state.ipc_client.lock().await;
    if !client.connected {
        return Ok(*state.paused.lock().await);
    }

    let response = client.send("system.isPaused", json!({})).await?;
    let paused = response["data"]["result"]["paused"].as_bool().unwrap_or(false);
    *state.paused.lock().await = paused;
    Ok(paused)
}
