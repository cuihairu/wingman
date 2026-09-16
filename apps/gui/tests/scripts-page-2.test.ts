import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, within, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/scripts/+page.svelte';
import { router } from '$lib/router.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { scripts, type ScriptInfo } from '$lib/stores/scripts';
import { scriptFiles } from '$lib/stores/script-files';

/**
 * scripts 页第二组：操作失败/未连接/边界分支与快捷启动列表、根目录切换。
 * 冒烟与筛选见 scripts-page.test.ts，文件管理见 scripts-files-page.test.ts。
 * jsdom 的 fireEvent 不拦截 disabled 按钮，可直接点击验证未连接守卫分支。
 */

function makeScript(id: string, state: ScriptInfo['state'], extra: Partial<ScriptInfo> = {}): ScriptInfo {
	return {
		id,
		name: `${id}.lua`,
		path: `scripts/${id}.lua`,
		size: 1024,
		is_running: state === 'running',
		state,
		error: '',
		loaded_at: 0,
		...extra,
	};
}

function itemOf(name: string): HTMLElement {
	return screen.getByText(name).closest('.script-item') as HTMLElement;
}

function logMessages(): string[] {
	return get(logs).map(e => e.message);
}

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	scripts.set([]);
	logs.clear();
	// scriptFiles 为模块级单例，重置 root/entries 避免「更改目录」用例间残留
	await scriptFiles.setRoot('');
	scriptFiles.set([]);
	await connection.connect();
});

afterEach(() => {
	vi.restoreAllMocks();
});

describe('scripts 页：运行时长与快捷启动列表', () => {
	it('运行时长按档位格式化：分钟/小时/天/无效', async () => {
		const now = Date.now();
		scripts.set([
			makeScript('fresh', 'running', { loaded_at: now - 65_000 }),
			makeScript('hourly', 'running', { loaded_at: now - 7_200_000 }),
			makeScript('daily', 'running', { loaded_at: now - 90_000_000 }),
			makeScript('future', 'running', { loaded_at: now + 60_000 }),
		]);
		render(Page);

		const uptimeOf = (id: string) => within(itemOf(`${id}.lua`)).getByText(/^⏱ /).textContent;
		expect(uptimeOf('fresh')).toMatch(/^⏱ 1m \d+s$/);
		expect(uptimeOf('hourly')).toBe('⏱ 2h 0m');
		expect(uptimeOf('daily')).toBe('⏱ 1d 1h');
		// loaded_at 晚于当前 tick → formatDuration 负数 → '-'
		expect(uptimeOf('future')).toBe('⏱ -');
	});

	it('快捷启动列表渲染 lua/python 标签与 MB 尺寸，点击启动', async () => {
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('triggers.lua', { selector: '.file-name' })).toBeInTheDocument();
		});
		// 注入大文件验证 MB 档位
		scriptFiles.set([
			{ path: 'scripts/big.py', name: 'big.py', is_dir: false, size: 1.5 * 1024 * 1024, modified: Date.now() },
			{ path: 'scripts/mini.lua', name: 'mini.lua', is_dir: false, size: 512, modified: Date.now() },
			{ path: 'notes.txt', name: 'notes.txt', is_dir: false, size: 10, modified: Date.now() },
		]);

		await waitFor(() => {
			expect(screen.getByText('big.py', { selector: '.quick-card strong' })).toBeInTheDocument();
		});
		const pyCard = screen.getByText('big.py', { selector: '.quick-card strong' }).closest('.quick-card')!;
		expect(within(pyCard).getByText('python')).toBeInTheDocument();
		expect(within(pyCard).getByText('1.5 MB')).toBeInTheDocument();

		const luaCard = screen.getByText('mini.lua', { selector: '.quick-card strong' }).closest('.quick-card')!;
		expect(within(luaCard).getByText('lua')).toBeInTheDocument();
		// 非 .lua/.py/.json/.toml 不进快捷列表
		expect(screen.queryByText('notes.txt', { selector: '.quick-card strong' })).not.toBeInTheDocument();

		await fireEvent.click(luaCard);
		await waitFor(() => {
			// dev 模式 start 仅切换已有条目状态，新路径以成功日志为准
			expect(logMessages().some(m => m === '已启动脚本: scripts/mini.lua')).toBe(true);
		});
	});

	it('快捷列表为空时显示空态提示', async () => {
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('triggers.lua', { selector: '.file-name' })).toBeInTheDocument();
		});
		scriptFiles.set([{ path: 'readme.md', name: 'readme.md', is_dir: false, size: 3, modified: Date.now() }]);
		await waitFor(() => {
			expect(screen.getByText('脚本目录暂无可启动文件，可在下方文件管理中新建')).toBeInTheDocument();
		});
	});
});

