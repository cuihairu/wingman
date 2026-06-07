use crate::state::AppState;
use serde::Deserialize;
use std::collections::HashSet;
use std::env;
use std::fs;
use std::path::PathBuf;
use tauri::{AppHandle, Manager, Runtime};
use tauri_plugin_global_shortcut::{GlobalShortcutExt, ShortcutEvent, ShortcutState};

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(default)]
struct ProfileHotkeys {
    start: Vec<String>,
    stop: Vec<String>,
    pause: Vec<String>,
    #[serde(rename = "emergencyStop")]
    emergency_stop: Vec<String>,
}

#[derive(Debug, Clone, Copy)]
enum HotkeyAction {
    Start,
    Stop,
    PauseToggle,
    EmergencyStop,
}

pub fn setup_hotkeys<R: Runtime>(app: &AppHandle<R>) -> Result<(), String> {
    register_profile_hotkeys(app)
}

pub fn reload_hotkeys<R: Runtime>(app: &AppHandle<R>) -> Result<(), String> {
    register_profile_hotkeys(app)
}

fn register_profile_hotkeys<R: Runtime>(app: &AppHandle<R>) -> Result<(), String> {
    let hotkeys = load_active_hotkeys();
    let manager = app.global_shortcut();
    let mut registered = HashSet::new();

    manager
        .unregister_all()
        .map_err(|e| format!("failed to clear global shortcuts: {e}"))?;

    register_action_shortcuts(app, &hotkeys.start, HotkeyAction::Start, &mut registered)?;
    register_action_shortcuts(app, &hotkeys.stop, HotkeyAction::Stop, &mut registered)?;
    register_action_shortcuts(
        app,
        &hotkeys.pause,
        HotkeyAction::PauseToggle,
        &mut registered,
    )?;
    register_action_shortcuts(
        app,
        &hotkeys.emergency_stop,
        HotkeyAction::EmergencyStop,
        &mut registered,
    )?;

    Ok(())
}

fn register_action_shortcuts<R: Runtime>(
    app: &AppHandle<R>,
    shortcuts: &[String],
    action: HotkeyAction,
    registered: &mut HashSet<String>,
) -> Result<(), String> {
    for shortcut in normalize_shortcuts(shortcuts) {
        if !registered.insert(shortcut.clone()) {
            eprintln!("duplicate global shortcut ignored: {shortcut}");
            continue;
        }

        app.global_shortcut()
            .on_shortcut(shortcut.as_str(), move |app, _, event| {
                handle_shortcut_event(app, action, event);
            })
            .map_err(|e| format!("failed to register shortcut `{shortcut}`: {e}"))?;
    }

    Ok(())
}

fn handle_shortcut_event<R: Runtime>(app: &AppHandle<R>, action: HotkeyAction, event: ShortcutEvent) {
    if event.state != ShortcutState::Pressed {
        return;
    }

    let handle = app.clone();
    tauri::async_runtime::spawn(async move {
        if let Err(error) = execute_hotkey_action(&handle, action).await {
            eprintln!("global hotkey action failed: {error}");
        }
    });
}

async fn execute_hotkey_action<R: Runtime>(
    app: &AppHandle<R>,
    action: HotkeyAction,
) -> Result<(), String> {
    let state = app.state::<AppState>();
    let mut client = state.ipc_client.lock().await;

    if !client.connected {
        return Err("runtime IPC is not connected".to_string());
    }

    let method = match action {
        HotkeyAction::Start => "profile.startActiveScripts",
        HotkeyAction::Stop => "profile.stopActiveScripts",
        HotkeyAction::PauseToggle => "system.togglePause",
        HotkeyAction::EmergencyStop => "system.stopAll",
    };

    match action {
        HotkeyAction::Start => {
            drop(client);
            crate::commands::scripts::start_active_profile_scripts_with_client(&state.ipc_client)
                .await?;
            *state.paused.lock().await = false;
        }
        HotkeyAction::Stop => {
            drop(client);
            crate::commands::scripts::stop_active_profile_scripts_with_client(&state.ipc_client)
                .await?;
            *state.paused.lock().await = false;
        }
        HotkeyAction::PauseToggle => {
            let response = client.send(method, serde_json::json!({})).await?;
            let paused = response["data"]["result"]["paused"].as_bool().unwrap_or(false);
            *state.paused.lock().await = paused;
        }
        HotkeyAction::EmergencyStop => {
            client.send(method, serde_json::json!({})).await?;
            *state.paused.lock().await = false;
        }
    }

    Ok(())
}

fn load_active_hotkeys() -> ProfileHotkeys {
    let active_id = fs::read_to_string(active_profile_path())
        .map(|value| value.trim().to_string())
        .unwrap_or_default();

    if active_id.is_empty() {
        return default_hotkeys();
    }

    let profile_path = profiles_dir().join(format!("{active_id}.json"));
    let content = match fs::read_to_string(profile_path) {
        Ok(value) => value,
        Err(_) => return default_hotkeys(),
    };

    let profile: serde_json::Value = match serde_json::from_str(&content) {
        Ok(value) => value,
        Err(_) => return default_hotkeys(),
    };

    serde_json::from_value(
        profile
            .get("hotkeys")
            .cloned()
            .unwrap_or_else(|| serde_json::json!({})),
    )
        .unwrap_or_else(|_| default_hotkeys())
}

fn normalize_shortcuts(shortcuts: &[String]) -> Vec<String> {
    let mut normalized = Vec::new();

    for shortcut in shortcuts {
        let value = shortcut.trim().to_uppercase();
        if value.is_empty() || normalized.iter().any(|existing| existing == &value) {
            continue;
        }
        normalized.push(value);
    }

    normalized
}

fn default_hotkeys() -> ProfileHotkeys {
    ProfileHotkeys {
        start: vec!["F5".to_string()],
        stop: vec!["F6".to_string()],
        pause: vec!["F7".to_string()],
        emergency_stop: vec!["F12".to_string()],
    }
}

fn profiles_dir() -> PathBuf {
    let mut path = local_data_dir();
    path.push("wingman");
    path.push("profiles");
    path
}

fn active_profile_path() -> PathBuf {
    let mut path = profiles_dir();
    path.pop();
    path.push("active_profile.txt");
    path
}

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
