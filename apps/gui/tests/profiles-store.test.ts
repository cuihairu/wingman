import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

type ProfilesModule = Awaited<typeof import('$lib/stores/profiles')>;
type GameProfile = ProfilesModule['GameProfile'];

const INVOKE_KEY = '__TAURI_INVOKE__';

async function fresh(): Promise<ProfilesModule> {
	vi.resetModules();
	return import('$lib/stores/profiles');
}

describe('profiles store（dev 模式，无 __TAURI_INVOKE__）', () => {
	beforeEach(() => {
		delete (window as any)[INVOKE_KEY];
	});

	it('create 新建配置：补默认热键，首个配置自动激活，后续不改变激活项', async () => {
		const { profiles } = await fresh();
		const id1 = await profiles.create('配置 A');
		expect(id1).toMatch(/^profile_\d+_[0-9a-z]+$/);
		let list = get(profiles);
		expect(list).toHaveLength(1);
		expect(list[0]).toMatchObject({
			name: '配置 A',
			version: '1.0',
			hotkeys: { start: ['F5'], stop: ['F6'], pause: ['F7'], emergencyStop: ['F12'] },
		});
		expect(get(profiles.activeId)).toBe(id1);

		const id2 = await profiles.create('配置 B');
		expect(get(profiles.activeId)).toBe(id1);
		expect(id2).not.toBe(id1);
	});

	it('remove 删除配置；删除激活项时切换到剩余第一个', async () => {
		const { profiles } = await fresh();
		const id1 = await profiles.create('A');
		const id2 = await profiles.create('B');
		await profiles.remove(id1);
		expect(get(profiles).map(p => p.id)).toEqual([id2]);
		expect(get(profiles.activeId)).toBe(id2);

		await profiles.remove(id2);
		expect(get(profiles)).toHaveLength(0);
		expect(get(profiles.activeId)).toBe('');
	});

	it('remove 未激活项不影响激活状态', async () => {
		const { profiles } = await fresh();
		const id1 = await profiles.create('A');
		const id2 = await profiles.create('B');
		await profiles.remove(id2);
		expect(get(profiles.activeId)).toBe(id1);
	});

	it('update 按 id 覆盖并补默认热键', async () => {
		const { profiles } = await fresh();
		const id = await profiles.create('A');
		await profiles.update({
			id,
			name: 'A2',
			version: '2.0',
			description: 'd',
			window: { title: 't', className: '', processName: '', exactMatch: false, fullscreen: false },
			colors: [],
			images: [],
			triggers: [],
			scripts: [],
			hotkeys: undefined as any,
			settings: {},
		});
		const item = get(profiles)[0];
		expect(item.name).toBe('A2');
		expect(item.hotkeys.start).toEqual(['F5']);
	});

	it('setActive 切换激活配置', async () => {
		const { profiles } = await fresh();
		const id1 = await profiles.create('A');
		const id2 = await profiles.create('B');
		expect(get(profiles.activeId)).toBe(id1);
		await profiles.setActive(id2);
		expect(get(profiles.activeId)).toBe(id2);
	});

	it('exportToJson 序列化指定配置，未找到返回空串', async () => {
		const { profiles } = await fresh();
		const id = await profiles.create('A');
		const json = await profiles.exportToJson(id);
		expect(JSON.parse(json).name).toBe('A');
		await expect(profiles.exportToJson('ghost')).resolves.toBe('');
	});

	it('importFromJson：新增、按 id 替换、非法 JSON 返回 false', async () => {
		const { profiles } = await fresh();
		const ok = await profiles.importFromJson(JSON.stringify({
			id: 'p1', name: '导入配置', version: '1.0', description: '',
			window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
			colors: [], images: [], triggers: [], scripts: [], settings: {},
		}));
		expect(ok).toBe(true);
		expect(get(profiles)).toHaveLength(1);

		const replaced = await profiles.importFromJson(JSON.stringify({
			id: 'p1', name: '覆盖配置', version: '1.1', description: '',
			window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
			colors: [], images: [], triggers: [], scripts: [], settings: {},
		}));
		expect(replaced).toBe(true);
		expect(get(profiles)).toHaveLength(1);
		expect(get(profiles)[0].name).toBe('覆盖配置');

		await expect(profiles.importFromJson('{bad json')).resolves.toBe(false);
	});

	it('loadDevData 填充两个示例配置并激活第一个', async () => {
		const { profiles, activeProfile } = await fresh();
		profiles.loadDevData();
		expect(get(profiles).map(p => p.id)).toEqual(['default', 'game1']);
		expect(get(activeProfile)?.name).toBe('默认配置');
	});

	it('load/loadActive 在 dev 模式下为 no-op', async () => {
		const { profiles } = await fresh();
		await expect(profiles.load()).resolves.toBeUndefined();
		await expect(profiles.loadActive()).resolves.toBeUndefined();
		expect(get(profiles)).toHaveLength(0);
	});

	it('activeProfile derived：激活项存在返回配置，否则 null', async () => {
		const { profiles, activeProfile } = await fresh();
		expect(get(activeProfile)).toBeNull();
		const id = await profiles.create('A');
		expect(get(activeProfile)?.id).toBe(id);
		await profiles.remove(id);
		expect(get(activeProfile)).toBeNull();
	});
});

