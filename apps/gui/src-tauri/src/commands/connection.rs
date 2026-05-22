use crate::state::AppState;
use std::time::SystemTime;

#[tauri::command]
pub async fn connect_websocket(
    state: tauri::State<'_, AppState>,
    url: Option<String>,
) -> Result<String, String> {
    let ws_url = url.unwrap_or_else(|| "ws://127.0.0.1:8080/ws".to_string());
    let mut client = state.ws_client.lock().await;
    client.url = ws_url.clone();
    client.connect().await?;

    let mut start_time = state.start_time.lock().await;
    *start_time = Some(SystemTime::now());

    Ok(format!("Connected to {}", ws_url))
}

#[tauri::command]
pub async fn disconnect_websocket(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let mut client = state.ws_client.lock().await;
    client.disconnect().await;
    Ok(())
}

#[tauri::command]
pub async fn is_connected(state: tauri::State<'_, AppState>) -> bool {
    state.ws_client.lock().await.connected
}
