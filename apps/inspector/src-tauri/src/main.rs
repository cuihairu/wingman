use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::sync::Arc;
use tokio::sync::Mutex;
use tokio_tungstenite::{connect_async, tungstenite::protocol::Message};
use url::Url;

/// WebSocket 客户端
struct WebSocketClient {
    sender: Option<tokio_tungstenite::WebSocketStream<
        tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>,
    >>,
    url: String,
}

impl WebSocketClient {
    fn new(url: String) -> Self {
        Self { sender: None, url }
    }

    async fn connect(&mut self) -> Result<(), String> {
        let url = Url::parse(&self.url).map_err(|e| e.to_string())?;
        let (ws_stream, _) = connect_async(url).await.map_err(|e| e.to_string())?;
        self.sender = Some(ws_stream);
        Ok(())
    }

    async fn call(&mut self, method: &str, params: Value) -> Result<Value, String> {
        use futures_util::{SinkExt, StreamExt};

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
                            .map_err(|e| format!("Parse error: {}", e))?;
                        return Ok(response);
                    }
                    Ok(_) => return Err("Unexpected message".to_string()),
                    Err(e) => return Err(e.to_string()),
                }
            }
        }
        Err("Not connected".to_string())
    }
}

/// 应用状态
struct AppState {
    ws: Arc<Mutex<WebSocketClient>>,
}

// ========== 基础连接 ==========

#[tauri::command]
async fn connect(state: tauri::State<'_, AppState>, url: Option<String>) -> Result<String, String> {
    let ws_url = url.unwrap_or_else(|| "ws://127.0.0.1:8080/ws".to_string());
    let mut client = state.ws.lock().await;
    client.url = ws_url.clone();
    client.connect().await?;
    Ok(ws_url)
}

// ========== 取色器 ==========

#[tauri::command]
async fn capture_pixel(
    state: tauri::State<'_, AppState>,
    x: i32,
    y: i32,
) -> Result<PixelInfo, String> {
    let mut client = state.ws.lock().await;
    let response = client.call("inspector.capturePixel", json!({ "x": x, "y": y })).await?;

    if response["data"]["success"].as_bool() == Some(true) {
        let r = response["data"]["result"]["r"].as_u64().unwrap_or(0) as u8;
        let g = response["data"]["result"]["g"].as_u64().unwrap_or(0) as u8;
        let b = response["data"]["result"]["b"].as_u64().unwrap_or(0) as u8;
        Ok(PixelInfo {
            x, y,
            r, g, b,
            hex: format!("{:02X}{:02X}{:02X}", r, g, b),
            rgb: format!("rgb({}, {}, {})", r, g, b),
        })
    } else {
        Err("Capture failed".to_string())
    }
}

// ========== 窗口/句柄 ==========

#[tauri::command]
async fn enum_windows(
    state: tauri::State<'_, AppState>,
) -> Result<Vec<WindowInfo>, String> {
    let mut client = state.ws.lock().await;
    let response = client.call("inspector.enumWindows", json!({})).await?;

    if response["data"]["success"].as_bool() == Some(true) {
        if let Some(windows) = response["data"]["result"]["windows"].as_array() {
            return Ok(windows.iter().map(|w| WindowInfo {
                hwnd: w["hwnd"].as_str().unwrap_or("").to_string(),
                title: w["title"].as_str().unwrap_or("").to_string(),
                class: w["class"].as_str().unwrap_or("").to_string(),
                rect: RectInfo {
                    left: w["rect"]["left"].as_i64().unwrap_or(0),
                    top: w["rect"]["top"].as_i64().unwrap_or(0),
                    right: w["rect"]["right"].as_i64().unwrap_or(0),
                    bottom: w["rect"]["bottom"].as_i64().unwrap_or(0),
                },
            }).collect());
        }
    }
    Err("Enum windows failed".to_string())
}

#[tauri::command]
async fn get_window_rect(
    state: tauri::State<'_, AppState>,
    hwnd: String,
) -> Result<RectInfo, String> {
    let mut client = state.ws.lock().await;
    let response = client.call("inspector.getWindowRect", json!({ "hwnd": hwnd })).await?;

    if response["data"]["success"].as_bool() == Some(true) {
        let r = &response["data"]["result"];
        Ok(RectInfo {
            left: r["left"].as_i64().unwrap_or(0),
            top: r["top"].as_i64().unwrap_or(0),
            right: r["right"].as_i64().unwrap_or(0),
            bottom: r["bottom"].as_i64().unwrap_or(0),
        })
    } else {
        Err("Get rect failed".to_string())
    }
}

