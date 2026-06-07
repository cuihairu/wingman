use crate::state::AppState;
use std::time::SystemTime;

#[tauri::command]
pub async fn connect_ipc(
    state: tauri::State<'_, AppState>,
    endpoint: Option<String>,
) -> Result<String, String> {
    let ipc_endpoint = endpoint.unwrap_or_else(|| "wingman".to_string());
    validate_ipc_endpoint(&ipc_endpoint)?;
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

fn validate_ipc_endpoint(endpoint: &str) -> Result<(), String> {
    let value = endpoint.trim();
    if value.is_empty() {
        return Ok(());
    }

    if value.len() > 240 || value.chars().any(char::is_control) {
        return Err("Invalid IPC endpoint".to_string());
    }

    validate_platform_endpoint(value)
}

#[cfg(windows)]
fn validate_platform_endpoint(endpoint: &str) -> Result<(), String> {
    const PREFIX: &str = r"\\.\pipe\";
    let pipe_name = endpoint.strip_prefix(PREFIX).unwrap_or(endpoint);

    if pipe_name.is_empty()
        || pipe_name.contains('\\')
        || pipe_name.contains('/')
        || pipe_name.contains("..")
        || !pipe_name
            .chars()
            .all(|c| c.is_ascii_alphanumeric() || matches!(c, '_' | '-' | '.'))
    {
        return Err("Invalid Windows named pipe endpoint".to_string());
    }

    Ok(())
}

#[cfg(unix)]
fn validate_platform_endpoint(endpoint: &str) -> Result<(), String> {
    if endpoint == "wingman" {
        return Ok(());
    }
    if !endpoint.starts_with('/') || endpoint.contains("..") {
        return Err("Invalid Unix socket endpoint".to_string());
    }
    Ok(())
}
