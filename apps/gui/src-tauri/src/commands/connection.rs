use crate::state::AppState;
use std::time::SystemTime;

#[tauri::command]
pub async fn connect_ipc(
    state: tauri::State<'_, AppState>,
    endpoint: Option<String>,
) -> Result<String, String> {
    let ipc_endpoint = endpoint.unwrap_or_else(|| "wingman".to_string());
    let mut client = state.ipc_client.lock().await;
    client.endpoint = ipc_endpoint.clone();
    client.connect().await?;

    let mut start_time = state.start_time.lock().await;
    *start_time = Some(SystemTime::now());

    Ok(format!("Connected to local IPC endpoint {}", ipc_endpoint))
}

#[tauri::command]
pub async fn disconnect_ipc(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let mut client = state.ipc_client.lock().await;
    client.disconnect().await;
    Ok(())
}

#[tauri::command]
pub async fn is_connected(state: tauri::State<'_, AppState>) -> bool {
    state.ipc_client.lock().await.connected
}
