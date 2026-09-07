use crate::state::AppState;
use serde::Serialize;
use serde_json::json;

/// 单条从 runtime 拉取的事件，透传给前端。
#[derive(Debug, Serialize)]
pub struct RuntimeEvent {
    pub method: String,
    pub payload: serde_json::Value,
    pub timestamp: u64,
}

/// events.drain 结果：事件列表 + runtime 有界缓冲的溢出/剩余计数。
#[derive(Debug, Serialize, Default)]
pub struct DrainResult {
    pub events: Vec<RuntimeEvent>,
    /// 缓冲溢出被丢弃的事件总数（累计，log.line 优先被丢）
    pub dropped: u64,
    /// 本次拉取后缓冲中剩余的事件数（>0 表示 GUI 消费速度跟不上）
    pub remaining: u64,
}

/// 拉取并清空 runtime 缓冲的事件（log.line / trigger.fired / script.* ...）。
///
/// 采用 pull 模型：GUI 轮询调用此命令，runtime 端 drain 缓冲区。
/// 这避免了为 Windows 阻塞 IO 引入异步读取循环带来的帧错位风险。
#[tauri::command]
pub async fn drain_events(state: tauri::State<'_, AppState>) -> Result<DrainResult, String> {
    let mut client = state.ipc_client.lock().await;
    if !client.connected {
        return Ok(DrainResult::default());
    }

    let response = client.send("events.drain", json!({})).await?;

    let success = response["data"]["success"].as_bool().unwrap_or(false);
    if !success {
        return Err(response["data"]["error"]
            .as_str()
            .unwrap_or("events.drain failed")
            .to_string());
    }

    let result = &response["data"]["result"];
    let events_arr = result["events"].as_array();
    let mut out = DrainResult {
        dropped: result["dropped"].as_u64().unwrap_or(0),
        remaining: result["remaining"].as_u64().unwrap_or(0),
        events: Vec::new(),
    };
    if let Some(arr) = events_arr {
        out.events.reserve(arr.len());
        for evt in arr {
            out.events.push(RuntimeEvent {
                method: evt["method"].as_str().unwrap_or("").to_string(),
                payload: evt["payload"].clone(),
                timestamp: evt["timestamp"].as_u64().unwrap_or(0),
            });
        }
    }
    Ok(out)
}
