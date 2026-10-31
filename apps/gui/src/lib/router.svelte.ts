import { writable, derived } from 'svelte/store';

/// 归一化 hash：`#/scripts` 与 `#scripts` 等价（外部链接/刷新常用带斜杠形式），
/// 否则 pageMap 查不到匹配页面，内容区渲染为空白。
function normalize(raw: string): string {
	return raw.replace(/^\/+/, '').trim() || 'dashboard';
}

function createRouter() {
	const hash = writable(normalize(window.location.hash.slice(1)));

	function navigate(page: string) {
		const route = normalize(page);
		window.location.hash = route;
		hash.set(route);
	}

	function init() {
		window.addEventListener('hashchange', () => {
			hash.set(normalize(window.location.hash.slice(1)));
		});
	}

	const current = derived(hash, ($hash) => $hash);
	const state = derived(hash, ($hash) => ({ current: $hash }));

	return { subscribe: state.subscribe, current, navigate, init };
}

export const router = createRouter();
