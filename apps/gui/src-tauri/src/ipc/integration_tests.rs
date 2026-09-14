//! GUI↔Runtime 本地 IPC 跨语言集成测试（Linux UDS）。
//!
//! 每个用例 spawn 真 C++ runtime 子进程（`wingman-runtime start --standalone`，
//! 经 `XDG_RUNTIME_DIR` 指向临时目录注入 socket 路径），用真实 [`IpcClient`]
//! 走 UDS 帧协议（u32 LE 长度 + JSON envelope）做端到端断言——补上全链路中
//! Rust 客户端帧读写这一唯一无跨端覆盖的环节。
//!
//! runtime 二进制缺失时打印 `SKIP:` 并跳过（CI Linux job 不构建 runtime，属预期；
//! 本地先 `cmake --build build-runtime --target wingman-runtime`，或设
//! `WINGMAN_RUNTIME_BIN` 指向已构建产物）。
//!
//! 协议契约见 docs/protocols.md ① Local IPC；响应恒为
//! `{"type":"response","id":<信封数字/payload内字符串>,"data":{...}}`：
//! handler 返回不含 `success` 时 data = `{success:true, result:{...}}`，
//! handler 自带 `success` 时无 `result` 键，未知方法为 `{success:false, error}`。

use super::client::{IpcClient, IPC_QUICK_TIMEOUT};
use serde_json::{json, Value};
use std::io::Read;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

/// 等待 runtime socket 可连接的总预算（子进程启动 + bind/listen）。
const CONNECT_DEADLINE: Duration = Duration::from_secs(15);
/// 轮询 events.drain 等待连接事件的窗口。
const EVENT_WAIT: Duration = Duration::from_secs(3);
/// 连接重试 / 事件轮询间隔。
const RETRY_INTERVAL: Duration = Duration::from_millis(200);

/// 测试专用临时目录（stdlib 无 tempdir，手动管理生命周期，同 script_files.rs TempRoot）。
struct TempDir(PathBuf);

impl TempDir {
    fn new() -> Self {
        static SEQ: AtomicU64 = AtomicU64::new(0);
        let unique = format!(
            "wingman_gui_ipc_test_{}_{}",
            std::process::id(),
            SEQ.fetch_add(1, Ordering::Relaxed)
        );
        let path = std::env::temp_dir().join(unique);
        std::fs::create_dir_all(&path).expect("create temp dir");
        TempDir(path)
    }

    fn path(&self) -> &Path {
        &self.0
    }
}

impl Drop for TempDir {
    fn drop(&mut self) {
        // socket 文件随目录一并清理；runtime 启动本身也会 unlink 陈旧 socket
        let _ = std::fs::remove_dir_all(&self.0);
    }
}

/// 一个真 runtime 子进程 + 其专属临时目录与 socket 路径。
struct RuntimeHandle {
    child: std::process::Child,
    socket_path: PathBuf,
    _temp: TempDir,
    /// 子进程合并的 stdout+stderr，启动失败时用于诊断输出。
    output: Arc<Mutex<String>>,
    reaped: bool,
}

/// 收集子进程输出到共享缓冲（spdlog 日志量小，逐块读取无死锁风险）。
fn spawn_output_drain<R: Read + Send + 'static>(mut reader: R, sink: Arc<Mutex<String>>) {
    std::thread::spawn(move || {
        let mut buf = [0u8; 4096];
        loop {
            match reader.read(&mut buf) {
                Ok(0) | Err(_) => break,
                Ok(n) => {
                    if let Ok(mut s) = sink.lock() {
                        s.push_str(&String::from_utf8_lossy(&buf[..n]));
                    }
                }
            }
        }
    });
}

impl RuntimeHandle {
    fn resolve_binary() -> Option<PathBuf> {
        if let Ok(path) = std::env::var("WINGMAN_RUNTIME_BIN") {
            let p = PathBuf::from(path);
            if p.is_file() {
                return Some(p);
            }
        }
        // 默认回退仓库根的 build-runtime 产物（src-tauri 上三级即仓库根）
        let p = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("../../../build-runtime/apps/runtime/wingman-runtime");
        p.is_file().then_some(p)
    }

