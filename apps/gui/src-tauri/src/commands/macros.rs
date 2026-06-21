use crate::state::AppState;
use serde_json::json;

/// 开始录制宏（runtime 装低层钩子 + 启动消息泵线程捕获输入事件）。
#[tauri::command]
pub async fn macro_record(state: tauri::State<'_, AppState>) -> Result<serde_json::Value, String> {
	let mut client = state.ipc_client.lock().await;
	let resp = client.send("macro.start", json!({})).await?;
	Ok(resp["data"]["result"].clone())
}

/// 停止录制，返回捕获到的事件数。
#[tauri::command]
pub async fn macro_stop(state: tauri::State<'_, AppState>) -> Result<serde_json::Value, String> {
	let mut client = state.ipc_client.lock().await;
	let resp = client.send("macro.stop", json!({})).await?;
	Ok(resp["data"]["result"].clone())
}

/// 回放宏。speed=100 原速；repeat 重复次数。同步执行（含事件间 sleep）。
#[tauri::command]
pub async fn macro_play(
	state: tauri::State<'_, AppState>,
	speed: Option<u32>,
	repeat: Option<u32>,
) -> Result<serde_json::Value, String> {
	let mut client = state.ipc_client.lock().await;
	let resp = client
		.send("macro.play", json!({ "speed": speed.unwrap_or(100), "repeat": repeat.unwrap_or(1) }))
		.await?;
	Ok(resp["data"]["result"].clone())
}

/// 查询录制状态：{ recording, paused, eventCount }。
#[tauri::command]
pub async fn macro_status(state: tauri::State<'_, AppState>) -> Result<serde_json::Value, String> {
	let mut client = state.ipc_client.lock().await;
	let resp = client.send("macro.status", json!({})).await?;
	Ok(resp["data"]["result"].clone())
}

/// 保存当前录制为 JSON 文件。
#[tauri::command]
pub async fn macro_save(state: tauri::State<'_, AppState>, path: String) -> Result<serde_json::Value, String> {
	let mut client = state.ipc_client.lock().await;
	let resp = client.send("macro.save", json!({ "path": path })).await?;
	Ok(resp["data"]["result"].clone())
}

/// 从 JSON 文件载入宏。
#[tauri::command]
pub async fn macro_load(state: tauri::State<'_, AppState>, path: String) -> Result<serde_json::Value, String> {
	let mut client = state.ipc_client.lock().await;
	let resp = client.send("macro.load", json!({ "path": path })).await?;
	Ok(resp["data"]["result"].clone())
}

/// 清空当前录制。
#[tauri::command]
pub async fn macro_clear(state: tauri::State<'_, AppState>) -> Result<serde_json::Value, String> {
	let mut client = state.ipc_client.lock().await;
	let resp = client.send("macro.clear", json!({})).await?;
	Ok(resp["data"]["result"].clone())
}
