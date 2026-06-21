import { writable } from 'svelte/store';

/// 主题 store：'dark'（默认）| 'light'。
/// 通过 document.documentElement.dataset.theme 切换 CSS 变量集（见 app.css），
/// 偏好持久化到 localStorage，初始化时同步应用。

export type Theme = 'dark' | 'light';

const STORAGE_KEY = 'wingman.theme';

function isTheme(v: unknown): v is Theme {
	return v === 'dark' || v === 'light';
}

function readStored(): Theme {
	try {
		const v = localStorage.getItem(STORAGE_KEY);
		if (isTheme(v)) return v;
	} catch { /* localStorage 不可用（如 Tauri webview 受限）时忽略 */ }
	return 'dark';
}

function applyTheme(theme: Theme) {
	if (typeof document !== 'undefined') {
		document.documentElement.dataset.theme = theme;
	}
}

const initial = readStored();
applyTheme(initial);

export const theme = writable<Theme>(initial);

export function setTheme(theme: Theme) {
	applyTheme(theme);
	try {
		localStorage.setItem(STORAGE_KEY, theme);
	} catch { /* 忽略持久化失败 */ }
}

export function toggleTheme() {
	const next: Theme = (readStored() === 'dark') ? 'light' : 'dark';
	theme.set(next);
	setTheme(next);
}
