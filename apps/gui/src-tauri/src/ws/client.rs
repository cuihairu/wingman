use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use tokio_tungstenite::{connect_async, tungstenite::protocol::Message};
use url::Url;

pub struct WebSocketClient {
    sender: Option<
        tokio_tungstenite::WebSocketStream<
            tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>,
        >,
    >,
    pub url: String,
    pub connected: bool,
}

impl WebSocketClient {
    pub fn new(url: String) -> Self {
        Self {
            sender: None,
            url,
            connected: false,
        }
    }

    pub async fn connect(&mut self) -> Result<(), String> {
        let url = Url::parse(&self.url).map_err(|e| e.to_string())?;
        let (ws_stream, _) = connect_async(url).await.map_err(|e| e.to_string())?;
        self.sender = Some(ws_stream);
        self.connected = true;
        Ok(())
    }

    pub async fn disconnect(&mut self) {
        self.sender = None;
        self.connected = false;
    }

    pub async fn send(&mut self, method: &str, params: Value) -> Result<Value, String> {
        if let Some(ws) = &mut self.sender {
            let request = json!({
                "type": "call",
                "id": uuid::Uuid::new_v4().to_string(),
                "method": method,
                "params": params
            });

            ws.send(Message::Text(request.to_string()))
                .await
                .map_err(|e| e.to_string())?;

            if let Some(msg) = ws.next().await {
                match msg {
                    Ok(Message::Text(text)) => {
                        let response: Value = serde_json::from_str(&text)
                            .map_err(|e| format!("Failed to parse response: {}", e))?;
                        return Ok(response);
                    }
                    Ok(_) => return Err("Unexpected message type".to_string()),
                    Err(e) => return Err(e.to_string()),
                }
            }
        }
        Err("Not connected".to_string())
    }
}
