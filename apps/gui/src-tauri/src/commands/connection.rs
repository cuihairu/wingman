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

/// Validates that the endpoint name contains only safe characters.
/// Only alphanumeric, underscore, hyphen, and dot are allowed.
fn is_safe_name(name: &str) -> bool {
    !name.is_empty()
        && name.len() <= 128
        && name
            .chars()
            .all(|c| c.is_ascii_alphanumeric() || matches!(c, '_' | '-' | '.'))
}

fn validate_ipc_endpoint(endpoint: &str) -> Result<(), String> {
    let value = endpoint.trim();

    // Empty resolves to default "wingman" — ok
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

    // Strip the pipe prefix if present; otherwise validate as a bare pipe name
    let pipe_name = endpoint.strip_prefix(PREFIX).unwrap_or(endpoint);

    // Reject if it looks like an absolute path that bypassed strip_prefix
    if pipe_name.contains(&[':', '\\', '/'][..]) {
        return Err("Invalid Windows named pipe endpoint".to_string());
    }

    if !is_safe_name(pipe_name) {
        return Err("Invalid Windows named pipe endpoint".to_string());
    }

    Ok(())
}

#[cfg(unix)]
fn validate_platform_endpoint(endpoint: &str) -> Result<(), String> {
    // Bare name (e.g. "wingman") — must be a safe identifier
    if !endpoint.contains('/') {
        if !is_safe_name(endpoint) {
            return Err("Invalid Unix socket endpoint name".to_string());
        }
        return Ok(());
    }

    // Absolute path — only allowed under /tmp/ with safe path segments
    if !endpoint.starts_with('/') {
        return Err("Unix socket endpoint must be a bare name or absolute path".to_string());
    }

    // Require /tmp/ prefix to limit scope
    if !endpoint.starts_with("/tmp/") {
        return Err("Unix socket path must be under /tmp/".to_string());
    }

    // Validate every path segment
    for segment in endpoint.split('/').filter(|s| !s.is_empty()) {
        if segment == "." || segment == ".." {
            return Err("Path traversal not allowed in Unix socket endpoint".to_string());
        }
        if !is_safe_name(segment) {
            return Err(format!("Invalid segment '{}' in Unix socket endpoint", segment));
        }
    }

    Ok(())
}
