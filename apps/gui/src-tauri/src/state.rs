use crate::ipc::client::IpcClient;
use std::sync::Arc;
use std::time::SystemTime;
use tokio::sync::Mutex;

pub struct AppState {
    pub ipc_client: Arc<Mutex<IpcClient>>,
    pub paused: Arc<Mutex<bool>>,
    pub start_time: Arc<Mutex<Option<SystemTime>>>,
}

impl AppState {
    pub fn new(ipc_endpoint: &str) -> Self {
        Self {
            ipc_client: Arc::new(Mutex::new(IpcClient::new(ipc_endpoint.to_string()))),
            paused: Arc::new(Mutex::new(false)),
            start_time: Arc::new(Mutex::new(None)),
        }
    }
}
