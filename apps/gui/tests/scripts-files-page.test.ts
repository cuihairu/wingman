import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, within, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/scripts/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';

/**
 * scripts 页「脚本文件管理」面板测试（只覆盖文件管理能力，页面其余行为
 * 见 scripts-page.test.ts）。dev 模式验证交互冒烟，invoke 模式验证与
 * Rust 命令的对接参数。文件管理不依赖 runtime IPC：新建/删除/浏览在
 * 未连接时也可用，「启动」仍需连接。
 */

function fileRow(path: string): HTMLElement {
	const name = path.split('/').pop() || path;
	return screen.getByText(name, { selector: '.file-name' }).closest('.file-row') as HTMLElement;
}

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	await connection.connect();
});

afterEach(() => {
	delete (window as any).__TAURI_INVOKE__;
});

describe('文件管理面板（dev 模式）', () => {
	it('渲染演示文件树与根目录，目录行禁用、文件行可点', async () => {
		render(Page);

		await waitFor(() => {
			expect(screen.getByText('triggers.lua', { selector: '.file-name' })).toBeInTheDocument();
		});
		expect(screen.getByText('example.py', { selector: '.file-name' })).toBeInTheDocument();
		expect(screen.getByText('/dev/demo/scripts-root')).toBeInTheDocument();

		const dirRow = fileRow('config');
		expect(dirRow).toBeDisabled();
		expect(fileRow('scripts/example.lua')).toBeEnabled();
	});

	it('点击文件选中并展示只读预览', async () => {
		render(Page);

		await fireEvent.click(fileRow('scripts/example.lua'));

		await waitFor(() => {
			expect(screen.getByText('scripts/example.lua', { selector: '.preview-path' })).toBeInTheDocument();
			expect(screen.getByText(/dev 预览: scripts\/example.lua/)).toBeInTheDocument();
		});
		expect(fileRow('scripts/example.lua')).toHaveClass('selected');
	});

	it('新建文件：输入相对路径后写入列表并自动预览', async () => {
		render(Page);

		const input = screen.getByPlaceholderText('新建文件相对路径，如 scripts/new.lua');
		await fireEvent.input(input, { target: { value: 'scripts/fresh.lua' } });
		await fireEvent.click(screen.getByRole('button', { name: '新建' }));

		await waitFor(() => {
			expect(screen.getByText('fresh.lua', { selector: '.file-name' })).toBeInTheDocument();
		});
		const logEntries = get(logs);
		expect(logEntries.some(e => e.message.includes('已新建脚本文件: scripts/fresh.lua'))).toBe(true);
	});

	it('删除走两段式确认：第一次待确认、第二次执行移除', async () => {
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('example.lua', { selector: '.file-name' })).toBeInTheDocument();
		});

		// 预览面板的删除按钮（同一文件的两段式确认）
		await fireEvent.click(fileRow('scripts/example.lua'));
		const previewDelete = await waitFor(() =>
			screen.getByRole('button', { name: '删除' })
		);
		await fireEvent.click(previewDelete);
		expect(screen.getByRole('button', { name: '确认删除' })).toBeInTheDocument();

		await fireEvent.click(screen.getByRole('button', { name: '确认删除' }));
		await waitFor(() => {
			expect(screen.queryByText('example.lua', { selector: '.file-name' })).not.toBeInTheDocument();
		});
		const logEntries = get(logs);
		expect(logEntries.some(e => e.message.includes('已删除脚本文件: scripts/example.lua'))).toBe(true);
	});

	it('文件树行内删除按钮与预览删除按钮共享两段式状态', async () => {
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('triggers.lua', { selector: '.file-name' })).toBeInTheDocument();
		});

		const row = fileRow('config/triggers.lua');
		const inlineDelete = within(row).getByText('删');
		await fireEvent.click(inlineDelete);
		await waitFor(() => {
			expect(within(fileRow('config/triggers.lua')).getByText('确认')).toBeInTheDocument();
		});
	});
});

