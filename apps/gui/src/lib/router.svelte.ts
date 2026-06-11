import { writable, derived } from 'svelte/store';

function createRouter() {
	const hash = writable(window.location.hash.slice(1) || 'dashboard');

	function navigate(page: string) {
		window.location.hash = page;
		hash.set(page);
	}

	function init() {
		window.addEventListener('hashchange', () => {
			hash.set(window.location.hash.slice(1) || 'dashboard');
		});
	}

	const current = derived(hash, ($hash) => $hash);
	const state = derived(hash, ($hash) => ({ current: $hash }));

	return { subscribe: state.subscribe, current, navigate, init };
}

export const router = createRouter();
