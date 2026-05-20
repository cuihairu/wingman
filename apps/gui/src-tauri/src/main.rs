use futures_util::{SinkExt, StreamExt};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::sync::Arc;
use std::time::SystemTime;
use tokio::sync::Mutex;
use tokio_tungstenite::{connect_async, tungstenite::protocol::Message};
use url::Url;

/// WebSocket 客户端状态
struct WebSocketClient {
    sender: Option<tokio_tungstenite::WebSocketStream<
        tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>,
    >>,
    url: String,
    connected: bool,
}

impl WebSocketClient {
    fn new(url: String) -> Self {
        Self {
            sender: None,
            url,
            connected: false,
        }
    }

    async fn connect(&mut self) -> Result<(), String> {
        let url = Url::parse(&self.url).map_err(|e| e.to_string())?;
        let (ws_stream, _) = connect_async(url).await.map_err(|e| e.to_string())?;
        self.sender = Some(ws_stream);
        self.connected = true;
        Ok(())
    }

    async fn disconnect(&mut self) {
        self.sender = None;
        self.connected = false;
    }

    async fn send(&mut self, method: &str, params: Value) -> Result<Value, String> {
        if let Some(ws) = &mut self.sender {
            let request = json!({
                "type": "call",
                "id": uuid::Uuid::new_v4().to_string(),
                "method": method,
                "params": params
            });

            ws.send(Message::Text(request.to_string()))
                .await
                .map_err(|e| e.to_string())?;

            if let Some(msg) = ws.next().await {
                match msg {
                    Ok(Message::Text(text)) => {
                        let response: Value = serde_json::from_str(&text)
                            .map_err(|e| format!("Failed to parse response: {}", e))?;
                        return Ok(response);
                    }
                    Ok(_) => return Err("Unexpected message type".to_string()),
                    Err(e) => return Err(e.to_string()),
                }
            }
        }
        Err("Not connected".to_string())
    }
}

/// 应用状态
struct AppState {
    ws_client: Arc<Mutex<WebSocketClient>>,
    paused: Arc<Mutex<bool>>,
    start_time: Arc<Mutex<Option<SystemTime>>>,
}

// Tauri 命令
#[tauri::command]
async fn connect_websocket(
    state: tauri::State<'_, AppState>,
    url: Option<String>,
) -> Result<String, String> {
    let ws_url = url.unwrap_or_else(|| "ws://127.0.0.1:8080/ws".to_string());
    let mut client = state.ws_client.lock().await;
    client.url = ws_url.clone();
    client.connect().await?;

    // 设置启动时间
    let mut start_time = state.start_time.lock().await;
    *start_time = Some(SystemTime::now());

    Ok(format!("Connected to {}", ws_url))
}

#[tauri::command]
async fn disconnect_websocket(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let mut client = state.ws_client.lock().await;
    client.disconnect().await;
    Ok(())
}

#[tauri::command]
async fn is_connected(state: tauri::State<'_, AppState>) -> bool {
    state.ws_client.lock().await.connected
}

#[tauri::command]
async fn call_rpc(
    state: tauri::State<'_, AppState>,
    method: String,
    params: Option<Value>,
) -> Result<Value, String> {
    let mut client = state.ws_client.lock().await;
    let params = params.unwrap_or_else(|| json!({}));
    client.send(&method, params).await
}

#[tauri::command]
async fn get_scripts(
    state: tauri::State<'_, AppState>,
) -> Result<Vec<ScriptInfo>, String> {
    let mut client = state.ws_client.lock().await;
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
async fn start_script(
    state: tauri::State<'_, AppState>,
    id: String,
) -> Result<ScriptStatus, String> {
    let mut client = state.ws_client.lock().await;
    let response = client.send("script.start", json!({ "path": format!("scripts/{}.lua", id) })).await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            return Ok(ScriptStatus {
                script_id: response["data"]["result"]["scriptId"].as_str().unwrap_or("").to_string(),
                status: response["data"]["result"]["status"].as_str().unwrap_or("").to_string(),
            });
        }
    }
    Err("Failed to start script".to_string())
}

