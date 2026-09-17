import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

/**
 * store 分支第四组：triggers 序列化/标记回退、connection 端点常量兜底、
 * screen throw undefined 的默认错误文案、script-files 空条目与尾斜杠命名、
 * scripts 缺 id 条目、profiles 双条目导入覆盖。
 */

async function freshInvoke(handler: (cmd: string, args?: Record<string, unknown>) => unknown) {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	(window as any).__TAURI_INVOKE__ = vi.fn(handler);
	return {
		triggers: await import('$lib/stores/triggers'),
		connection: await import('$lib/stores/connection'),
		settings: await import('$lib/stores/settings'),
		screen: await import('$lib/stores/screen'),
		scriptFiles: await import('$lib/stores/script-files'),
		scripts: await import('$lib/stores/scripts'),
		profiles: await import('$lib/stores/profiles'),
	};
}

function entriesOf(scriptFiles: any): Array<{ path: string; name: string }> {
	const snap: Array<{ path: string; name: string }> = [];
	(scriptFiles.scriptFiles.subscribe as any).entries((list: Array<{ path: string; name: string }>) => {
		snap.length = 0;
		snap.push(...list);
	})();
	return snap;
}

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
});

afterEach(() => {
	vi.restoreAllMocks();
	delete (window as any).__TAURI_INVOKE__;
});

describe('triggers store：分支回退', () => {
	it('serializeForRuntime 的 pixel_changed 无 region 时展开空对象', async () => {
		const { triggers } = await freshInvoke(() => ({}));
		const out = triggers.serializeForRuntime({
			id: 'p1',
			name: 'p',
			enabled: true,
			condition: { type: 'pixel_changed', value: '5,6' } as any,
			actions: [],
		});
		expect(out.condition.region).toMatchObject({ x: 5, y: 6 });
	});

	it('normalizeTrigger 的 region 有 x 无 y 时 y 回退 0', async () => {
		const { triggers } = await freshInvoke((cmd) => {
			if (cmd === 'get_triggers') {
				return [{ id: 'r1', name: 'r', enabled: true, trigger_type: 'pixel_changed', condition: { value: 'ignored', region: { x: 5 } } }];
			}
			return {};
		});
		const store = triggers.triggers;
		await store.load();
		expect(get(store)[0].condition.value).toBe('5,0');
	});

	it('get_triggers 返回 null 时列表为空', async () => {
		const { triggers } = await freshInvoke((cmd) => {
			if (cmd === 'get_triggers') return null;
			return {};
		});
		const store = triggers.triggers;
		await store.load();
		expect(get(store)).toHaveLength(0);
	});

	it('markFired 缺 id 时按空串匹配不命中', async () => {
		const { triggers } = await freshInvoke(() => ({}));
		const store = triggers.triggers;
		await store.set([{ id: 't1', name: '回血', enabled: true, condition: { type: 'color_found', value: '#ff0000' } as any, actions: [] } as any]);
		store.markFired(undefined as any, '不存在的名字');
		expect(get(store)[0].last_triggered).toBeFalsy();
	});
});

describe('connection store：端点常量兜底', () => {
	it('settings.ipcEndpoint 为空串时 connect 使用默认 wingman', async () => {
		const endpoints: unknown[] = [];
		const { connection, settings } = await freshInvoke((cmd, args) => {
			if (cmd === 'connect_ipc') {
				endpoints.push(args?.endpoint);
				return {};
			}
			if (cmd === 'get_system_status') return { server: 'wingman', version: '1.0', uptime: 0, running_scripts: 0, paused: false };
			if (cmd === 'get_ipc_state') return { connected: false, endpoint: '' };
			return {};
		});
		settings.settings.update({ ipcEndpoint: '' });
		await connection.connection.connect();
		expect(endpoints).toContain('wingman');
	});
});

describe('screen store：throw undefined 的默认文案', () => {
	it('capture 抛出 undefined 时错误文案为默认值', async () => {
		const { screen } = await freshInvoke((cmd) => {
			if (cmd === 'capture_screenshot') return Promise.reject(undefined);
			if (cmd === 'list_monitors') return [];
			return {};
		});
		await screen.screen.capture().catch(() => {});
		expect(get(screen.screen).error).toBe('截图失败');
	});

	it('listMonitors 抛出 undefined 时错误文案为默认值', async () => {
		const { screen } = await freshInvoke((cmd) => {
			if (cmd === 'capture_screenshot') return { image: '', width: 1, height: 1, region: { x: 0, y: 0, width: 1, height: 1 }, timestamp: 0 };
			if (cmd === 'list_monitors') return Promise.reject(undefined);
			return {};
		});
		await screen.screen.listMonitors().catch(() => {});
		expect(get(screen.screen).monitorsError).toBe('无法枚举显示器');
	});
});

describe('script-files store：空条目与尾斜杠', () => {
	it('list_script_files 含 null/空对象条目时回退空字符串', async () => {
		const { scriptFiles } = await freshInvoke((cmd) => {
			if (cmd === 'list_script_files') return { entries: [null, {}] };
			return {};
		});
		await scriptFiles.scriptFiles.load();
		const list = entriesOf(scriptFiles);
		expect(list).toHaveLength(2);
		expect(list.every(e => e.path === '' && e.name === '')).toBe(true);
	});

	it('dev 新建根路径 "/" 时条目名回退整段路径', async () => {
		vi.resetModules();
		delete (window as any).__TAURI_INVOKE__;
		const scriptFiles = await import('$lib/stores/script-files');
		await scriptFiles.scriptFiles.create('/');
		const list = entriesOf(scriptFiles);
		expect(list.some(e => e.path === '/' && e.name === '/')).toBe(true);
	});
});

describe('scripts store：缺 id 条目', () => {
	it('get_scripts 条目缺 id 时回退空串', async () => {
		const { scripts } = await freshInvoke((cmd) => {
			if (cmd === 'get_scripts') return [{ name: 'anon.lua', path: 'anon.lua', state: 'idle' }];
			return {};
		});
		await scripts.scripts.load();
		expect(get(scripts.scripts)[0].id).toBe('');
	});
});

describe('profiles store：双条目导入覆盖', () => {
	it('导入已存在配置时其他条目保持原样', async () => {
		// importFromJson 只有 dev（无 invoke）分支直接写 store
		vi.resetModules();
		delete (window as any).__TAURI_INVOKE__;
		const { profiles } = await import('$lib/stores/profiles');
		const mk = (id: string, name: string) => JSON.stringify({
			id, name, description: '', version: '1.0',
			window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
			colors: [], images: [], triggers: [], scripts: [],
			hotkeys: { start: ['F5'], stop: ['F6'], pause: ['F7'], emergencyStop: ['F12'] },
			settings: {},
		});
		const store = profiles;
		await store.importFromJson(mk('a', '甲'));
		await store.importFromJson(mk('b', '乙'));
		await store.importFromJson(mk('a', '甲改'));
		const list = get(store);
		expect(list.map(p => p.id)).toEqual(['a', 'b']);
		expect(list.find(p => p.id === 'a')?.name).toBe('甲改');
		expect(list.find(p => p.id === 'b')?.name).toBe('乙');
	});
});
