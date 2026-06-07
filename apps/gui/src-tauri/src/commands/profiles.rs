use crate::hotkeys;
use serde_json::{json, Value};
use std::env;
use std::fs;
use std::path::PathBuf;
use std::time::{SystemTime, UNIX_EPOCH};
use tauri::AppHandle;

fn profiles_dir() -> PathBuf {
    let mut path = local_data_dir();
    path.push("wingman");
    path.push("profiles");
    fs::create_dir_all(&path).ok();
    path
}

fn active_profile_path() -> PathBuf {
    let mut path = profiles_dir();
    path.pop();
    path.push("active_profile.txt");
    path
}

fn profile_path(id: &str) -> PathBuf {
    // Validate that id is a safe filename to prevent path traversal attacks
    // Reject unsafe IDs instead of silently sanitizing (fail-closed approach)
    if !is_safe_filename(id) {
        panic!("Unsafe profile id detected: '{}'. Use set_active_profile() which validates before calling this.", id);
    }

    let mut path = profiles_dir();
    path.push(format!("{}.json", id));
    path
}

/// Check if a string is a safe filename (no path traversal)
fn is_safe_filename(id: &str) -> bool {
    if id.is_empty() {
        return false;
    }

    // Reject path separators and parent directory references
    if id.contains('/') || id.contains('\\') || id.contains("..") {
        return false;
    }

    // Only allow alphanumeric, underscore, hyphen, and dot
    id.chars().all(|c| c.is_alphanumeric() || c == '_' || c == '-' || c == '.')
}

/// Sanitize a filename by removing unsafe characters
fn local_data_dir() -> PathBuf {
    #[cfg(windows)]
    {
        if let Some(value) = env::var_os("LOCALAPPDATA") {
            return PathBuf::from(value);
        }
        if let Some(value) = env::var_os("APPDATA") {
            return PathBuf::from(value);
        }
    }

    #[cfg(target_os = "macos")]
    {
        if let Some(value) = env::var_os("HOME") {
            return PathBuf::from(value).join("Library").join("Application Support");
        }
    }

    #[cfg(all(unix, not(target_os = "macos")))]
    {
        if let Some(value) = env::var_os("XDG_DATA_HOME") {
            return PathBuf::from(value);
        }
        if let Some(value) = env::var_os("HOME") {
            return PathBuf::from(value).join(".local").join("share");
        }
    }

    PathBuf::from(".")
}

fn timestamp_millis() -> u128 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_millis())
        .unwrap_or_default()
}

fn load_all_profiles() -> Vec<Value> {
    let dir = profiles_dir();
    let mut profiles = Vec::new();
    if let Ok(entries) = fs::read_dir(&dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.extension().map_or(false, |e| e == "json") {
                if let Ok(content) = fs::read_to_string(&path) {
                    if let Ok(profile) = serde_json::from_str::<Value>(&content) {
                        profiles.push(profile);
                    }
                }
            }
        }
    }
    profiles
}

#[tauri::command]
pub async fn get_profiles() -> Result<Vec<Value>, String> {
    Ok(load_all_profiles())
}

#[tauri::command]
pub async fn get_active_profile() -> Result<Value, String> {
    let path = active_profile_path();
    if let Ok(active_id) = fs::read_to_string(&path) {
        let active_id = active_id.trim();
        if !active_id.is_empty() {
            let profile_path = profile_path(active_id);
            if let Ok(content) = fs::read_to_string(&profile_path) {
                if let Ok(profile) = serde_json::from_str::<Value>(&content) {
                    return Ok(profile);
                }
            }
        }
    }
    Ok(json!(null))
}

#[tauri::command]
pub async fn set_active_profile(app: AppHandle, id: String) -> Result<(), String> {
    // Validate id to prevent injection attacks
    if !is_safe_filename(&id) {
        return Err("Invalid profile id: contains unsafe characters".to_string());
    }

    let path = active_profile_path();
    fs::write(&path, &id).map_err(|e| e.to_string())?;
    hotkeys::reload_hotkeys(&app)?;
    Ok(())
}

#[tauri::command]
pub async fn create_profile(name: String) -> Result<String, String> {
    let id = format!("profile_{}", timestamp_millis());
    let profile = json!({
        "id": id,
        "name": name,
        "version": "1.0",
        "description": "",
        "window": { "title": "", "className": "", "processName": "", "exactMatch": false, "fullscreen": false },
        "colors": [],
        "images": [],
        "triggers": [],
        "scripts": [],
        "hotkeys": {
            "start": ["F5"],
            "stop": ["F6"],
            "pause": ["F7"],
            "emergencyStop": ["F12"]
        },
        "settings": {}
    });
    let path = profile_path(&id);
    let content = serde_json::to_string_pretty(&profile).map_err(|e| e.to_string())?;
    fs::write(&path, content).map_err(|e| e.to_string())?;
    Ok(id)
}

#[tauri::command]
pub async fn delete_profile(app: AppHandle, id: String) -> Result<(), String> {
    let path = profile_path(&id);
    if path.exists() {
        fs::remove_file(&path).map_err(|e| e.to_string())?;
    }
    // If this was the active profile, clear it
    let active_path = active_profile_path();
    if let Ok(active_id) = fs::read_to_string(&active_path) {
        if active_id.trim() == id {
            fs::write(&active_path, "").ok();
            hotkeys::reload_hotkeys(&app)?;
        }
    }
    Ok(())
}

#[tauri::command]
pub async fn update_profile(app: AppHandle, profile: Value) -> Result<(), String> {
    let id = profile["id"]
        .as_str()
        .ok_or("Profile missing id field")?
        .to_string();
    let path = profile_path(&id);
    let content = serde_json::to_string_pretty(&profile).map_err(|e| e.to_string())?;
    fs::write(&path, content).map_err(|e| e.to_string())?;

    if let Ok(active_id) = fs::read_to_string(active_profile_path()) {
        if active_id.trim() == id {
            hotkeys::reload_hotkeys(&app)?;
        }
    }

    Ok(())
}

#[tauri::command]
pub async fn export_profile_json(id: String) -> Result<String, String> {
    let path = profile_path(&id);
    fs::read_to_string(&path).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn import_profile_json(json: String) -> Result<(), String> {
    let profile: Value = serde_json::from_str(&json).map_err(|e| e.to_string())?;
    let id = profile["id"]
        .as_str()
        .ok_or("Profile missing id field")?
        .to_string();
    let path = profile_path(&id);
    let content = serde_json::to_string_pretty(&profile).map_err(|e| e.to_string())?;
    fs::write(&path, content).map_err(|e| e.to_string())?;
    Ok(())
}
