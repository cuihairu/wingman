#include "wingman/hotkey.hpp"
#include "wingman/platform/input_factory.hpp"
#include "wingman/platform/iinput.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <system_error>
#include <vector>

namespace wingman {

namespace {

std::string toUpperAscii(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
		return static_cast<char>(std::toupper(c));
	});
	return s;
}

// keyNameToCode 命名键名 → KeyCode；未知返回 false。
bool keyNameToCode(const std::string& upper, platform::KeyCode& out) {
	// 字母 A-Z：'A'..'Z'
	if (upper.size() == 1 && upper[0] >= 'A' && upper[0] <= 'Z') {
		out = static_cast<platform::KeyCode>(upper[0]);
		return true;
	}
	// 数字 0-9
	if (upper.size() == 1 && upper[0] >= '0' && upper[0] <= '9') {
		out = static_cast<platform::KeyCode>(
			static_cast<uint32_t>(platform::KeyCode::Num0) + (upper[0] - '0'));
		return true;
	}
	static const std::unordered_map<std::string, platform::KeyCode> kNamed = {
		{"F1", platform::KeyCode::F1}, {"F2", platform::KeyCode::F2},
		{"F3", platform::KeyCode::F3}, {"F4", platform::KeyCode::F4},
		{"F5", platform::KeyCode::F5}, {"F6", platform::KeyCode::F6},
		{"F7", platform::KeyCode::F7}, {"F8", platform::KeyCode::F8},
		{"F9", platform::KeyCode::F9}, {"F10", platform::KeyCode::F10},
		{"F11", platform::KeyCode::F11}, {"F12", platform::KeyCode::F12},
		{"SPACE", platform::KeyCode::Space},
		{"ENTER", platform::KeyCode::Enter}, {"RETURN", platform::KeyCode::Enter},
		{"ESC", platform::KeyCode::Escape}, {"ESCAPE", platform::KeyCode::Escape},
		{"TAB", platform::KeyCode::Tab},
		{"BACKSPACE", platform::KeyCode::Backspace},
		{"DELETE", platform::KeyCode::Delete}, {"DEL", platform::KeyCode::Delete},
		{"INSERT", platform::KeyCode::Insert}, {"INS", platform::KeyCode::Insert},
		{"HOME", platform::KeyCode::Home}, {"END", platform::KeyCode::End},
		{"PAGEUP", platform::KeyCode::PageUp}, {"PGUP", platform::KeyCode::PageUp},
		{"PAGEDOWN", platform::KeyCode::PageDown}, {"PGDN", platform::KeyCode::PageDown},
		{"LEFT", platform::KeyCode::Left}, {"UP", platform::KeyCode::Up},
		{"RIGHT", platform::KeyCode::Right}, {"DOWN", platform::KeyCode::Down},
		{"PRINTSCREEN", platform::KeyCode::PrintScreen},
		{"PAUSE", platform::KeyCode::Pause},
	};
	auto it = kNamed.find(upper);
	if (it != kNamed.end()) {
		out = it->second;
		return true;
	}
	return false;
}

bool comboDown(platform::IInput& input, const HotkeyCombo& combo) {
	if (combo.ctrl && !input.isKeyPressed(platform::KeyCode::Control)) return false;
	if (combo.shift && !input.isKeyPressed(platform::KeyCode::Shift)) return false;
	if (combo.alt && !input.isKeyPressed(platform::KeyCode::Alt)) return false;
	return input.isKeyPressed(combo.key);
}

} // namespace

// ========== 组合键解析 ==========

bool parseHotkeyCombo(const std::string& text, HotkeyCombo& out) {
	HotkeyCombo combo;
	bool hasKey = false;
	size_t start = 0;
	while (start <= text.size()) {
		const size_t end = text.find('+', start);
		const std::string token = toUpperAscii(
			text.substr(start, end == std::string::npos ? std::string::npos : end - start));
		if (token.empty()) {
			return false;
		}
		if (token == "CTRL" || token == "CONTROL") {
			if (combo.ctrl) return false;
			combo.ctrl = true;
		} else if (token == "SHIFT") {
			if (combo.shift) return false;
			combo.shift = true;
		} else if (token == "ALT") {
			if (combo.alt) return false;
			combo.alt = true;
		} else if (token == "WIN" || token == "META" || token == "SUPER") {
			// Win/Meta 修饰 v1 不支持（X11 需 Super 映射且语义因 WM 而异），
			// 显式拒绝避免静默无效注册
			return false;
		} else {
			// 主键：组合里只能有一个
			if (hasKey) return false;
			if (!keyNameToCode(token, combo.key)) return false;
			hasKey = true;
		}
		if (end == std::string::npos) break;
		start = end + 1;
	}
	if (!hasKey) return false;
	out = combo;
	return true;
}

