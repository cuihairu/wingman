use crate::models::{ScreenRegion, ScreenshotInfo};
use crate::state::AppState;
use serde_json::json;

#[tauri::command]
pub async fn capture_screenshot(
    state: tauri::State<'_, AppState>,
    region: ScreenRegion,
) -> Result<ScreenshotInfo, String> {
    let mut client = state.ipc_client.lock().await;
    let response = client
        .send(
            "screenshot.capture",
            json!({
                "region": {
                    "x": region.x,
                    "y": region.y,
                    "width": region.width,
                    "height": region.height,
                }
            }),
        )
        .await?;

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