    /// spawn `wingman-runtime start --standalone`；XDG_RUNTIME_DIR 仅注入子进程，
    /// 不改测试进程全局 env（Rust 客户端直接传 socket 绝对路径，不依赖 env）。
    fn spawn() -> Result<Self, String> {
        let binary = Self::resolve_binary().ok_or_else(|| {
            "runtime binary not found (build: cmake --build build-runtime \
             --target wingman-runtime, or set WINGMAN_RUNTIME_BIN)"
                .to_string()
        })?;
        let temp = TempDir::new();
        let socket_path = temp.path().join("wingman.sock");

        let mut child = std::process::Command::new(&binary)
            .args(["start", "--standalone"])
            .env("XDG_RUNTIME_DIR", temp.path())
            .stdout(std::process::Stdio::piped())
            .stderr(std::process::Stdio::piped())
            .spawn()
            .map_err(|e| format!("failed to spawn {}: {}", binary.display(), e))?;

        let output = Arc::new(Mutex::new(String::new()));
        if let Some(out) = child.stdout.take() {
            spawn_output_drain(out, Arc::clone(&output));
        }
        if let Some(err) = child.stderr.take() {
            spawn_output_drain(err, Arc::clone(&output));
        }

        Ok(RuntimeHandle {
            child,
            socket_path,
            _temp: temp,
            output,
            reaped: false,
        })
    }

    fn output_snapshot(&self) -> String {
        self.output.lock().map(|s| s.clone()).unwrap_or_default()
    }

    /// 等待就绪并返回已连接客户端。就绪等待直接复用真实 IpcClient 的重试连接——
    /// server 是单客户端（accept 后即关 listen fd），任何探测连接都会占掉唯一槽位
    /// 触发断开-重听循环。二进制在但启动失败属被测对象问题，报错（fail）而非跳过。
    async fn connect_client(&mut self) -> Result<IpcClient, String> {
        let deadline = Instant::now() + CONNECT_DEADLINE;
        let mut last_err = String::new();
        while Instant::now() < deadline {
            if let Ok(Some(status)) = self.child.try_wait() {
                return Err(format!(
                    "runtime exited early (status: {}):\n{}",
                    status,
                    self.output_snapshot()
                ));
            }
            let mut client = IpcClient::new(self.socket_path.to_string_lossy().into_owned());
            match client.connect().await {
                Ok(()) => return Ok(client),
                Err(e) => {
                    last_err = e;
                    tokio::time::sleep(RETRY_INTERVAL).await;
                }
            }
        }
        Err(format!(
            "runtime socket not connectable within {:?} (last error: {}):\n{}",
            CONNECT_DEADLINE,
            last_err,
            self.output_snapshot()
        ))
    }

    /// SIGKILL + 收尸（幂等）。不为此引 libc 发 SIGTERM；socket 文件由 TempDir 兜底。
    fn terminate(&mut self) {
        if self.reaped {
            return;
        }
        self.reaped = true;
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

impl Drop for RuntimeHandle {
    fn drop(&mut self) {
        self.terminate();
    }
}

/// 断言 dispatcher 响应 `data.success == true`，返回 `data` 对象
/// （`result` 键可能缺失——handler 自带 success 的响应如 trigger.remove）。
fn expect_ok<'a>(payload: &'a Value, what: &str) -> &'a Value {
    let data = payload
        .get("data")
        .unwrap_or_else(|| panic!("{what}: response missing data: {payload}"));
    assert_eq!(
        data.get("success").and_then(Value::as_bool),
        Some(true),
        "{what}: expected success, got: {payload}"
    );
    data
}

fn result_of<'a>(data: &'a Value, what: &str) -> &'a Value {
    data.get("result")
        .unwrap_or_else(|| panic!("{what}: response missing result: {data}"))
}

/// 逐事件断言：method 匹配且 payload.state == "connected"。
fn is_client_connected_event(event: &Value) -> bool {
    event.get("method").and_then(Value::as_str) == Some("connection.ipc_client")
        && event
            .get("payload")
            .and_then(|p| p.get("state"))
            .and_then(Value::as_str)
            == Some("connected")
}

