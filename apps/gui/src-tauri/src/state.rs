use crate::ws::client::WebSocketClient;
use std::sync::Arc;
use std::time::SystemTime;
use tokio::sync::Mutex;

pub struct AppState {
    pub ws_client: Arc<Mutex<WebSocketClient>>,
    pub paused: Arc<Mutex<bool>>,
    pub start_time: Arc<Mutex<Option<SystemTime>>>,
}

impl AppState {
    pub fn new(ws_url: &str) -> Self {
        Self {
            ws_client: Arc::new(Mutex::new(WebSocketClient::new(ws_url.to_string()))),
            paused: Arc::new(Mutex::new(false)),
            start_time: Arc::new(Mutex::new(Some(SystemTime::now()))),
        }
    }
}
