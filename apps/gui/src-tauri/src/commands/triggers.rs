use crate::models::TriggerInfo;
use crate::state::AppState;
use serde_json::{json, Value};

#[tauri::command]
pub async fn get_triggers(
    state: tauri::State<'_, AppState>,
) -> Result<Vec<TriggerInfo>, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client.send("trigger.list", json!({})).await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            if let Some(triggers) = response["data"]["result"]["triggers"].as_array() {
                return Ok(triggers
                    .iter()
                    .map(|t| TriggerInfo {
                        id: t["id"].as_str().unwrap_or("").to_string(),
                        name: t["name"].as_str().unwrap_or("").to_string(),
                        enabled: t["enabled"].as_bool().unwrap_or(false),
                        trigger_type: t["type"].as_str().unwrap_or("").to_string(),
                        last_triggered: t["lastTriggered"].as_bool().unwrap_or(false),
                    })
                    .collect());
            }
        }
    }
    Err("Failed to get triggers".to_string())
}

#[tauri::command]
pub async fn add_trigger(
    state: tauri::State<'_, AppState>,
    config: Value,
) -> Result<String, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client.send("trigger.add", json!({ "config": config })).await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            return Ok(response["data"]["result"]["id"]
                .as_str()
                .unwrap_or("")
                .to_string());
        }
    }
    Err("Failed to add trigger".to_string())
}

#[tauri::command]
pub async fn remove_trigger(
    state: tauri::State<'_, AppState>,
    id: String,
) -> Result<(), String> {
    let mut client = state.ipc_client.lock().await;
    let response = client
        .send("trigger.remove", json!({ "id": id }))
        .await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            return Ok(());
        }
    }
    Err("Failed to remove trigger".to_string())
}

#[tauri::command]
pub async fn update_trigger(
    state: tauri::State<'_, AppState>,
    id: String,
    config: Value,
) -> Result<(), String> {
    let mut client = state.ipc_client.lock().await;
    let response = client
        .send("trigger.update", json!({ "id": id, "config": config }))
        .await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            return Ok(());
        }
    }
    Err("Failed to update trigger".to_string())
}

#[tauri::command]
pub async fn toggle_trigger(
    state: tauri::State<'_, AppState>,
    id: String,
) -> Result<bool, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client
        .send("trigger.toggle", json!({ "id": id }))
        .await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            return Ok(response["data"]["result"]["enabled"].as_bool().unwrap_or(false));
        }
    }
    Err("Failed to toggle trigger".to_string())
}
