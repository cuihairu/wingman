import { describe, it, expect, beforeEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, within, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/scripts/+page.svelte';
import { scripts, type ScriptInfo } from '$lib/stores/scripts';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';

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

/// 根据脚本名定位其列表项容器
function itemOf(name: string): HTMLElement {
	return screen.getByText(name).closest('.script-item') as HTMLElement;
}

const SEARCH_PLACEHOLDER = '搜索名称或路径';

describe('scripts 页面冒烟测试', () => {
	beforeEach(async () => {
		delete (window as any).__TAURI_INVOKE__;
		scripts.set([]);
		logs.clear();
		/// dev 模式下 connect 直接模拟已连接
		await connection.connect();
	});

	it('渲染页面骨架与空状态', () => {
		render(Page);
		expect(screen.getByText('脚本管理')).toBeInTheDocument();
		expect(screen.getByText('还没有加载脚本')).toBeInTheDocument();
		expect(screen.getByText('runtime IPC 已连接')).toBeInTheDocument();
	});

	it('指标卡统计脚本总数与各状态数量', () => {
		scripts.set([
			makeScript('r1', 'running'),
			makeScript('r2', 'running'),
			makeScript('p', 'paused'),
			makeScript('e', 'error'),
		]);
		const { container } = render(Page);
		const values = [...container.querySelectorAll('.metric-card strong')].map(el => el.textContent);
		expect(values).toEqual(['4', '2', '1', '1']);
	});

	it('按状态渲染操作矩阵与状态标签', () => {
		scripts.set([
			makeScript('run', 'running'),
			makeScript('hold', 'paused'),
			makeScript('fresh', 'loaded'),
			makeScript('bad', 'error'),
			makeScript('idle', 'stopped'),
			makeScript('myst', 'unknown'),
		]);
		render(Page);

		const cases: Array<[string, string[], string]> = [
			['run.lua', ['暂停', '停止', '重启'], '运行中'],
			['hold.lua', ['恢复', '停止', '重启'], '已暂停'],
			['fresh.lua', ['启动', '卸载'], '已加载'],
			['bad.lua', ['重试', '卸载'], '错误'],
			['idle.lua', ['启动', '重启', '卸载'], '已停止'],
			['myst.lua', ['启动', '重启', '卸载'], '未知'],
		];
		for (const [name, ops, chip] of cases) {
			const item = itemOf(name);
			expect(within(item).getByText(chip)).toBeInTheDocument();
			const labels = within(item).getAllByRole('button').map(b => b.textContent?.trim());
			expect(labels).toEqual(ops);
		}
	});

	it('错误脚本的错误信息可见', () => {
		scripts.set([makeScript('bad', 'error', { error: 'lua panic' })]);
		render(Page);
		expect(screen.getByText('lua panic')).toBeInTheDocument();
	});

	it('搜索过滤：按名称与路径匹配，大小写不敏感', async () => {
		scripts.set([
			makeScript('farm', 'running'),
			makeScript('city', 'stopped'),
			makeScript('ore', 'paused', { name: 'ore.lua', path: 'scripts/deep/MINE.lua' }),
		]);
		render(Page);

		const search = screen.getByPlaceholderText(SEARCH_PLACEHOLDER);
		await fireEvent.input(search, { target: { value: 'farm' } });
		await waitFor(() => {
			expect(screen.getByText('farm.lua')).toBeInTheDocument();
			expect(screen.queryByText('city.lua')).not.toBeInTheDocument();
		});

		// 仅路径包含 mine（大写输入小写匹配）
		await fireEvent.input(search, { target: { value: 'MINE' } });
		await waitFor(() => {
			expect(screen.queryByText('farm.lua')).not.toBeInTheDocument();
			expect(screen.getByText('ore.lua')).toBeInTheDocument();
		});
	});

	it('状态筛选与组合筛选、清除筛选', async () => {
		scripts.set([
			makeScript('farm', 'running'),
			makeScript('city', 'stopped'),
		]);
		render(Page);

		await fireEvent.click(screen.getByRole('button', { name: '运行中' }));
		await waitFor(() => {
			expect(screen.getByText('farm.lua')).toBeInTheDocument();
			expect(screen.queryByText('city.lua')).not.toBeInTheDocument();
		});

		// 组合：状态=运行中 + 搜索 city → 无匹配
		await fireEvent.input(screen.getByPlaceholderText(SEARCH_PLACEHOLDER), { target: { value: 'city' } });
		await waitFor(() => {
			expect(screen.getByText('没有匹配脚本')).toBeInTheDocument();
		});

		await fireEvent.click(screen.getByRole('button', { name: '清除筛选' }));
		await waitFor(() => {
			expect(screen.getByText('farm.lua')).toBeInTheDocument();
			expect(screen.getByText('city.lua')).toBeInTheDocument();
		});
	});

	it('批量操作按钮按计数启用/禁用', () => {
		scripts.set([makeScript('p', 'paused')]);
		render(Page);
		expect(screen.getByRole('button', { name: /全部暂停/ })).toBeDisabled();
		expect(screen.getByRole('button', { name: /全部恢复/ })).toBeEnabled();
		expect(screen.getByRole('button', { name: /全部停止/ })).toBeEnabled();
	});

	it('点击暂停联动 store 状态与日志', async () => {
		scripts.set([makeScript('r', 'running')]);
		render(Page);

		await fireEvent.click(within(itemOf('r.lua')).getByRole('button', { name: '暂停' }));
		await waitFor(() => {
			expect(within(itemOf('r.lua')).getByText('已暂停')).toBeInTheDocument();
			expect(get(scripts)[0].state).toBe('paused');
		});
		const entries = get(logs);
		expect(entries.some(e => e.message.includes('已暂停脚本: r.lua') && e.type === 'success')).toBe(true);
	});

	it('点击卸载后脚本从列表移除并回到空状态', async () => {
		scripts.set([makeScript('fresh', 'loaded')]);
		render(Page);

		await fireEvent.click(within(itemOf('fresh.lua')).getByRole('button', { name: '卸载' }));
		await waitFor(() => {
			expect(screen.queryByText('fresh.lua')).not.toBeInTheDocument();
			expect(screen.getByText('还没有加载脚本')).toBeInTheDocument();
		});
	});

	it('runtime 事件 applyState 后界面状态即时更新', async () => {
		scripts.set([makeScript('r', 'running')]);
		render(Page);
		expect(within(itemOf('r.lua')).getByText('运行中')).toBeInTheDocument();

		scripts.applyState('r', 'paused');
		await waitFor(() => {
			expect(within(itemOf('r.lua')).getByText('已暂停')).toBeInTheDocument();
		});
	});

	it('IPC 未连接时禁用全部操作按钮', async () => {
		await connection.disconnect();
		scripts.set([makeScript('r', 'running')]);
		render(Page);

		expect(screen.getByText('runtime IPC 未连接')).toBeInTheDocument();
		for (const btn of within(itemOf('r.lua')).getAllByRole('button')) {
			expect(btn).toBeDisabled();
		}
		expect(screen.getByRole('button', { name: /全部暂停/ })).toBeDisabled();
		expect(screen.getByRole('button', { name: '启动脚本' })).toBeDisabled();
	});
});