describe('scripts 页：操作守卫与失败路径', () => {
	it('未连接时单脚本操作与批量操作记录错误日志', async () => {
		await connection.disconnect();
		scripts.set([makeScript('r1', 'running')]);
		render(Page);

		await fireEvent.click(within(itemOf('r1.lua')).getByRole('button', { name: '暂停' }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '未连接到 runtime IPC')).toBe(true);
		});

		// 批量操作同样被守卫拦截（jsdom 可点击 disabled 按钮）
		await fireEvent.click(screen.getByRole('button', { name: /全部暂停/ }));
		await fireEvent.click(screen.getByRole('button', { name: /全部恢复/ }));
		await fireEvent.click(screen.getByRole('button', { name: /全部停止/ }));
		const guardLogs = logMessages().filter(m => m === '未连接到 runtime IPC');
		expect(guardLogs.length).toBeGreaterThanOrEqual(4);
	});

	it('单脚本操作失败记录错误日志并复位 busy 状态', async () => {
		scripts.set([makeScript('r1', 'running')]);
		vi.spyOn(scripts, 'pause').mockRejectedValue(new Error('ipc down'));
		render(Page);

		const btn = within(itemOf('r1.lua')).getByRole('button', { name: '暂停' });
		await fireEvent.click(btn);
		await waitFor(() => {
			expect(logMessages().some(m => m === '暂停失败: Error: ipc down' && get(logs).find(e => e.message === '暂停失败: Error: ipc down')?.type === 'error')).toBe(true);
		});
		// busy 复位：按钮标签从 … 恢复
		await waitFor(() => {
			expect(within(itemOf('r1.lua')).getByRole('button', { name: '暂停' })).toBeInTheDocument();
		});
	});

	it('重启操作成功写重启日志（error 脚本的重试入口）', async () => {
		scripts.set([makeScript('bad', 'error', { error: 'boom' })]);
		render(Page);

		await fireEvent.click(within(itemOf('bad.lua')).getByRole('button', { name: '重试' }));
		await waitFor(() => {
			// error 状态的 restart 操作 label 为「重试」
			expect(logMessages().some(m => m === '已重试脚本: bad.lua')).toBe(true);
		});
		expect(get(scripts)[0].state).toBe('running');
	});

	it('批量操作无目标时提示，部分失败给出 1/2 警告', async () => {
		scripts.set([makeScript('stopped-one', 'stopped')]);
		render(Page);
		await fireEvent.click(screen.getByRole('button', { name: /全部暂停/ }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '没有符合条件的脚本')).toBe(true);
		});

		// 两个 running，其一 pause 失败 → 批量暂停完成: 1/2（warning）
		logs.clear();
		scripts.set([makeScript('ok', 'running'), makeScript('bad', 'running')]);
		vi.spyOn(scripts, 'pause').mockImplementation(async (id: string) => {
			if (id === 'bad') throw new Error('refused');
		});
		await fireEvent.click(screen.getByRole('button', { name: /全部暂停/ }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '批量暂停完成: 1/2')).toBe(true);
		});
		expect(get(logs).find(e => e.message === '批量暂停完成: 1/2')?.type).toBe('warning');
	});

	it('批量恢复与批量停止成功路径', async () => {
		scripts.set([makeScript('p1', 'paused'), makeScript('p2', 'paused'), makeScript('r1', 'running')]);
		render(Page);

		await fireEvent.click(screen.getByRole('button', { name: /全部恢复/ }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '批量恢复完成: 2/2')).toBe(true);
		});
		expect(get(scripts).every(s => s.state === 'running')).toBe(true);

		await fireEvent.click(screen.getByRole('button', { name: /全部停止/ }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '批量停止完成: 3/3')).toBe(true);
		});
		expect(get(scripts).every(s => s.state === 'stopped')).toBe(true);
	});

	it('按路径启动：空路径提示，未连接报错，失败记录错误', async () => {
		render(Page);
		const input = screen.getByPlaceholderText('scripts/example.lua 或绝对路径') as HTMLInputElement;

		// 空路径（初始预填 scripts/example.lua，先清空）
		await fireEvent.input(input, { target: { value: '' } });
		await fireEvent.click(screen.getByRole('button', { name: '启动脚本' }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '请输入脚本路径')).toBe(true);
		});

		// 未连接（disabled 按钮在 jsdom 中可点击）
		logs.clear();
		await fireEvent.input(input, { target: { value: 'scripts/x.lua' } });
		await connection.disconnect();
		await fireEvent.click(screen.getByRole('button', { name: '启动脚本' }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '未连接到 runtime IPC')).toBe(true);
		});

		// 连接但 start 失败
		logs.clear();
		await connection.connect();
		vi.spyOn(scripts, 'start').mockRejectedValue(new Error('script not found'));
		await fireEvent.click(screen.getByRole('button', { name: '启动脚本' }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '启动失败: Error: script not found')).toBe(true);
		});
		// 失败后按钮标签复位
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '启动脚本' })).toBeInTheDocument();
		});
	});

	it('路径输入框回车触发启动', async () => {
		render(Page);
		const input = screen.getByPlaceholderText('scripts/example.lua 或绝对路径');
		await fireEvent.input(input, { target: { value: 'scripts/enter.lua' } });
		await fireEvent.keyDown(input, { key: 'Enter' });
		await waitFor(() => {
			expect(logMessages().some(m => m === '已启动脚本: scripts/enter.lua')).toBe(true);
		});
	});

	it('列表刷新与日志导航', async () => {
		router.navigate('scripts');
		const { container } = render(Page);

		// 两个刷新按钮：文件管理工具栏在 .files-actions，列表工具栏在 .list-toolbar
		const listRefresh = container.querySelector('.list-toolbar > .btn') as HTMLButtonElement;
		await fireEvent.click(listRefresh);
		await waitFor(() => {
			expect(logMessages().some(m => m === '脚本列表已刷新')).toBe(true);
		});

		await fireEvent.click(screen.getByRole('button', { name: '日志' }));
		expect(get(router).current).toBe('logs');
	});
});

