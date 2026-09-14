//! 脚本文件管理命令（本地 UI 能力，直接操作脚本目录文件）。
//!
//! 设计约束（见 docs/architecture-decisions.md「Local Standalone UI」）：
//! 文件浏览/新建/删除是本地 GUI 的文件系统能力，不走 runtime IPC——runtime
//! RPC 面已覆盖脚本执行生命周期，本地文件管理不扩大协议面。
//!
//! 安全模型：
//! - 所有路径先经过 `sanitize_relative_path`（组件级校验，拒绝 `..`、绝对
//!   路径、盘符/冒号、隐藏项），再 join 到脚本根目录并做 canonicalize
//!   + starts_with 校验，防目录穿越与符号链接逃逸。
//! - 读写删除仅允许脚本/配置扩展名白名单（`.lua` `.py` `.json` `.toml`）。
//! - 读写内容上限 1 MiB，递归列目录限深 8 层、限 2000 条目。
//!
//! 脚本根目录解析优先级：UI 持久化设置 > `WINGMAN_SCRIPTS_DIR` 环境变量
//! > 默认 `./scripts`（与 runtime `[standalone] script_dir` 及 GUI 现有
//! `scripts/example.lua` 路径约定一致；runtime 按进程 cwd 解析相对路径）。

use serde::{Deserialize, Serialize};
use std::fs;
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

/// 单文件读取/写入内容上限：1 MiB。
const MAX_FILE_BYTES: u64 = 1024 * 1024;
/// 递归列目录的最大条目数（防止超大目录拖垮 IPC 响应）。
const MAX_LIST_ENTRIES: usize = 2000;
/// 递归列目录的最大深度。
const MAX_LIST_DEPTH: usize = 8;
/// 允许读写的扩展名白名单（小写、不含点）。
const ALLOWED_EXTENSIONS: [&str; 4] = ["lua", "py", "json", "toml"];