describe('文件管理面板（invoke 模式）', () => {
	it('与 Rust 命令对接：列表/预览/新建/删除/启动的参数与日志', async () => {
		const calls: Array<{ cmd: string; args?: Record<string, unknown> }> = [];
		(window as any).__TAURI_INVOKE__ = vi.fn(async (cmd: string, args?: Record<string, unknown>) => {
			calls.push({ cmd, args });
			switch (cmd) {
				case 'connect_ipc':
					return {};
				case 'get_ipc_state':
					return { connected: true, endpoint: 'wingman' };
				case 'get_system_status':
					return { server: 'w', version: '1', uptime: 1, running_scripts: 0, paused: false };
				case 'get_scripts_root':
					return { root: '/opt/wm/scripts', source: 'setting' };
				case 'list_script_files':
					return {
						entries: [
							{ path: 'scripts', name: 'scripts', is_dir: true, size: 0, modified: 100 },
							{ path: 'scripts/plan.lua', name: 'plan.lua', is_dir: false, size: 64, modified: 200 },
						],
						truncated: false,
					};
				case 'read_script_file':
					return { path: 'scripts/plan.lua', content: '-- plan body', size: 13, modified: 200 };
				case 'write_script_file':
					return { path: args?.path, name: 'made.lua', is_dir: false, size: 0, modified: 300 };
				case 'delete_script_file':
					return null;
				case 'start_script':
					return { script_id: 's1', status: 'running' };
				case 'get_scripts':
					return [];
				default:
					return null;
			}
		});

		vi.resetModules();
		const [connectionMod, pageMod, testingLib] = await Promise.all([
			import('$lib/stores/connection'),
			import('../src/routes/scripts/+page.svelte'),
			import('@testing-library/svelte'),
		]);
		const rtl = testingLib as typeof import('@testing-library/svelte');
		await connectionMod.connection.connect();

		rtl.render(pageMod.default);
		const screen2 = rtl.screen;
		const { waitFor: wait } = rtl;

		// 列表来自 list_script_files
		await wait(() => {
			expect(screen2.getByText('plan.lua', { selector: '.file-name' })).toBeInTheDocument();
		});

		// 选中 → read_script_file 预览
		await rtl.fireEvent.click(screen2.getByText('plan.lua', { selector: '.file-name' }));
		await wait(() => {
			expect(screen2.getByText('-- plan body')).toBeInTheDocument();
		});
		expect(calls.some(c => c.cmd === 'read_script_file' && c.args?.path === 'scripts/plan.lua')).toBe(true);

		// 启动选中文件 → start_script 用选中路径
		await rtl.fireEvent.click(screen2.getByRole('button', { name: '启动', exact: true }));
		await wait(() => {
			expect(calls.some(c => c.cmd === 'start_script' && c.args?.path === 'scripts/plan.lua')).toBe(true);
		});

		// 新建 → write_script_file 空内容
		const input = screen2.getByPlaceholderText('新建文件相对路径，如 scripts/new.lua');
		await rtl.fireEvent.input(input, { target: { value: 'scripts/made.lua' } });
		await rtl.fireEvent.click(screen2.getByRole('button', { name: '新建' }));
		await wait(() => {
			expect(calls.some(c => c.cmd === 'write_script_file' && c.args?.path === 'scripts/made.lua' && c.args?.content === '')).toBe(true);
		});

		// 删除两段式 → delete_script_file
		await rtl.fireEvent.click(screen2.getByRole('button', { name: '删除' }));
		await rtl.fireEvent.click(screen2.getByRole('button', { name: '确认删除' }));
		await wait(() => {
			expect(calls.some(c => c.cmd === 'delete_script_file' && c.args?.path === 'scripts/made.lua')).toBe(true);
		});
		expect(screen2.queryByText('made.lua', { selector: '.file-name' })).not.toBeInTheDocument();
	});
});
