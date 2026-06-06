use crate::models::{ScriptInfo, ScriptStatus};
use crate::state::AppState;
use serde_json::json;

#[tauri::command]
pub async fn get_scripts(
    state: tauri::State<'_, AppState>,
) -> Result<Vec<ScriptInfo>, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client.send("script.list", json!({})).await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            if let Some(scripts) = response["data"]["result"]["scripts"].as_array() {
                return Ok(scripts
                    .iter()
                    .map(|s| ScriptInfo {
                        id: s["id"].as_str().unwrap_or("").to_string(),
                        name: s["name"].as_str().unwrap_or("").to_string(),
                        path: s["path"].as_str().unwrap_or("").to_string(),
                        size: s["size"].as_u64().unwrap_or(0),
                        is_running: s["isRunning"].as_bool().unwrap_or(false),
                    })
                    .collect());
            }
        }
    }
    Err("Failed to get scripts".to_string())
}

#[tauri::command]
pub async fn start_script(
    state: tauri::State<'_, AppState>,
    id: String,
) -> Result<ScriptStatus, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client
        .send("script.start", json!({ "path": format!("scripts/{}.lua", id) }))
        .await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            return Ok(ScriptStatus {
                script_id: response["data"]["result"]["scriptId"]
                    .as_str()
                    .unwrap_or("")
                    .to_string(),
                status: response["data"]["result"]["status"]
                    .as_str()
                    .unwrap_or("")
                    .to_string(),
            });
        }
    }
    Err("Failed to start script".to_string())
}

#[tauri::command]
pub async fn stop_script(
    state: tauri::State<'_, AppState>,
    script_id: String,
) -> Result<String, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client
        .send("script.stop", json!({ "scriptId": script_id }))
        .await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            return Ok("Script stopped".to_string());
        }
    }
    Err("Failed to stop script".to_string())
}