// ========== UIA ==========

#[tauri::command]
async fn uia_get_element_tree(
    state: tauri::State<'_, AppState>,
    hwnd: String,
) -> Result<UIAElement, String> {
    let mut client = state.ws.lock().await;
    let response = client.call("inspector.uiaGetElementTree", json!({ "hwnd": hwnd })).await?;

    if response["data"]["success"].as_bool() == Some(true) {
        // 简化解析，实际需要递归
        let r = &response["data"]["result"];
        Ok(UIAElement {
            name: r["name"].as_str().unwrap_or("").to_string(),
            class_name: r["className"].as_str().unwrap_or("").to_string(),
            control_type: r["controlType"].as_str().unwrap_or("").to_string(),
            automation_id: r["automationId"].as_str().unwrap_or("").to_string(),
            bounds: RectInfo {
                left: r["bounds"]["left"].as_i64().unwrap_or(0),
                top: r["bounds"]["top"].as_i64().unwrap_or(0),
                right: r["bounds"]["right"].as_i64().unwrap_or(0),
                bottom: r["bounds"]["bottom"].as_i64().unwrap_or(0),
            },
            children: vec![],
        })
    } else {
        Err("UIA get element tree failed".to_string())
    }
}

// ========== 代码片段执行 ==========

#[tauri::command]
async fn eval_code(
    state: tauri::State<'_, AppState>,
    code: String,
) -> Result<EvalResult, String> {
    let mut client = state.ws.lock().await;
    let response = client.call("inspector.evalCode", json!({ "code": code })).await?;

    if response["data"]["success"].as_bool() == Some(true) {
        Ok(EvalResult {
            output: response["data"]["result"]["output"].as_str().unwrap_or("").to_string(),
            success: true,
        })
    } else {
        Ok(EvalResult {
            output: response["data"]["error"].as_str().unwrap_or("Unknown error").to_string(),
            success: false,
        })
    }
}

// ========== 图找图 ==========

#[tauri::command]
async fn find_image(
    state: tauri::State<'_, AppState>,
    image_path: String,
    similarity: f64,
) -> Result<FindImageResult, String> {
    let mut client = state.ws.lock().await;
    let response = client.call("inspector.findImage", json!({
        "imagePath": image_path,
        "similarity": similarity
    })).await?;

    if response["data"]["success"].as_bool() == Some(true) {
        let r = &response["data"]["result"];
        Ok(FindImageResult {
            found: r["found"].as_bool().unwrap_or(false),
            x: r["x"].as_i64().unwrap_or(0),
            y: r["y"].as_i64().unwrap_or(0),
            similarity: r["similarity"].as_f64().unwrap_or(0.0),
        })
    } else {
        Ok(FindImageResult {
            found: false,
            x: 0,
            y: 0,
            similarity: 0.0,
        })
    }
}

// ========== 数据结构 ==========

#[derive(Debug, Clone, Serialize, Deserialize)]
struct PixelInfo {
    x: i32, y: i32,
    r: u8, g: u8, b: u8,
    hex: String,
    rgb: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct RectInfo {
    left: i64, top: i64, right: i64, bottom: i64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct WindowInfo {
    hwnd: String,
    title: String,
    class: String,
    rect: RectInfo,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct UIAElement {
    name: String,
    class_name: String,
    control_type: String,
    automation_id: String,
    bounds: RectInfo,
    children: Vec<UIAElement>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct EvalResult {
    output: String,
    success: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct FindImageResult {
    found: bool,
    x: i64, y: i64,
    similarity: f64,
}

fn main() {
    let ws = Arc::new(Mutex::new(WebSocketClient::new("ws://127.0.0.1:8080/ws".to_string())));

    tauri::Builder::default()
        .manage(AppState { ws })
        .invoke_handler(tauri::generate_handler![
            connect,
            capture_pixel,
            enum_windows,
            get_window_rect,
            uia_get_element_tree,
            eval_code,
            find_image,
        ])
        .run(tauri::generate_context!())
        .expect("error while running inspector");
}
