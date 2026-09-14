pub mod client;

/// GUI↔Runtime 本地 IPC 跨语言集成测试（spawn 真 C++ runtime 子进程，POSIX 专属）。
#[cfg(all(test, unix))]
mod integration_tests;