describe('scripts 页：文件管理边界', () => {
	it('刷新文件列表成功与失败', async () => {
		const { container } = render(Page);
		await waitFor(() => {
			expect(screen.getByText('triggers.lua', { selector: '.file-name' })).toBeInTheDocument();
		});

		const filesRefresh = container.querySelector('.files-actions .btn') as HTMLButtonElement;
		await fireEvent.click(filesRefresh);
		await waitFor(() => {
			expect(logMessages().some(m => m === '脚本文件列表已刷新')).toBe(true);
		});

		vi.spyOn(scriptFiles, 'load').mockRejectedValue(new Error('fs broken'));
		await fireEvent.click(filesRefresh);
		await waitFor(() => {
			expect(logMessages().some(m => m === '刷新脚本文件失败: Error: fs broken')).toBe(true);
		});
	});

	it('选中文件被移出列表后重新读取报错显示错误面板', async () => {
		render(Page);
		await fireEvent.click(screen.getByText('triggers.lua', { selector: '.file-name' }));
		await waitFor(() => {
			expect(screen.getByText('config/triggers.lua', { selector: '.preview-path' })).toBeInTheDocument();
		});

		// 条目消失后 read 找不到 → previewError
		scriptFiles.set([]);
		await fireEvent.click(screen.getByRole('button', { name: '重新读取' }));
		await waitFor(() => {
			expect(screen.getByText(/文件不存在/)).toBeInTheDocument();
		});
	});

	it('新建文件空白路径回车提示；创建失败记录错误', async () => {
		render(Page);
		const input = screen.getByPlaceholderText('新建文件相对路径，如 scripts/new.lua');

		await fireEvent.input(input, { target: { value: '   ' } });
		await fireEvent.keyDown(input, { key: 'Enter' });
		await waitFor(() => {
			expect(logMessages().some(m => m === '请输入新建文件的相对路径')).toBe(true);
		});

		vi.spyOn(scriptFiles, 'create').mockRejectedValue(new Error('readonly'));
		await fireEvent.input(input, { target: { value: 'scripts/denied.lua' } });
		await fireEvent.click(screen.getByRole('button', { name: '新建' }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '新建文件失败: Error: readonly')).toBe(true);
		});
	});

	it('行内删除按钮支持键盘 Enter 确认，删除失败记录错误', async () => {
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('triggers.lua', { selector: '.file-name' })).toBeInTheDocument();
		});
		const row = screen.getByText('triggers.lua', { selector: '.file-name' }).closest('.file-row') as HTMLElement;
		const inlineDelete = within(row).getByText('删');

		// Enter 第一次进入待确认
		await fireEvent.keyDown(inlineDelete, { key: 'Enter' });
		await waitFor(() => {
			expect(within(row).getByText('确认')).toBeInTheDocument();
		});

		vi.spyOn(scriptFiles, 'removeDev').mockRejectedValue(new Error('locked'));
		await fireEvent.keyDown(within(row).getByText('确认'), { key: ' ' });
		await waitFor(() => {
			expect(logMessages().some(m => m === '删除文件失败: Error: locked')).toBe(true);
		});
	});

	it('预览面板删除当前文件后清空预览回到空态', async () => {
		render(Page);
		await fireEvent.click(screen.getByText('example.lua', { selector: '.file-name' }));
		await waitFor(() => {
			expect(screen.getByText('scripts/example.lua', { selector: '.preview-path' })).toBeInTheDocument();
		});

		await fireEvent.click(screen.getByRole('button', { name: '删除' }));
		await fireEvent.click(screen.getByRole('button', { name: '确认删除' }));
		await waitFor(() => {
			expect(screen.getByText('未选择文件')).toBeInTheDocument();
		});
	});

	it('目录条目过多时显示截断提示', async () => {
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('triggers.lua', { selector: '.file-name' })).toBeInTheDocument();
		});
		scriptFiles.set([
			{ path: 'a.lua', name: 'a.lua', is_dir: false, size: 1, modified: 0 },
		], true);
		await waitFor(() => {
			expect(screen.getByText('目录条目过多，列表已截断')).toBeInTheDocument();
		});
	});

	it('修改时间显示：0 为 -，近期为刚刚，其余本地化时间', async () => {
		render(Page);
		scriptFiles.set([
			{ path: 'zero.lua', name: 'zero.lua', is_dir: false, size: 1, modified: 0 },
			{ path: 'now.lua', name: 'now.lua', is_dir: false, size: 1, modified: Date.now() },
			{ path: 'old.lua', name: 'old.lua', is_dir: false, size: 1, modified: Date.now() - 3_600_000 },
		]);
		await waitFor(() => {
			expect(screen.getByText('now.lua', { selector: '.file-name' })).toBeInTheDocument();
		});
		const metaOf = (name: string) =>
			(screen.getByText(name, { selector: '.file-name' }).closest('.file-row') as HTMLElement)
				.querySelector('.file-meta')!.textContent || '';
		// 页面 nowTick 每秒 tick 一次，等首个 tick 后「刚刚」判定生效
		await waitFor(() => {
			expect(metaOf('now.lua')).toContain('刚刚');
		}, { timeout: 2500 });
		expect(metaOf('old.lua')).not.toContain('刚刚');
		expect(metaOf('old.lua')).toMatch(/, \d{1,2}:\d{2}/);
		// modified=0 → '-'（formatModified 的 falsy 分支）
		expect(metaOf('zero.lua')).toContain('-');
		expect(metaOf('zero.lua')).not.toContain('刚刚');
	});
});