// ---------------------------------------------------------------------------
// 用例：每个测试独立子进程 + 独立临时目录（单客户端约束下互不干扰，可并行）。
// ---------------------------------------------------------------------------

#[tokio::test]
async fn status_roundtrip_reports_runtime_fields() {
    let mut rt = match RuntimeHandle::spawn() {
        Ok(rt) => rt,
        Err(e) => {
            eprintln!("SKIP: {e}");
            return;
        }
    };
    let mut client = rt.connect_client().await.expect("connect to runtime");

    let payload = client
        .send("system.getStatus", json!({}))
        .await
        .expect("system.getStatus round-trip over UDS");

    let result = result_of(expect_ok(&payload, "system.getStatus"), "system.getStatus");
    assert_eq!(
        result.get("server").and_then(Value::as_str),
        Some("wingman")
    );
    assert!(result
        .get("version")
        .and_then(Value::as_str)
        .is_some_and(|v| !v.is_empty()));
    assert!(result.get("uptime").is_some());
    // --standalone 强制关闭远程链路
    assert_eq!(
        result.get("remoteConnected").and_then(Value::as_bool),
        Some(false)
    );
    assert_eq!(
        result.get("remoteState").and_then(Value::as_str),
        Some("disabled")
    );
    // 本测试客户端已连接（runtime 视角的诊断标志）
    assert_eq!(
        result.get("ipcClientConnected").and_then(Value::as_bool),
        Some(true)
    );

    rt.terminate();
}

#[tokio::test]
async fn connection_event_reachable_via_events_drain() {
    let mut rt = match RuntimeHandle::spawn() {
        Ok(rt) => rt,
        Err(e) => {
            eprintln!("SKIP: {e}");
            return;
        }
    };
    let mut client = rt.connect_client().await.expect("connect to runtime");

    // connection.ipc_client 在 listen 成功后即刻入队，早于 accept；
    // poll-drain 兜底时序窗口，不做首 drain 硬断言。
    let deadline = Instant::now() + EVENT_WAIT;
    let mut drained: Vec<Value> = Vec::new();
    let found = loop {
        let payload = client
            .send_with_timeout("events.drain", json!({}), IPC_QUICK_TIMEOUT)
            .await
            .expect("events.drain");
        let result = result_of(expect_ok(&payload, "events.drain"), "events.drain");
        let events = result
            .get("events")
            .and_then(Value::as_array)
            .expect("events array")
            .clone();
        let hit = events.iter().any(is_client_connected_event);
        drained.extend(events);
        if hit {
            break true;
        }
        if Instant::now() >= deadline {
            break false;
        }
        tokio::time::sleep(RETRY_INTERVAL).await;
    };
    assert!(
        found,
        "expected connection.ipc_client connected event, drained: {drained:?}"
    );

    // drain 清空缓冲：随后 remaining 归零、二次 drain 无事件、dropped 计数在场
    let payload = client
        .send_with_timeout("events.drain", json!({}), IPC_QUICK_TIMEOUT)
        .await
        .expect("second events.drain");
    let result = result_of(expect_ok(&payload, "second events.drain"), "second drain").clone();
    assert_eq!(
        result.get("remaining").and_then(Value::as_u64),
        Some(0),
        "buffer should be empty after drain: {result}"
    );
    assert_eq!(
        result.get("events").and_then(Value::as_array).map(Vec::len),
        Some(0)
    );
    assert!(result.get("dropped").and_then(Value::as_u64).is_some());

    rt.terminate();
}