#[tauri::command]
async fn stop_script(
    state: tauri::State<'_, AppState>,
    script_id: String,
) -> Result<String, String> {
    let mut client = state.ws_client.lock().await;
    let response = client.send("script.stop", json!({ "scriptId": script_id })).await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            return Ok("Script stopped".to_string());
        }
    }
    Err("Failed to stop script".to_string())
}

#[tauri::command]
async fn pause_all(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let mut paused = state.paused.lock().await;
    *paused = true;
    Ok(())
}

#[tauri::command]
async fn resume_all(state: tauri::State<'_, AppState>) -> Result<(), String> {
    let mut paused = state.paused.lock().await;
    *paused = false;
    Ok(())
}

#[tauri::command]
async fn is_paused(state: tauri::State<'_, AppState>) -> bool {
    *state.paused.lock().await
}

#[tauri::command]
async fn get_system_status(
    state: tauri::State<'_, AppState>,
) -> Result<SystemStatus, String> {
    let mut client = state.ws_client.lock().await;
    let response = client.send("system.getStatus", json!({})).await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            let uptime = response["data"]["result"]["uptime"].as_u64().unwrap_or(0);
            return Ok(SystemStatus {
                server: response["data"]["result"]["server"].as_str().unwrap_or("").to_string(),
                version: response["data"]["result"]["version"].as_str().unwrap_or("").to_string(),
                uptime,
                running_scripts: response["data"]["result"]["runningScripts"].as_u64().unwrap_or(0),
                paused: *state.paused.lock().await,
            });
        }
    }
    Err("Failed to get system status".to_string())
}

#[tauri::command]
async fn get_version(
    state: tauri::State<'_, AppState>,
) -> Result<VersionInfo, String> {
    let mut client = state.ws_client.lock().await;
    let response = client.send("system.getVersion", json!({})).await?;

    if let Some(success) = response["data"]["success"].as_bool() {
        if success {
            return Ok(VersionInfo {
                server: response["data"]["result"]["server"].as_str().unwrap_or("").to_string(),
                version: response["data"]["result"]["version"].as_str().unwrap_or("").to_string(),
                build_date: response["data"]["result"]["buildDate"].as_str().unwrap_or("").to_string(),
            });
        }
    }
    Err("Failed to get version".to_string())
}

#[tauri::command]
async fn get_runtime_info(state: tauri::State<'_, AppState>) -> Result<RuntimeInfo, String> {
    let paused = *state.paused.lock().await;
    let start_time = *state.start_time.lock().await;
    let uptime = if let Some(st) = start_time {
        st.duration_since(SystemTime::UNIX_EPOCH).unwrap_or_default().as_secs()
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
async fn toggle_pause(state: tauri::State<'_, AppState>) -> Result<bool, String> {
    let mut paused = state.paused.lock().await;
    *paused = !*paused;
    Ok(*paused)
}

// 数据结构
#[derive(Debug, Clone, Serialize, Deserialize)]
struct ScriptInfo {
    id: String,
    name: String,
    path: String,
    size: u64,
    is_running: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct ScriptStatus {
    script_id: String,
    status: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct SystemStatus {
    server: String,
    version: String,
    uptime: u64,
    running_scripts: u64,
    paused: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct VersionInfo {
    server: String,
    version: String,
    build_date: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct RuntimeInfo {
    paused: bool,
    uptime: u64,
    memory_usage: u64,
    cpu_usage: f64,
}

fn main() {
    let ws_client = Arc::new(Mutex::new(WebSocketClient::new("ws://127.0.0.1:8080/ws".to_string())));
    let paused = Arc::new(Mutex::new(false));
    let start_time = Arc::new(Mutex::new(Some(SystemTime::now())));

    tauri::Builder::default()
        .manage(AppState {
            ws_client,
            paused,
            start_time,
        })
        .invoke_handler(tauri::generate_handler![
            connect_websocket,
            disconnect_websocket,
            is_connected,
            call_rpc,
            get_scripts,
            start_script,
            stop_script,
            pause_all,
            resume_all,
            is_paused,
            get_system_status,
            get_version,
            get_runtime_info,
            toggle_pause,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