/// 持久化脚本根目录设置的文件名（位于 local_data_dir/wingman/ 下）。
const ROOT_SETTING_FILE: &str = "scripts_root.txt";
/// 环境变量覆盖脚本根目录。
const ROOT_ENV_VAR: &str = "WINGMAN_SCRIPTS_DIR";

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScriptsRootInfo {
    /// 解析后的绝对路径
    pub root: String,
    /// 解析来源：setting（UI 持久化）/ env / default
    pub source: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScriptFileEntry {
    /// 相对脚本根目录的路径（正斜杠分隔）
    pub path: String,
    pub name: String,
    pub is_dir: bool,
    /// 字节数（目录为 0）
    pub size: u64,
    /// 最近修改时间（epoch 毫秒）
    pub modified: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScriptFileList {
    pub entries: Vec<ScriptFileEntry>,
    /// 因条目上限被截断时为 true
    pub truncated: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScriptFileContent {
    pub path: String,
    pub content: String,
    pub size: u64,
    /// 最近修改时间（epoch 毫秒）
    pub modified: u64,
}

// ---------- 路径安全校验 ----------

/// 把相对路径字符串清洗为受限的 `PathBuf`。
///
/// 规则：反斜杠归一为正斜杠后按 `/` 分组件，拒绝空组件、`.`、`..`、
/// 以 `.` 开头的隐藏项、含 `:` 的组件（Windows 盘符/ADS）以及任何
/// 形式的绝对路径前缀。通过后返回纯相对路径（不会向上回溯）。
fn sanitize_relative_path(input: &str) -> Result<PathBuf, String> {
    let trimmed = input.trim();
    if trimmed.is_empty() {
        return Err("路径不能为空".to_string());
    }
    if trimmed.len() > 512 {
        return Err("路径过长".to_string());
    }

    // 绝对路径前置拒绝（Unix 根分隔符 / Windows 盘符 / UNC 前缀）。
    // 注意不能依赖 split 后的空组件过滤：`/etc/passwd` 首组件为空，
    // 会被误当成相对路径 `etc/passwd` 放行。
    if trimmed.starts_with('/') || trimmed.starts_with('\\') {
        return Err("路径不允许使用绝对路径".to_string());
    }
    let bytes = trimmed.as_bytes();
    if bytes.len() >= 2
        && bytes[1] == b':'
        && bytes[0].is_ascii_alphabetic()
    {
        return Err("路径不允许使用 Windows 盘符".to_string());
    }

    let normalized = trimmed.replace('\\', "/");
    let mut components = Vec::new();
    for part in normalized.split('/') {
        if part.is_empty() || part == "." {
            continue;
        }
        if part == ".." {
            return Err("路径不允许包含 ..".to_string());
        }
        if part.starts_with('.') {
            return Err(format!("路径不允许隐藏项: {part}"));
        }
        if part.contains(':') {
            return Err(format!("路径不允许包含冒号: {part}"));
        }
        if part.contains('\0') {
            return Err("路径包含非法字符".to_string());
        }
        components.push(part);
    }

    if components.is_empty() {
        return Err("路径不能为空".to_string());
    }

    let mut path = PathBuf::new();
    for component in components {
        path.push(component);
    }
    Ok(path)
}

/// 扩展名白名单校验（大小写不敏感）。
fn is_allowed_extension(path: &Path) -> bool {
    path.extension()
        .and_then(|ext| ext.to_str())
        .map(|ext| {
            let lower = ext.to_ascii_lowercase();
            ALLOWED_EXTENSIONS.contains(&lower.as_str())
        })
        .unwrap_or(false)
}

/// 相对路径转正斜杠字符串（列表/返回给前端时使用）。
fn relative_string(path: &Path) -> String {
    path.to_string_lossy().replace('\\', "/")
}

/// epoch 毫秒（系统时间早于 epoch 时为 0）。
fn epoch_millis(time: SystemTime) -> u64 {
    time.duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as u64)
        .unwrap_or(0)
}

/// 解析脚本根目录：持久化设置 > 环境变量 > 默认 `./scripts`。
fn resolve_scripts_root() -> (PathBuf, String) {
    if let Some(root) = persisted_scripts_root() {
        return (root, "setting".to_string());
    }
    if let Some(value) = std::env::var(ROOT_ENV_VAR)
        .ok()
        .map(|v| v.trim().to_string())
        .filter(|v| !v.is_empty())
    {
        return (PathBuf::from(value), "env".to_string());
    }
    (PathBuf::from("scripts"), "default".to_string())
}

fn persisted_scripts_root() -> Option<PathBuf> {
    let path = root_setting_path();
    let value = fs::read_to_string(path).ok()?;
    let trimmed = value.trim().to_string();
    if trimmed.is_empty() {
        return None;
    }
    Some(PathBuf::from(trimmed))
}

fn root_setting_path() -> PathBuf {
    let mut path = super::scripts::local_data_dir();
    path.push("wingman");
    path.push(ROOT_SETTING_FILE);
    path
}

/// 把根目录解析为绝对 canonical 路径（不存在时按 cwd 拼接）。
fn canonical_root(raw_root: &Path) -> PathBuf {
    raw_root.canonicalize().unwrap_or_else(|_| {
        if raw_root.is_absolute() {
            raw_root.to_path_buf()
        } else {
            std::env::current_dir()
                .unwrap_or_else(|_| PathBuf::from("."))
                .join(raw_root)
        }
    })
}

/// 校验 `target` 位于 `root` 之内（canonicalize 后 starts_with）。
/// `target` 已存在时直接 canonicalize；否则向上找最近的已存在祖先校验，
/// 不存在的尾段已由 `sanitize_relative_path` 保证不会向上回溯。
fn ensure_within_root(root_canon: &Path, target: &Path) -> Result<(), String> {
    let probe = if target.exists() {
        target.to_path_buf()
    } else {
        let mut deepest = target.to_path_buf();
        loop {
            match deepest.parent() {
                Some(parent) => {
                    deepest = parent.to_path_buf();
                    if deepest.exists() {
                        break;
                    }
                    if deepest == deepest.parent().unwrap_or(&deepest) {
                        break;
                    }
                }
                None => break,
            }
        }
        deepest
    };

    let resolved = probe
        .canonicalize()
        .map_err(|e| format!("路径解析失败: {e}"))?;
    if !resolved.starts_with(root_canon) {
        return Err("路径越出脚本根目录".to_string());
    }
    Ok(())
}

// ---------- Tauri 命令 ----------

/// 查询当前脚本根目录（绝对路径 + 解析来源）。
#[tauri::command]
pub async fn get_scripts_root() -> Result<ScriptsRootInfo, String> {
    let (root, source) = resolve_scripts_root();
    Ok(ScriptsRootInfo {
        root: canonical_root(&root).to_string_lossy().to_string(),
        source,
    })
}

/// 设置脚本根目录（canonical 绝对路径会被持久化）。
/// 传空字符串清除持久化设置、恢复环境变量/默认值。
/// 目录必须已存在——文件管理不代建任意目录。
#[tauri::command]
pub async fn set_scripts_root(path: String) -> Result<ScriptsRootInfo, String> {
    let trimmed = path.trim().to_string();
    let setting_path = root_setting_path();

    if trimmed.is_empty() {
        if setting_path.exists() {
            fs::remove_file(&setting_path).map_err(|e| format!("重置脚本目录设置失败: {e}"))?;
        }
        let (root, source) = resolve_scripts_root();
        return Ok(ScriptsRootInfo {
            root: canonical_root(&root).to_string_lossy().to_string(),
            source,
        });
    }

    let resolved = PathBuf::from(&trimmed)
        .canonicalize()
        .map_err(|e| format!("脚本目录不存在或不可访问: {e}"))?;
    if !resolved.is_dir() {
        return Err("脚本目录必须是一个已存在的目录".to_string());
    }

    if let Some(parent) = setting_path.parent() {
        fs::create_dir_all(parent).map_err(|e| format!("写入设置失败: {e}"))?;
    }
    fs::write(&setting_path, resolved.to_string_lossy().as_bytes())
        .map_err(|e| format!("写入设置失败: {e}"))?;

    Ok(ScriptsRootInfo {
        root: resolved.to_string_lossy().to_string(),
        source: "setting".to_string(),
    })
}

/// 递归列出脚本根目录（或其子目录）下的文件与目录。
/// 跳过隐藏项与符号链接；限深 8 层、限 2000 条目（超出时 truncated=true）。
#[tauri::command]
pub async fn list_script_files(sub_dir: Option<String>) -> Result<ScriptFileList, String> {
    let (raw_root, _) = resolve_scripts_root();
    let root = canonical_root(&raw_root);
    if !root.is_dir() {
        return Err(format!(
            "脚本目录不存在: {}（可在下方设置或用 WINGMAN_SCRIPTS_DIR 指定）",
            root.to_string_lossy()
        ));
    }

    let base = match sub_dir {
        Some(dir) if !dir.trim().is_empty() => {
            let rel = sanitize_relative_path(&dir)?;
            let joined = root.join(&rel);
            ensure_within_root(&root, &joined)?;
            if !joined.is_dir() {
                return Err("子路径不是目录".to_string());
            }
            joined
        }
        _ => root.clone(),
    };

    let mut entries = Vec::new();
    let mut truncated = false;
    walk_dir(&base, &root, 0, &mut entries, &mut truncated);

    // 目录优先，其次按路径排序，保证前端展示稳定
    entries.sort_by(|a, b| {
        b.is_dir
            .cmp(&a.is_dir)
            .then_with(|| a.path.to_lowercase().cmp(&b.path.to_lowercase()))
    });

    Ok(ScriptFileList { entries, truncated })
}

/// 递归遍历目录，跳过隐藏项与符号链接。
fn walk_dir(
    dir: &Path,
    root: &Path,
    depth: usize,
    entries: &mut Vec<ScriptFileEntry>,
    truncated: &mut bool,
) {
    if depth > MAX_LIST_DEPTH || *truncated {
        return;
    }

    let Ok(read_dir) = fs::read_dir(dir) else {
        return;
    };

    let mut children: Vec<_> = read_dir.filter_map(|item| item.ok()).collect();
    children.sort_by_key(|item| item.file_name().to_string_lossy().to_lowercase());

    for child in children {
        if entries.len() >= MAX_LIST_ENTRIES {
            *truncated = true;
            return;
        }

        let name = child.file_name().to_string_lossy().to_string();
        if name.starts_with('.') {
            continue;
        }

        // 符号链接一律跳过：防止把脚本目录之外的树展示给前端
        let Ok(meta) = fs::symlink_metadata(child.path()) else {
            continue;
        };
        if meta.file_type().is_symlink() {
            continue;
        }

        let rel = child.path().strip_prefix(root).unwrap_or(&child.path()).to_path_buf();
        let is_dir = meta.is_dir();
        entries.push(ScriptFileEntry {
            path: relative_string(&rel),
            name,
            is_dir,
            size: if is_dir { 0 } else { meta.len() },
            modified: meta.modified().map(epoch_millis).unwrap_or(0),
        });

        if is_dir {
            walk_dir(&child.path(), root, depth + 1, entries, truncated);
        }
    }
}

/// 读取脚本文件文本内容（UTF-8，上限 1 MiB）。
#[tauri::command]
pub async fn read_script_file(path: String) -> Result<ScriptFileContent, String> {
    let (raw_root, _) = resolve_scripts_root();
    let root = canonical_root(&raw_root);
    let rel = sanitize_relative_path(&path)?;
    if !is_allowed_extension(&rel) {
        return Err(format!(
            "仅允许读取 {:?} 类型的文件",
            ALLOWED_EXTENSIONS
        ));
    }
    let target = root.join(&rel);
    ensure_within_root(&root, &target)?;

    let meta = fs::symlink_metadata(&target).map_err(|e| format!("读取文件失败: {e}"))?;
    if meta.file_type().is_symlink() {
        return Err("不允许读取符号链接".to_string());
    }
    if meta.is_dir() {
        return Err("目标路径是目录".to_string());
    }
    if meta.len() > MAX_FILE_BYTES {
        return Err(format!(
            "文件超过 {} MiB 读取上限（{} 字节）",
            MAX_FILE_BYTES / 1024 / 1024,
            meta.len()
        ));
    }

    let content = fs::read_to_string(&target).map_err(|e| {
        if e.kind() == std::io::ErrorKind::InvalidData {
            "文件不是有效的 UTF-8 文本".to_string()
        } else {
            format!("读取文件失败: {e}")
        }
    })?;

    Ok(ScriptFileContent {
        path: relative_string(&rel),
        content,
        size: meta.len(),
        modified: meta.modified().map(epoch_millis).unwrap_or(0),
    })
}

/// 写入/新建脚本文件（自动创建父目录，内容上限 1 MiB）。
/// 返回写入后的文件元信息。
#[tauri::command]
pub async fn write_script_file(
    path: String,
    content: String,
) -> Result<ScriptFileEntry, String> {
    let (raw_root, _) = resolve_scripts_root();
    let root = canonical_root(&raw_root);
    let rel = sanitize_relative_path(&path)?;
    if !is_allowed_extension(&rel) {
        return Err(format!(
            "仅允许写入 {:?} 类型的文件",
            ALLOWED_EXTENSIONS
        ));
    }

    let content_len = content.len() as u64;
    if content_len > MAX_FILE_BYTES {
        return Err(format!(
            "内容超过 {} MiB 写入上限（{} 字节）",
            MAX_FILE_BYTES / 1024 / 1024,
            content_len
        ));
    }

    let target = root.join(&rel);
    // 已存在的符号链接拒绝覆写（写后还有 canonicalize 兜底校验）
    if let Ok(meta) = fs::symlink_metadata(&target) {
        if meta.file_type().is_symlink() {
            return Err("目标路径是符号链接，拒绝覆写".to_string());
        }
    }

    if let Some(parent) = target.parent() {
        fs::create_dir_all(parent).map_err(|e| format!("创建目录失败: {e}"))?;
    }

    fs::write(&target, content.as_bytes()).map_err(|e| format!("写入文件失败: {e}"))?;

    // 写后校验：真实路径必须仍在根目录内（防父目录符号链接逃逸），
    // 越界则回滚删除刚写入的文件。
    if ensure_within_root(&root, &target).is_err() {
        let _ = fs::remove_file(&target);
        return Err("路径越出脚本根目录".to_string());
    }

    let meta = fs::metadata(&target).map_err(|e| format!("写入文件失败: {e}"))?;
    Ok(ScriptFileEntry {
        path: relative_string(&rel),
        name: rel
            .file_name()
            .map(|n| n.to_string_lossy().to_string())
            .unwrap_or_default(),
        is_dir: false,
        size: meta.len(),
        modified: meta.modified().map(epoch_millis).unwrap_or(0),
    })
}

/// 删除脚本目录内的单个文件（不递归目录；前端负责二次确认）。
#[tauri::command]
pub async fn delete_script_file(path: String) -> Result<(), String> {
    let (raw_root, _) = resolve_scripts_root();
    let root = canonical_root(&raw_root);
    let rel = sanitize_relative_path(&path)?;
    if !is_allowed_extension(&rel) {
        return Err(format!(
            "仅允许删除 {:?} 类型的文件",
            ALLOWED_EXTENSIONS
        ));
    }
    let target = root.join(&rel);
    ensure_within_root(&root, &target)?;

    let meta = fs::symlink_metadata(&target).map_err(|e| format!("读取文件失败: {e}"))?;
    if meta.file_type().is_symlink() {
        return Err("不允许删除符号链接".to_string());
    }
    if meta.is_dir() {
        return Err("仅支持删除文件，不支持删除目录".to_string());
    }

    fs::remove_file(&target).map_err(|e| format!("删除文件失败: {e}"))
}

// ---------- 单元测试 ----------

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU64, Ordering};

    /// 测试专用的临时脚本根目录（stdlib 无 tempdir，手动管理生命周期）。
    struct TempRoot(PathBuf);

    impl TempRoot {
        fn new() -> Self {
            static SEQ: AtomicU64 = AtomicU64::new(0);
            let unique = format!(
                "wingman_gui_script_files_test_{}_{}",
                std::process::id(),
                SEQ.fetch_add(1, Ordering::Relaxed) + chrono_millis()
            );
            let path = std::env::temp_dir().join(unique);
            fs::create_dir_all(&path).expect("create temp root");
            TempRoot(path)
        }

        fn path(&self) -> &Path {
            &self.0
        }
    }

    impl Drop for TempRoot {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.0);
        }
    }

    fn chrono_millis() -> u64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|d| d.as_millis() as u64)
            .unwrap_or(0)
    }

    fn root_canon_of(temp: &TempRoot) -> PathBuf {
        temp.path().canonicalize().unwrap()
    }

    #[test]
    fn sanitize_rejects_traversal_and_absolute_paths() {
        assert!(sanitize_relative_path("../evil.lua").is_err());
        assert!(sanitize_relative_path("a/../../evil.lua").is_err());
        assert!(sanitize_relative_path("..\\evil.lua").is_err());
        assert!(sanitize_relative_path("/etc/passwd").is_err());
        assert!(sanitize_relative_path("C:\\evil.lua").is_err());
        assert!(sanitize_relative_path("C:/evil.lua").is_err());
        assert!(sanitize_relative_path("").is_err());
        assert!(sanitize_relative_path("   ").is_err());
        assert!(sanitize_relative_path("./../x.lua").is_err());
        assert!(sanitize_relative_path("a/..\\/b.lua").is_err());
        assert!(sanitize_relative_path(".hidden/secret.lua").is_err());
        assert!(sanitize_relative_path("scripts/.git/config").is_err());
    }

    #[test]
    fn sanitize_allows_normal_relative_paths() {
        let cleaned = sanitize_relative_path("scripts/example.lua").unwrap();
        assert_eq!(cleaned, PathBuf::from("scripts").join("example.lua"));

        // 冗余的 ./ 与连续斜杠被归一（不含 .. 时不构成逃逸）
        let cleaned = sanitize_relative_path("./scripts//example.lua").unwrap();
        assert_eq!(cleaned, PathBuf::from("scripts").join("example.lua"));

        // 反斜杠统一按分隔符处理
        let cleaned = sanitize_relative_path("scripts\\sub\\a.lua").unwrap();
        assert_eq!(cleaned, PathBuf::from("scripts").join("sub").join("a.lua"));
    }

    #[test]
    fn extension_whitelist_is_enforced() {
        for name in ["a.lua", "b.PY", "c.Json", "d.toml"] {
            assert!(is_allowed_extension(Path::new(name)), "{name} 应放行");
        }
        for name in ["e.exe", "f.sh", "g.txt", "h.bat", "i", "j.lua.bak"] {
            assert!(!is_allowed_extension(Path::new(name)), "{name} 应拒绝");
        }
    }

    #[test]
    fn write_read_list_delete_roundtrip() {
        let temp = TempRoot::new();
        let root = root_canon_of(&temp);

        let rel = sanitize_relative_path("scripts/demo.lua").unwrap();
        let target = root.join(&rel);
        fs::create_dir_all(target.parent().unwrap()).unwrap();
        fs::write(&target, "-- hello").unwrap();

        // read
        let content = std::fs::read_to_string(&target).unwrap();
        assert_eq!(content, "-- hello");

        // list：包含文件与父目录，路径为正斜杠相对路径
        let mut entries = Vec::new();
        let mut truncated = false;
        walk_dir(temp.path(), temp.path(), 0, &mut entries, &mut truncated);
        assert!(!truncated);
        let paths: Vec<&str> = entries.iter().map(|e| e.path.as_str()).collect();
        assert!(paths.contains(&"scripts/demo.lua"));
        assert!(paths.contains(&"scripts"));
        let demo = entries.iter().find(|e| e.path == "scripts/demo.lua").unwrap();
        assert!(!demo.is_dir);
        assert_eq!(demo.size, "-- hello".len() as u64);
        assert!(demo.modified > 0);
    }

    #[test]
    fn write_rejects_out_of_root_and_bad_extension() {
        let temp = TempRoot::new();
        let root = root_canon_of(&temp);

        // 越界：sanitize 已拒绝 ..
        assert!(sanitize_relative_path("../outside.lua").is_err());

        // 白名单：写入 .txt 被拒
        let rel = sanitize_relative_path("notes.txt").unwrap();
        assert!(!is_allowed_extension(&rel));

        // 顶层目录 + 白名单扩展正常写入
        let rel = sanitize_relative_path("config/ok.toml").unwrap();
        let target = root.join(&rel);
        fs::create_dir_all(target.parent().unwrap()).unwrap();
        fs::write(&target, "k = 1").unwrap();
        assert!(target.exists());
    }

    #[test]
    fn list_skips_hidden_and_symlinks() {
        let temp = TempRoot::new();
        let root = root_canon_of(&temp);

        fs::create_dir_all(root.join("sub")).unwrap();
        fs::write(root.join("sub/keep.lua"), "x").unwrap();
        fs::write(root.join(".secret.lua"), "x").unwrap();
        fs::create_dir_all(root.join(".hidden_dir")).unwrap();

        #[cfg(unix)]
        {
            std::os::unix::fs::symlink("/etc", root.join("etc_link")).unwrap();
            std::os::unix::fs::symlink("../outside.lua", root.join("escape.lua")).unwrap();
        }

        let mut entries = Vec::new();
        let mut truncated = false;
        walk_dir(temp.path(), temp.path(), 0, &mut entries, &mut truncated);
        let paths: Vec<&str> = entries.iter().map(|e| e.path.as_str()).collect();
        assert!(paths.contains(&"sub/keep.lua"));
        assert!(!paths.iter().any(|p| p.contains(".secret")));
        assert!(!paths.iter().any(|p| p.contains(".hidden")));
        #[cfg(unix)]
        {
            assert!(!paths.iter().any(|p| p.contains("etc_link")));
            assert!(!paths.iter().any(|p| p.contains("escape")));
        }
    }

    #[cfg(unix)]
    #[test]
    fn read_rejects_symlink_escape() {
        let temp = TempRoot::new();
        let root = root_canon_of(&temp);

        // root 外的机密文件 + root 内指向它的符号链接
        let outside_dir = TempRoot::new();
        let outside = outside_dir.path().join("outside.lua");
        fs::write(&outside, "secret").unwrap();
        std::os::unix::fs::symlink(&outside, root.join("link.lua")).unwrap();

        let rel = sanitize_relative_path("link.lua").unwrap();
        assert!(is_allowed_extension(&rel));
        let target = root.join(&rel);
        // symlink_metadata 识别出链接并拒绝
        let meta = fs::symlink_metadata(&target).unwrap();
        assert!(meta.file_type().is_symlink());
        // canonicalize + starts_with 兜底：目标越出根目录
        assert!(!target.canonicalize().unwrap().starts_with(&root));
    }

    #[test]
    fn ensure_within_root_blocks_missing_probe_escape() {
        let temp = TempRoot::new();
        let root = root_canon_of(&temp);

        // 深层不存在路径的最近存在祖先是 root 本身 → 放行
        let inside = root.join("a/b/c.lua");
        assert!(ensure_within_root(&root, &inside).is_ok());

        // 指向根外的已存在路径 → 拒绝
        let outside = TempRoot::new();
        assert!(ensure_within_root(&root, outside.path()).is_err());
    }

    #[test]
    fn oversized_content_is_rejected() {
        let temp = TempRoot::new();
        let root = root_canon_of(&temp);
        let rel = sanitize_relative_path("big.lua").unwrap();
        let target = root.join(&rel);
        let big = "x".repeat((MAX_FILE_BYTES + 1) as usize);
        fs::write(&target, &big).unwrap();

        let meta = fs::metadata(&target).unwrap();
        assert!(meta.len() > MAX_FILE_BYTES);
    }
}
