import { writable } from 'svelte/store';

/// 脚本文件条目（与 Rust commands/script_files.rs 的 ScriptFileEntry 对齐）
export interface ScriptFileEntry {
	/** 相对脚本根目录的路径（正斜杠） */
	path: string;
	name: string;
	is_dir: boolean;
	size: number;
	/** 最近修改时间（epoch 毫秒） */
	modified: number;
}

export interface ScriptFileList {
	entries: ScriptFileEntry[];
	truncated: boolean;
}

export interface ScriptFileContent {
	path: string;
	content: string;
	size: number;
	modified: number;
}

export interface ScriptsRootInfo {
	root: string;
	/** setting（UI 持久化）/ env / default */
	source: string;
}

function normalizeEntry(input: any): ScriptFileEntry {
	return {
		path: String(input?.path || ''),
		name: String(input?.name || input?.path || ''),
		is_dir: Boolean(input?.is_dir),
		size: Number(input?.size || 0),
		modified: Number(input?.modified || 0),
	};
}

function normalizeRoot(input: any): ScriptsRootInfo {
	return {
		root: String(input?.root || ''),
		source: String(input?.source || 'default'),
	};
}

function createScriptFilesStore() {
	const entries = writable<ScriptFileEntry[]>([]);
	const truncated = writable(false);
	const root = writable<ScriptsRootInfo>({ root: '', source: 'default' });
	const selected = writable<string | null>(null);
	const loading = writable(false);
	const invoke = (window as any).__TAURI_INVOKE__;

	/// dev（浏览器）模式的演示条目：保证无 Tauri 时页面仍可预览交互。
	/// root 不在这里覆盖——root 归 loadRoot/setRoot 管理，load 只刷条目。
	function devData() {
		entries.set([
			{ path: 'config', name: 'config', is_dir: true, size: 0, modified: Date.now() - 86_400_000 },
			{ path: 'config/triggers.lua', name: 'triggers.lua', is_dir: false, size: 812, modified: Date.now() - 3_600_000 },
			{ path: 'scripts', name: 'scripts', is_dir: true, size: 0, modified: Date.now() - 7_200_000 },
			{ path: 'scripts/example.lua', name: 'example.lua', is_dir: false, size: 1024, modified: Date.now() - 7_200_000 },
			{ path: 'scripts/example.py', name: 'example.py', is_dir: false, size: 512, modified: Date.now() - 172_800_000 },
		]);
		truncated.set(false);
		loading.set(false);
	}

	/// dev 模式本地 upsert 条目（无真实文件系统）
	function devUpsert(path: string) {
		entries.update(items => {
			if (items.some(item => item.path === path)) return items;
			const name = path.split('/').filter(Boolean).pop() || path;
			return [...items, { path, name, is_dir: false, size: 0, modified: Date.now() }];
		});
	}

	/// 当前 root 值（同步读取快照）
	function currentRoot(): string {
		let value = '';
		root.subscribe(r => { value = r.root; })();
		return value;
	}

	return {
		/// 各分片订阅函数（复合形态）：scriptFiles.subscribe.entries / root / selected / loading / truncated
		subscribe: {
			entries: entries.subscribe,
			truncated: truncated.subscribe,
			root: root.subscribe,
			selected: selected.subscribe,
			loading: loading.subscribe,
		},
		/** 测试/开发直接注入条目 */
		set(list: ScriptFileEntry[], isTruncated = false) {
			entries.set(list.map(normalizeEntry));
			truncated.set(isTruncated);
		},
		setSelected(path: string | null) {
			selected.set(path);
		},
		currentRoot,
		/// 刷新脚本根目录信息；dev 模式填演示根目录
		async loadRoot() {
			if (!invoke) {
				if (!currentRoot()) {
					root.set({ root: '/dev/demo/scripts-root', source: 'default' });
				}
				return;
			}
			try {
				const info = await invoke('get_scripts_root');
				root.set(normalizeRoot(info));
			} catch { /* ignore */ }
		},
		/// 修改脚本根目录（传空恢复默认），随后重载列表并清空选中
		async setRoot(path: string) {
			if (!invoke) {
				root.set({ root: path || '/dev/demo/scripts-root', source: path ? 'setting' : 'default' });
				selected.set(null);
				await this.load();
				return;
			}
			const info = await invoke('set_scripts_root', { path });
			root.set(normalizeRoot(info));
			selected.set(null);
			await this.load();
		},
		/// 列出脚本文件（递归）；dev 模式填演示数据
		async load(subDir?: string) {
			if (!invoke) {
				devData();
				return;
			}
			loading.set(true);
			try {
				const list: ScriptFileList = await invoke('list_script_files', { subDir: subDir ?? null });
				entries.set((list?.entries || []).map(normalizeEntry));
				truncated.set(Boolean(list?.truncated));
			} finally {
				loading.set(false);
			}
		},
		async read(path: string): Promise<ScriptFileContent> {
			if (!invoke) {
				let items: ScriptFileEntry[] = [];
				entries.subscribe(state => { items = state; })();
				const match = items.find(item => item.path === path);
				if (!match || match.is_dir) throw new Error('文件不存在');
				return {
					path,
					content: `-- dev 预览: ${path}\n${match.name} (${match.size} B)\n`,
					size: match.size,
					modified: match.modified,
				};
			}
			return (await invoke('read_script_file', { path })) as ScriptFileContent;
		},
		/// 新建空文件
		async create(path: string) {
			if (!invoke) {
				devUpsert(path);
				return;
			}
			await invoke('write_script_file', { path, content: '' });
			await this.load();
		},
		async write(path: string, content: string) {
			if (!invoke) {
				devUpsert(path);
				return;
			}
			await invoke('write_script_file', { path, content });
			await this.load();
		},
		/// 删除后联动：删除的是选中项则清空选中
		async remove(path: string) {
			await invoke('delete_script_file', { path });
			selected.update(current => (current === path ? null : current));
			await this.load();
		},
		/// dev 模式本地删除（无 Tauri 时供页面冒烟），invoke 模式委托 remove
		async removeDev(path: string) {
			if (!invoke) {
				entries.update(items => items.filter(item => item.path !== path));
				selected.update(current => (current === path ? null : current));
				return;
			}
			await this.remove(path);
		},
	};
}

export const scriptFiles = createScriptFilesStore();