#[tokio::test]
async fn unknown_method_returns_error_envelope() {
    let mut rt = match RuntimeHandle::spawn() {
        Ok(rt) => rt,
        Err(e) => {
            eprintln!("SKIP: {e}");
            return;
        }
    };
    let mut client = rt.connect_client().await.expect("connect to runtime");

    let payload = client
        .send("definitely.not.a.method", json!({}))
        .await
        .expect("unknown method must still return an error envelope");

    let data = payload
        .get("data")
        .unwrap_or_else(|| panic!("response missing data: {payload}"));
    assert_eq!(data.get("success").and_then(Value::as_bool), Some(false));
    let error = data
        .get("error")
        .and_then(Value::as_str)
        .expect("error text");
    assert!(
        error.contains("Unknown method: definitely.not.a.method"),
        "unexpected error text: {error}"
    );
    // 错误响应不破坏链路：客户端仍在线且后续请求正常
    assert!(client.connected);
    let payload = client
        .send("system.getStatus", json!({}))
        .await
        .expect("getStatus after unknown-method error");
    assert_eq!(
        result_of(expect_ok(&payload, "getStatus after error"), "status")
            .get("server")
            .and_then(Value::as_str),
        Some("wingman")
    );

    rt.terminate();
}

#[tokio::test]
async fn trigger_add_list_remove_roundtrip() {
    let mut rt = match RuntimeHandle::spawn() {
        Ok(rt) => rt,
        Err(e) => {
            eprintln!("SKIP: {e}");
            return;
        }
    };
    let mut client = rt.connect_client().await.expect("connect to runtime");

    // TimeElapsed 条件 + Log 动作：零平台依赖（无 Lua / 无 X 也可走通）
    let config = json!({
        "name": "ipc-e2e-trigger",
        "condition": { "type": "TimeElapsed", "interval": 60000 },
        "actions": [ { "type": "Log", "value": "ping" } ]
    });

    let payload = client
        .send("trigger.add", json!({ "config": config }))
        .await
        .expect("trigger.add");
    let added_id = result_of(expect_ok(&payload, "trigger.add"), "trigger.add")
        .get("id")
        .and_then(Value::as_str)
        .expect("string trigger id")
        .to_string();

    let payload = client
        .send("trigger.list", json!({}))
        .await
        .expect("trigger.list");
    let triggers = result_of(expect_ok(&payload, "trigger.list"), "trigger.list")
        .get("triggers")
        .and_then(Value::as_array)
        .expect("triggers array")
        .clone();
    let listed = triggers
        .iter()
        .find(|t| t.get("id").and_then(Value::as_str) == Some(added_id.as_str()))
        .unwrap_or_else(|| panic!("added trigger {added_id} not listed back: {triggers:?}"));
    assert_eq!(
        listed.get("name").and_then(Value::as_str),
        Some("ipc-e2e-trigger")
    );

    // trigger.remove 的 handler 自带 success：响应无 result 键
    let payload = client
        .send("trigger.remove", json!({ "id": added_id }))
        .await
        .expect("trigger.remove");
    let data = expect_ok(&payload, "trigger.remove");
    assert!(
        data.get("result").is_none(),
        "remove response should carry no result: {payload}"
    );

    let payload = client
        .send("trigger.list", json!({}))
        .await
        .expect("trigger.list after remove");
    let triggers = result_of(
        expect_ok(&payload, "trigger.list after remove"),
        "trigger.list",
    )
    .get("triggers")
    .and_then(Value::as_array)
    .expect("triggers array");
    assert!(
        triggers.is_empty(),
        "expected empty list, got: {triggers:?}"
    );

    rt.terminate();
}

#[tokio::test]
async fn runtime_death_breaks_subsequent_requests() {
    let mut rt = match RuntimeHandle::spawn() {
        Ok(rt) => rt,
        Err(e) => {
            eprintln!("SKIP: {e}");
            return;
        }
    };
    let mut client = rt.connect_client().await.expect("connect to runtime");

    let payload = client
        .send("system.getStatus", json!({}))
        .await
        .expect("getStatus before kill");
    expect_ok(&payload, "getStatus before kill");

    // 杀掉 runtime：后续请求写侧 EPIPE / 读侧 EOF（Rust std 忽略 SIGPIPE，不会崩测试进程）
    rt.terminate();

    let outcome = client.send("system.getStatus", json!({})).await;
    assert!(
        outcome.is_err(),
        "request after runtime death must fail, got: {outcome:?}"
    );
    assert!(
        !client.connected,
        "client must mark itself disconnected after IO failure"
    );
}
