use crate::models::{ScriptInfo, ScriptStatus};
use crate::state::AppState;
use serde_json::json;
use std::env;
use std::fs;
use std::path::PathBuf;
use std::sync::Arc;
use tokio::sync::Mutex;

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

#[tauri::command]
pub async fn start_active_profile_scripts(
    state: tauri::State<'_, AppState>,
) -> Result<Vec<ScriptStatus>, String> {
    start_active_profile_scripts_with_client(&state.ipc_client).await
}

#[tauri::command]
pub async fn stop_active_profile_scripts(
    state: tauri::State<'_, AppState>,
) -> Result<u64, String> {
    stop_active_profile_scripts_with_client(&state.ipc_client).await
}

pub async fn start_active_profile_scripts_with_client(
    ipc_client: &Arc<Mutex<crate::ipc::client::IpcClient>>,
) -> Result<Vec<ScriptStatus>, String> {
    let script_paths = active_profile_script_paths()?;
    if script_paths.is_empty() {
        return Err("Active profile has no scripts".to_string());
    }

    let mut client = ipc_client.lock().await;
    let mut results = Vec::new();

    for path in script_paths {
        let response = client.send("script.start", json!({ "path": path })).await?;
        if let Some(success) = response["data"]["success"].as_bool() {
            if success {
                results.push(ScriptStatus {
                    script_id: response["data"]["result"]["scriptId"]
                        .as_str()
                        .unwrap_or("")
                        .to_string(),
                    status: response["data"]["result"]["status"]
                        .as_str()
                        .unwrap_or("")
                        .to_string(),
                });
                continue;
            }
        }

        return Err(response["data"]["error"]
            .as_str()
            .unwrap_or("Failed to start active profile scripts")
            .to_string());
    }

    Ok(results)
}

pub async fn stop_active_profile_scripts_with_client(
    ipc_client: &Arc<Mutex<crate::ipc::client::IpcClient>>,
) -> Result<u64, String> {
    let script_paths = active_profile_script_paths()?;
    if script_paths.is_empty() {
        return Err("Active profile has no scripts".to_string());
    }

    let mut client = ipc_client.lock().await;
    let response = client.send("script.list", json!({})).await?;
    let scripts = response["data"]["result"]["scripts"]
        .as_array()
        .ok_or("Failed to list scripts".to_string())?;

    let mut stopped = 0u64;
    for script in scripts {
        let path = script["path"].as_str().unwrap_or("");
        if !script_paths.iter().any(|candidate| candidate == path) {
            continue;
        }

        let script_id = script["id"].as_str().unwrap_or("");
        if script_id.is_empty() {
            continue;
        }

        client.send("script.stop", json!({ "scriptId": script_id })).await?;
        stopped += 1;
    }

    Ok(stopped)
}

fn active_profile_script_paths() -> Result<Vec<String>, String> {
    let active_id = fs::read_to_string(active_profile_path())
        .map(|value| value.trim().to_string())
        .unwrap_or_default();
    if active_id.is_empty() {
        return Err("No active profile".to_string());
    }

    let content = fs::read_to_string(profile_path(&active_id)).map_err(|e| e.to_string())?;
    let profile: serde_json::Value = serde_json::from_str(&content).map_err(|e| e.to_string())?;
    let scripts = profile["scripts"]
        .as_array()
        .ok_or("Active profile scripts is not an array".to_string())?;

    let mut result = Vec::new();
    for script in scripts {
        let path = script["path"].as_str().unwrap_or("").trim();
        if !path.is_empty() && !result.iter().any(|existing| existing == path) {
            result.push(path.to_string());
        }
    }

    Ok(result)
}

fn profiles_dir() -> PathBuf {
    let mut path = local_data_dir();
    path.push("wingman");
    path.push("profiles");
    path
}

fn active_profile_path() -> PathBuf {
    let mut path = profiles_dir();
    path.pop();
    path.push("active_profile.txt");
    path
}

fn profile_path(id: &str) -> PathBuf {
    let mut path = profiles_dir();
    path.push(format!("{}.json", id));
    path
}

fn local_data_dir() -> PathBuf {
    #[cfg(windows)]
    {
        if let Some(value) = env::var_os("LOCALAPPDATA") {
            return PathBuf::from(value);
        }
        if let Some(value) = env::var_os("APPDATA") {
            return PathBuf::from(value);
        }
    }

    #[cfg(target_os = "macos")]
    {
        if let Some(value) = env::var_os("HOME") {
            return PathBuf::from(value).join("Library").join("Application Support");
        }
    }

    #[cfg(all(unix, not(target_os = "macos")))]
    {
        if let Some(value) = env::var_os("XDG_DATA_HOME") {
            return PathBuf::from(value);
        }
        if let Some(value) = env::var_os("HOME") {
            return PathBuf::from(value).join(".local").join("share");
        }
    }

    PathBuf::from(".")
}
