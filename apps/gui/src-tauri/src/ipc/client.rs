use serde_json::Value;
use std::io::{Read, Write};
use std::time::{SystemTime, UNIX_EPOCH};

/// Maximum allowed IPC response payload size (10 MiB).
const MAX_RESPONSE_SIZE: usize = 10 * 1024 * 1024;

#[cfg(windows)]
use std::fs::{File, OpenOptions};

#[cfg(unix)]
use std::os::unix::net::UnixStream;

enum IpcStream {
    #[cfg(windows)]
    NamedPipe(File),
    #[cfg(unix)]
    UnixSocket(UnixStream),
}

impl IpcStream {
    fn write_all(&mut self, data: &[u8]) -> Result<(), String> {
        match self {
            #[cfg(windows)]
            IpcStream::NamedPipe(file) => file.write_all(data).map_err(|e| e.to_string()),
            #[cfg(unix)]
            IpcStream::UnixSocket(stream) => stream.write_all(data).map_err(|e| e.to_string()),
        }
    }

    fn read_exact(&mut self, data: &mut [u8]) -> Result<(), String> {
        match self {
            #[cfg(windows)]
            IpcStream::NamedPipe(file) => file.read_exact(data).map_err(|e| e.to_string()),
            #[cfg(unix)]
            IpcStream::UnixSocket(stream) => stream.read_exact(data).map_err(|e| e.to_string()),
        }
    }
}

pub struct IpcClient {
    pub endpoint: String,
    pub connected: bool,
    stream: Option<IpcStream>,
    next_id: u64,
}

impl IpcClient {
    pub fn new(endpoint: String) -> Self {
        Self {
            endpoint,
            connected: false,
            stream: None,
            next_id: 1,
        }
    }

    pub async fn connect(&mut self) -> Result<(), String> {
        let endpoint = if self.endpoint.trim().is_empty() || self.endpoint.trim() == "wingman" {
            default_endpoint()
        } else {
            self.endpoint.clone()
        };

        self.stream = Some(connect_stream(&endpoint)?);
        self.endpoint = endpoint;
        self.connected = true;
        Ok(())
    }

    pub async fn disconnect(&mut self) {
        self.stream = None;
        self.connected = false;
    }

    pub async fn send(&mut self, method: &str, params: Value) -> Result<Value, String> {
        if !self.connected {
            return Err("IPC client is not connected".to_string());
        }

        let id = self.next_id;
        self.next_id += 1;

        let request = serde_json::json!({
            "type": 0,
            "method": method,
            "payload": params,
            "id": id,
            "timestamp": now_millis(),
        });

        let body = serde_json::to_vec(&request).map_err(|e| e.to_string())?;
        let length = (body.len() as u32).to_le_bytes();

        let stream = self
            .stream
            .as_mut()
            .ok_or_else(|| "IPC stream is not connected".to_string())?;
        stream.write_all(&length)?;
        stream.write_all(&body)?;

        let mut length_buf = [0u8; 4];
        stream.read_exact(&mut length_buf)?;
        let response_len = u32::from_le_bytes(length_buf) as usize;
        if response_len == 0 || response_len > MAX_RESPONSE_SIZE {
            return Err(format!("Invalid IPC response length: {}", response_len));
        }

        let mut response_buf = vec![0u8; response_len];
        stream.read_exact(&mut response_buf)?;

        let envelope: Value = serde_json::from_slice(&response_buf).map_err(|e| e.to_string())?;
        let payload = envelope
            .get("payload")
            .cloned()
            .ok_or_else(|| "IPC response missing payload".to_string())?;

        Ok(payload)
    }
}

#[cfg(windows)]
fn connect_stream(endpoint: &str) -> Result<IpcStream, String> {
    let pipe_path = if endpoint.starts_with(r"\\.\pipe\") {
        endpoint.to_string()
    } else {
        format!(r"\\.\pipe\{}", endpoint)
    };

    let file = OpenOptions::new()
        .read(true)
        .write(true)
        .open(&pipe_path)
        .map_err(|e| format!("Failed to connect named pipe {}: {}", pipe_path, e))?;

    Ok(IpcStream::NamedPipe(file))
}

#[cfg(unix)]
fn connect_stream(endpoint: &str) -> Result<IpcStream, String> {
    UnixStream::connect(endpoint)
        .map(IpcStream::UnixSocket)
        .map_err(|e| format!("Failed to connect Unix socket {}: {}", endpoint, e))
}

fn default_endpoint() -> String {
    #[cfg(windows)]
    {
        "wingman".to_string()
    }

    #[cfg(target_os = "linux")]
    {
        std::env::var("XDG_RUNTIME_DIR")
            .map(|dir| format!("{}/wingman.sock", dir))
            .unwrap_or_else(|_| "/tmp/wingman.sock".to_string())
    }

    #[cfg(target_os = "macos")]
    {
        std::env::var("TMPDIR")
            .map(|dir| format!("{}wingman.sock", dir))
            .unwrap_or_else(|_| "/tmp/wingman.sock".to_string())
    }
}

fn now_millis() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_millis() as u64)
        .unwrap_or_default()
}
