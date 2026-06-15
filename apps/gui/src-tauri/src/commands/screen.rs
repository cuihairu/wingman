use crate::models::{MonitorInfo, ScreenRegion, ScreenshotInfo};
use crate::state::AppState;
use serde_json::json;

// Capture a screenshot. `display_id` is optional: when `Some(id)` it targets
// a specific monitor (resolved via `screen.listMonitors`); when `None` the
// runtime captures the primary monitor (backwards compatible).
#[tauri::command]
pub async fn capture_screenshot(
    state: tauri::State<'_, AppState>,
    region: ScreenRegion,
    display_id: Option<i32>,
) -> Result<ScreenshotInfo, String> {
    let mut payload = json!({
        "region": {
            "x": region.x,
            "y": region.y,
            "width": region.width,
            "height": region.height,
        }
    });
    if let Some(id) = display_id {
        payload["displayId"] = json!(id);
    }

    let mut client = state.ipc_client.lock().await;
    let response = client.send("screenshot.capture", payload).await?;

    if response["data"]["success"].as_bool().unwrap_or(false) {
        let result = &response["data"]["result"];
        let response_region = &result["region"];
        return Ok(ScreenshotInfo {
            image: result["image"].as_str().unwrap_or("").to_string(),
            width: result["width"].as_u64().unwrap_or(0) as u32,
            height: result["height"].as_u64().unwrap_or(0) as u32,
            timestamp: result["timestamp"].as_u64().unwrap_or(0),
            region: ScreenRegion {
                x: response_region["x"].as_i64().unwrap_or(region.x as i64) as i32,
                y: response_region["y"].as_i64().unwrap_or(region.y as i64) as i32,
                width: response_region["width"].as_i64().unwrap_or(region.width as i64) as i32,
                height: response_region["height"].as_i64().unwrap_or(region.height as i64) as i32,
            },
        });
    }

    Err(response["data"]["error"]
        .as_str()
        .unwrap_or("Failed to capture screenshot")
        .to_string())
}

#[tauri::command]
pub async fn list_monitors(
    state: tauri::State<'_, AppState>,
) -> Result<Vec<MonitorInfo>, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client.send("screen.listMonitors", json!({})).await?;

    if !response["data"]["success"].as_bool().unwrap_or(false) {
        return Err(response["data"]["error"]
            .as_str()
            .unwrap_or("Failed to list monitors")
            .to_string());
    }

    let monitors = response["data"]["result"]["monitors"]
        .as_array()
        .ok_or_else(|| "Invalid monitors response".to_string())?;

    let result: Vec<MonitorInfo> = monitors
        .iter()
        .map(|m| {
            let bounds = &m["bounds"];
            MonitorInfo {
                id: m["id"].as_i64().unwrap_or(0) as i32,
                name: m["name"].as_str().unwrap_or("").to_string(),
                is_primary: m["isPrimary"].as_bool().unwrap_or(false),
                bounds: ScreenRegion {
                    x: bounds["x"].as_i64().unwrap_or(0) as i32,
                    y: bounds["y"].as_i64().unwrap_or(0) as i32,
                    width: bounds["width"].as_i64().unwrap_or(0) as i32,
                    height: bounds["height"].as_i64().unwrap_or(0) as i32,
                },
            }
        })
        .collect();

    Ok(result)
}
