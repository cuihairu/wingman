use crate::ipc::client::IPC_QUICK_TIMEOUT;
use crate::state::AppState;
use serde::{Deserialize, Serialize};
use serde_json::json;

/// runtime 的远程注册配置（经 config.getRemote / config.setRemote 读写，
/// 仅本地 IPC——远程配置不允许从 Go server 侧改写，见架构决策文档）。
#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RemoteConfig {
    pub server_ip: String,
    pub server_port: u16,
    pub register_token: String,
}

#[tauri::command]
pub async fn get_remote_config(
    state: tauri::State<'_, AppState>,
) -> Result<RemoteConfig, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client
        .send_with_timeout("config.getRemote", json!({}), IPC_QUICK_TIMEOUT)
        .await?;

    if response["data"]["success"].as_bool() == Some(true) {
        let result = &response["data"]["result"];
        return Ok(RemoteConfig {
            server_ip: result["serverIp"].as_str().unwrap_or("").to_string(),
            server_port: result["serverPort"].as_u64().unwrap_or(0) as u16,
            register_token: result["registerToken"].as_str().unwrap_or("").to_string(),
        });
    }
    Err(response["data"]["error"]
        .as_str()
        .unwrap_or("Failed to get remote config")
        .to_string())
}

#[tauri::command]
pub async fn set_remote_config(
    state: tauri::State<'_, AppState>,
    server_ip: String,
    server_port: u16,
    register_token: String,
) -> Result<RemoteConfig, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client
        .send(
            "config.setRemote",
            json!({
                "serverIp": server_ip,
                "serverPort": server_port,
                "registerToken": register_token,
            }),
        )
        .await?;

    if response["data"]["success"].as_bool() == Some(true) {
        let result = &response["data"]["result"];
        return Ok(RemoteConfig {
            server_ip: result["serverIp"].as_str().unwrap_or("").to_string(),
            server_port: result["serverPort"].as_u64().unwrap_or(0) as u16,
            register_token: result["registerToken"].as_str().unwrap_or("").to_string(),
        });
    }
    Err(response["data"]["error"]
        .as_str()
        .unwrap_or("Failed to set remote config")
        .to_string())
}
