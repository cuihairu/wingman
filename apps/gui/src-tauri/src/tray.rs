use tauri::{
    menu::{Menu, MenuItem},
    tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent},
    AppHandle, Manager, Runtime,
};

/// runtime 托盘意图 → 系统托盘控件（脚本 notify.trayShow/… 经 events.drain
/// 链路到达，见前端 events.ts 的 tray.* 分发）。
///
/// action: "show" / "hide" / "badge" / "tooltip"；badge/tooltip 的 text 为
/// null 时清除对应状态。show/hide 幂等。tauri v2 TrayIcon 没有角标 label
/// API（ nightly 37240534697 三腿 E0599 实证），"badge" 显式返回错误走
/// 前端失败日志，不再臆造 no-op 语义；错误统一返回给前端记日志。
#[tauri::command]
pub fn tray_control(app: AppHandle, action: String, text: Option<String>) -> Result<(), String> {
    let tray = app
        .tray_by_id("main-tray")
        .ok_or_else(|| "tray not initialized".to_string())?;
    let result = match action.as_str() {
        "show" => tray.set_visible(true),
        "hide" => tray.set_visible(false),
        "badge" => {
            let _ = text;
            return Err(
                "tray badge not supported: tauri v2 TrayIcon has no badge label API".to_string(),
            )
        }
        "tooltip" => tray.set_tooltip(text.as_deref()),
        other => return Err(format!("unknown tray action: {other}")),
    };
    result.map_err(|e| e.to_string())
}

/// 设置系统托盘：左键点击切换窗口显隐，右键菜单提供「显示/隐藏」「退出」。
///
/// 仅在 setup 阶段调用一次。窗口关闭行为（最小化到托盘 vs 退出）由前端
/// window-close 事件配合控制；此处只负责托盘交互。
pub fn setup_tray<R: Runtime>(app: &AppHandle<R>) -> tauri::Result<()> {
    let show_item = MenuItem::with_id(app, "tray_show", "显示/隐藏", true, None::<&str>)?;
    let quit_item = MenuItem::with_id(app, "tray_quit", "退出 Wingman", true, None::<&str>)?;
    let menu = Menu::with_items(app, &[&show_item, &quit_item])?;

    TrayIconBuilder::with_id("main-tray")
        .tooltip("Wingman")
        .icon(app.default_window_icon().cloned().unwrap())
        .menu(&menu)
        .show_menu_on_left_click(false)
        .on_menu_event(|app, event| match event.id.as_ref() {
            "tray_show" => {
                toggle_main_window(app);
            }
            "tray_quit" => {
                app.exit(0);
            }
            _ => {}
        })
        .on_tray_icon_event(|tray, event| {
            if let TrayIconEvent::Click {
                button: MouseButton::Left,
                button_state: MouseButtonState::Up,
                ..
            } = event
            {
                toggle_main_window(tray.app_handle());
            }
        })
        .build(app)?;

    Ok(())
}

fn toggle_main_window<R: Runtime>(app: &AppHandle<R>) {
    if let Some(window) = app.get_webview_window("main") {
        if window.is_visible().unwrap_or(false) {
            let _ = window.hide();
        } else {
            let _ = window.show();
            let _ = window.set_focus();
        }
    }
}