describe('profiles store（invoke 模式）', () => {
	let invokeMock: ReturnType<typeof vi.fn>;

	beforeEach(async () => {
		invokeMock = vi.fn();
		(window as any)[INVOKE_KEY] = invokeMock;
	});

	afterEach(() => {
		delete (window as any)[INVOKE_KEY];
	});

	it('load 拉取配置并补默认热键；失败静默', async () => {
		const { profiles } = await fresh();
		invokeMock.mockResolvedValueOnce([{ id: 'p1', name: 'P1' }]);
		await profiles.load();
		expect(invokeMock).toHaveBeenCalledWith('get_profiles');
		expect(get(profiles)[0].hotkeys.emergencyStop).toEqual(['F12']);

		invokeMock.mockRejectedValue(new Error('down'));
		await expect(profiles.load()).resolves.toBeUndefined();
	});

	it('loadActive 读取激活配置 id；无 id / 失败保持现状', async () => {
		const { profiles } = await fresh();
		invokeMock.mockResolvedValueOnce({ id: 'p9' });
		await profiles.loadActive();
		expect(get(profiles.activeId)).toBe('p9');

		invokeMock.mockResolvedValueOnce({});
		await profiles.loadActive();
		expect(get(profiles.activeId)).toBe('p9');

		invokeMock.mockRejectedValue(new Error('down'));
		await expect(profiles.loadActive()).resolves.toBeUndefined();
	});

	it('setActive 调用后端成功才更新本地', async () => {
		const { profiles } = await fresh();
		invokeMock.mockResolvedValueOnce(null);
		await profiles.setActive('p1');
		expect(invokeMock).toHaveBeenCalledWith('set_active_profile', { id: 'p1' });
		expect(get(profiles.activeId)).toBe('p1');

		invokeMock.mockRejectedValueOnce(new Error('denied'));
		await profiles.setActive('p2');
		expect(get(profiles.activeId)).toBe('p1');
	});

	it('create 调用后端并重新 load；失败返回空串', async () => {
		const { profiles } = await fresh();
		invokeMock.mockImplementation(async (cmd: string) => {
			if (cmd === 'create_profile') return 'new-id';
			if (cmd === 'get_profiles') return [{ id: 'new-id', name: 'N' }];
			return null;
		});
		await expect(profiles.create('N')).resolves.toBe('new-id');
		expect(invokeMock).toHaveBeenCalledWith('create_profile', { name: 'N' });
		expect(get(profiles)).toHaveLength(1);

		invokeMock.mockRejectedValue(new Error('fail'));
		await expect(profiles.create('X')).resolves.toBe('');
	});

	it('remove/update 调用后端并刷新；失败静默', async () => {
		const { profiles } = await fresh();
		invokeMock.mockImplementation(async (cmd: string) => {
			if (cmd === 'get_profiles') return [{ id: 'p1', name: 'P1' }];
			return null;
		});
		await profiles.load();
		await profiles.remove('p1');
		expect(invokeMock).toHaveBeenCalledWith('delete_profile', { id: 'p1' });
		expect(get(profiles)).toHaveLength(1); // mock 未真正删除

		const profile = get(profiles)[0];
		await profiles.update({ ...profile, name: 'P1x' } as GameProfile);
		expect(invokeMock).toHaveBeenCalledWith('update_profile', { profile: { ...profile, name: 'P1x' } });

		invokeMock.mockRejectedValueOnce(new Error('down'));
		await expect(profiles.remove('p1')).resolves.toBeUndefined();
	});

	it('exportToJson 走后端；失败返回空串', async () => {
		const { profiles } = await fresh();
		invokeMock.mockResolvedValueOnce('{"id":"p1"}');
		await expect(profiles.exportToJson('p1')).resolves.toBe('{"id":"p1"}');
		expect(invokeMock).toHaveBeenCalledWith('export_profile_json', { id: 'p1' });

		invokeMock.mockRejectedValueOnce(new Error('down'));
		await expect(profiles.exportToJson('p1')).resolves.toBe('');
	});

	it('importFromJson 走后端并刷新；失败返回 false', async () => {
		const { profiles } = await fresh();
		invokeMock.mockImplementation(async (cmd: string) => {
			if (cmd === 'import_profile_json') return null;
			if (cmd === 'get_profiles') return [{ id: 'imported', name: 'I' }];
			return null;
		});
		await expect(profiles.importFromJson('{"id":"imported"}')).resolves.toBe(true);
		expect(get(profiles)).toHaveLength(1);

		invokeMock.mockRejectedValueOnce(new Error('bad'));
		await expect(profiles.importFromJson('{}')).resolves.toBe(false);
	});
});