// ========== HotkeyManager ==========

HotkeyManager& HotkeyManager::defaultInstance() {
	static HotkeyManager manager;
	return manager;
}

HotkeyManager::HotkeyManager(std::shared_ptr<platform::IInput> input)
	: input_(input ? std::move(input) : platform::defaultSharedInput()) {}

HotkeyManager::~HotkeyManager() {
	stop();
}

uint64_t HotkeyManager::registerHotkey(const HotkeyCombo& combo, std::function<void()> callback) {
	if (!callback) return 0;
	uint64_t id;
	{
		// ensureRunning 自行加锁，调用方不得持锁（非递归锁自死锁）
		std::lock_guard<std::mutex> lock(mutex_);
		id = nextId_++;
		entries_[id] = Entry{combo, std::move(callback), false};
	}
	ensureRunning();
	return id;
}

uint64_t HotkeyManager::registerHotkey(const std::string& comboText, std::function<void()> callback) {
	HotkeyCombo combo;
	if (!callback || !parseHotkeyCombo(comboText, combo)) return 0;
	return registerHotkey(combo, std::move(callback));
}

bool HotkeyManager::unregister(uint64_t id) {
	std::lock_guard<std::mutex> lock(mutex_);
	return entries_.erase(id) > 0;
}

size_t HotkeyManager::count() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return entries_.size();
}

void HotkeyManager::start(uint32_t pollIntervalMs) {
	if (pollIntervalMs > 0) {
		intervalMs_ = pollIntervalMs;
	}
	ensureRunning();
}

// ensureRunning 空转则启动轮询线程。启动判断在锁内、线程创建在锁外：
// 自查退出的 worker 已不再触碰 mutex_，锁外 join 立即返回；上一线程经
// stop() join 过时 joinable() 为假，直接跳过。
void HotkeyManager::ensureRunning() {
	bool needSpawn = false;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!running_) {
			running_ = true;
			needSpawn = true;
		}
	}
	if (!needSpawn) return;
	if (thread_.joinable()) {
		thread_.join();
	}
	try {
		thread_ = std::thread([this] { workerLoop(); });
	} catch (const std::system_error& e) {
		// 线程资源耗尽：登记无效化，保持 running_ 与线程事实一致
		spdlog::error("[hotkey] spawn worker failed: {}", e.what());
		std::lock_guard<std::mutex> lock(mutex_);
		running_ = false;
	}
}

void HotkeyManager::stop() {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		running_ = false;
	}
	if (thread_.joinable()) {
		thread_.join();
	}
}

bool HotkeyManager::isRunning() const {
	return running_.load();
}

// workerLoop 周期检查：显式停止或注销清空（自查退出并同步 running_）。
// 退出前不持锁，保证 ensureRunning/stop 的锁外 join 无死锁。
void HotkeyManager::workerLoop() {
	while (true) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (!running_ || entries_.empty()) {
				running_ = false;
				break;
			}
		}
		pollOnce();
		std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs_.load()));
	}
}

void HotkeyManager::pollOnce() {
	if (!input_) return;
	// 快照待触发回调，锁外触发（回调可再入 register/unregister）；
	// 触发前按 ID 复核条目仍在——快照与触发之间可能被注销
	struct Pending {
		uint64_t id;
		std::function<void()> callback;
	};
	std::vector<Pending> fire;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto& [id, entry] : entries_) {
			const bool down = comboDown(*input_, entry.combo);
			if (down && !entry.wasPressed) {
				fire.push_back({id, entry.callback});
			}
			entry.wasPressed = down;
		}
	}
	for (auto& f : fire) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (entries_.count(f.id) == 0) continue;
		}
		try {
			if (f.callback) f.callback();
		} catch (const std::exception& e) {
			spdlog::error("[hotkey] callback exception for id={}: {}", f.id, e.what());
		} catch (...) {
			spdlog::error("[hotkey] callback unknown exception for id={}", f.id);
		}
	}
}

} // namespace wingman
