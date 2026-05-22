mod commands;
mod models;
mod state;
mod ws;

use state::AppState;

fn main() {
    tauri::Builder::default()
        .manage(AppState::new("ws://127.0.0.1:8080/ws"))
        .invoke_handler(tauri::generate_handler![
            // 连接管理
            commands::connection::connect_websocket,
            commands::connection::disconnect_websocket,
            commands::connection::is_connected,
            // 脚本管理
            commands::scripts::get_scripts,
            commands::scripts::start_script,
            commands::scripts::stop_script,
            // 系统状态
            commands::system::get_system_status,
            commands::system::get_version,
            commands::system::get_runtime_info,
            commands::system::toggle_pause,
            commands::system::pause_all,
            commands::system::resume_all,
            commands::system::is_paused,
            // 触发器管理
            commands::triggers::get_triggers,
            commands::triggers::add_trigger,
            commands::triggers::remove_trigger,
            commands::triggers::update_trigger,
            commands::triggers::toggle_trigger,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
