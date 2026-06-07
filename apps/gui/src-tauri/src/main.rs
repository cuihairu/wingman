mod commands;
mod hotkeys;
mod ipc;
mod models;
mod state;

use state::AppState;

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_global_shortcut::Builder::new().build())
        .manage(AppState::new("wingman"))
        .setup(|app| {
            if let Err(error) = hotkeys::setup_hotkeys(&app.handle()) {
                eprintln!("failed to set up global hotkeys: {error}");
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            // 连接管理
            commands::connection::connect_ipc,
            commands::connection::disconnect_ipc,
            commands::connection::is_connected,
            // 脚本管理
            commands::scripts::get_scripts,
            commands::scripts::start_script,
            commands::scripts::stop_script,
            commands::scripts::start_active_profile_scripts,
            commands::scripts::stop_active_profile_scripts,
            // 系统状态
            commands::system::get_system_status,
            commands::system::get_version,
            commands::system::get_runtime_info,
            commands::system::toggle_pause,
            commands::system::pause_all,
            commands::system::resume_all,
            commands::system::stop_all,
            commands::system::is_paused,
            // 触发器管理
            commands::triggers::get_triggers,
            commands::triggers::add_trigger,
            commands::triggers::remove_trigger,
            commands::triggers::update_trigger,
            commands::triggers::toggle_trigger,
            // 配置管理
            commands::profiles::get_profiles,
            commands::profiles::get_active_profile,
            commands::profiles::set_active_profile,
            commands::profiles::create_profile,
            commands::profiles::delete_profile,
            commands::profiles::update_profile,
            commands::profiles::export_profile_json,
            commands::profiles::import_profile_json,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