describe('scripts 页：根目录切换', () => {
	it('prompt 输入新目录后切换根目录并写日志', async () => {
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('/dev/demo/scripts-root')).toBeInTheDocument();
		});

		const prompt = vi.fn(() => '/custom/root');
		vi.stubGlobal('prompt', prompt);
		await fireEvent.click(screen.getByRole('button', { name: '更改目录' }));
		await waitFor(() => {
			expect(screen.getByText('/custom/root')).toBeInTheDocument();
		});
		expect(logMessages().some(m => m === '脚本根目录已切换: /custom/root')).toBe(true);
		vi.unstubAllGlobals();
	});

	it('prompt 取消不动根目录，空串恢复默认目录', async () => {
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('/dev/demo/scripts-root')).toBeInTheDocument();
		});

		vi.stubGlobal('prompt', vi.fn(() => null));
		await fireEvent.click(screen.getByRole('button', { name: '更改目录' }));
		await new Promise(r => setTimeout(r, 50));
		expect(screen.getByText('/dev/demo/scripts-root')).toBeInTheDocument();

		vi.stubGlobal('prompt', vi.fn(() => '   '));
		await fireEvent.click(screen.getByRole('button', { name: '更改目录' }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '脚本根目录已切换: /dev/demo/scripts-root')).toBe(true);
		});
		vi.unstubAllGlobals();
	});

	it('设置根目录失败记录错误日志', async () => {
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('/dev/demo/scripts-root')).toBeInTheDocument();
		});

		vi.stubGlobal('prompt', vi.fn(() => '/denied'));
		vi.spyOn(scriptFiles, 'setRoot').mockRejectedValue(new Error('no access'));
		await fireEvent.click(screen.getByRole('button', { name: '更改目录' }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '设置脚本根目录失败: Error: no access')).toBe(true);
		});
		vi.unstubAllGlobals();
	});
});
