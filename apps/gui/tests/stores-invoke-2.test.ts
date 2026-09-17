import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

/**
 * stores invoke 分支第二组：connection 端点缺省链与返回值回退、
 * profiles/scripts 列表空响应、macros eventCount 缺省、events 无级别日志。
 */

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
});

afterEach(() => {
	vi.restoreAllMocks();
	delete (window as any).__TAURI_INVOKE__;
});

async function freshInvoke(handler: (cmd: string, args?: Record<string, unknown>) => unknown) {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	(window as any).__TAURI_INVOKE__ = vi.fn(handler);
	const mods = {
		connection: await import('$lib/stores/connection'),
		settings: await import('$lib/stores/settings'),
		profiles: await import('$lib/stores/profiles'),
		scripts: await import('$lib/stores/scripts'),
		macros: await import('$lib/stores/macros'),
	};
	return mods;
}

describe('connection invoke 分支回退', () => {
	it('connect 无参时使用 settings.ipcEndpoint', async () => {
		const endpoints: unknown[] = [];
		const { connection, settings } = await freshInvoke((cmd, args) => {
			if (cmd === 'connect_ipc') {
				endpoints.push(args?.endpoint);
				return {};
			}
			if (cmd === 'get_system_status') return { server: 'wingman', version: '1.0', uptime: 0, running_scripts: 0, paused: false };
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'custom.sock' };
			return {};
		});
		settings.settings.update({ ipcEndpoint: 'custom.sock' });
		await connection.connection.connect();
		expect(endpoints).toContain('custom.sock');
	});

	it('stop_active_profile 返回空值时计 0', async () => {
		const { connection } = await freshInvoke((cmd) => {
			if (cmd === 'stop_active_profile_scripts') return undefined;
			return {};
		});
		const stopped = await connection.connection.stopActiveProfile();
		expect(stopped).toBe(0);
	});

	it('get_system_status 的 version 为空串时保持原版本', async () => {
		const { connection } = await freshInvoke((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_system_status') {
				return { server: 'wingman', version: '', uptime: 0, running_scripts: 0, paused: false };
			}
			if (cmd === 'get_ipc_state') return { connected: false, endpoint: '' };
			return {};
		});
		await connection.connection.connect();
		const before = get(connection.connection).version;
		const status = await connection.connection.refresh();
		expect(status?.version).toBe('');
		expect(get(connection.connection).version).toBe(before);
	});
});

describe('profiles/scripts invoke 列表空响应', () => {
	it('get_profiles 返回 null 时列表为空', async () => {
		const { profiles } = await freshInvoke((cmd) => {
			if (cmd === 'get_profiles') return null;
			return {};
		});
		await profiles.profiles.load();
		expect(get(profiles.profiles)).toHaveLength(0);
	});

	it('get_scripts 返回 null 时列表为空', async () => {
		const { scripts } = await freshInvoke((cmd) => {
			if (cmd === 'get_scripts') return null;
			return {};
		});
		await scripts.scripts.load();
		expect(get(scripts.scripts)).toHaveLength(0);
	});
});

describe('macros eventCount 缺省', () => {
	it('macro_load 响应缺 eventCount 时计 0', async () => {
		const { macros } = await freshInvoke((cmd) => {
			if (cmd === 'macro_load') return {};
			return {};
		});
		const ok = await macros.macros.load('m.lua');
		expect(ok).toBe(true);
		expect(get(macros.macros).eventCount).toBe(0);
	});
});

describe('events 无级别日志回退 info', () => {
	it('log.line 事件缺 level 字段按 info 记录', async () => {
		await freshInvoke((cmd) => {
			if (cmd === 'drain_events') {
				return { events: [{ method: 'log.line', payload: { message: '无级别日志' }, timestamp: Date.now() }] };
			}
			return { events: [] };
		});
		const eventsMod = await import('$lib/stores/events');
		const { logs } = await import('$lib/stores/logs');
		const poller = eventsMod.createEventPoller();
		poller.start();
		await new Promise(r => setTimeout(r, 100));
		poller.stop();
		const entry = get(logs).find(e => e.message === '无级别日志');
		expect(entry).toBeTruthy();
		expect(entry!.type).toBe('info');
	});
});
