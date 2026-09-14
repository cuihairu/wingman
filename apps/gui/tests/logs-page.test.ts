import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import { get } from 'svelte/store';
import Page from '../src/routes/logs/+page.svelte';
import { logs, type LogEntry } from '$lib/stores/logs';
import { installClipboardStub, installObjectURLStub } from './helpers/test-utils';

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
});

/// 读取四张计数卡的数字
function cardValues(container: HTMLElement): string[] {
	return [...container.querySelectorAll('.metric-card strong')].map(el => el.textContent ?? '');
}

describe('日志页面', () => {
	it('空态渲染与禁用的导出/清空按钮', () => {
		const { container } = render(Page);
		expect(screen.getByText('系统日志')).toBeInTheDocument();
		expect(screen.getByText('还没有日志')).toBeInTheDocument();
		expect(screen.getByRole('button', { name: '导出' })).toBeDisabled();
		expect(screen.getByRole('button', { name: '清空' })).toBeDisabled();
		expect(cardValues(container)).toEqual(['0', '0', '0', '0']);
	});

	it('条目渲染时间、级别徽章与来源缩写', () => {
		logs.add('GUI 操作', 'info');
		logs.addRuntime('runtime 上报', 'warning');
		const { container } = render(Page);
		expect(screen.getByText('GUI 操作')).toBeInTheDocument();
		expect(screen.getByText('runtime 上报')).toBeInTheDocument();
		expect(container.querySelector('.log-level.log-info')!.textContent).toBe('INFO');
		expect(container.querySelector('.log-level.log-warning')!.textContent).toBe('WARNING');
		expect(container.querySelector('.src-gui')).toBeInTheDocument();
		expect(container.querySelector('.src-runtime')!.textContent).toBe('RT');
		expect(screen.getByText('2 lines')).toBeInTheDocument();
	});

	it('计数卡统计总量/来源/警告/错误', () => {
		logs.add('a', 'info');
		logs.add('b', 'error');
		logs.add('c', 'error');
		logs.add('d', 'warning');
		logs.addRuntime('e', 'info');
		const { container } = render(Page);
		expect(cardValues(container)).toEqual(['5', '1', '1', '2']);
	});

	it('级别筛选与来源筛选组合', async () => {
		logs.add('info 行', 'info');
		logs.add('error 行', 'error');
		logs.addRuntime('runtime 行', 'info');
		const { container } = render(Page);

		// 级别组：错误
		await fireEvent.click(screen.getByRole('button', { name: '错误' }));
		expect(screen.getByText('error 行')).toBeInTheDocument();
		expect(screen.queryByText('info 行')).not.toBeInTheDocument();
		expect(screen.getByText('1 lines')).toBeInTheDocument();

		// 叠加来源组：GUI → error 行仍是 gui 来源，保留
		const sourceGroup = container.querySelectorAll('[aria-label="日志来源筛选"] button');
		await fireEvent.click(sourceGroup[1]); // GUI
		expect(screen.getByText('error 行')).toBeInTheDocument();

		// 复位级别组的全部
		const levelGroup = container.querySelectorAll('[aria-label="日志等级筛选"] button');
		await fireEvent.click(levelGroup[0]); // 全部
		expect(screen.getByText('2 lines')).toBeInTheDocument(); // error 行 + runtime 行（GUI 筛选排除 gui 的 info 行）
	});

	it('搜索关键字匹配消息，无匹配显示空态与清除筛选', async () => {
		logs.add('alpha 启动完成', 'info');
		logs.add('beta 失败', 'error');
		render(Page);
		const search = screen.getByPlaceholderText('搜索日志内容或时间');

		await fireEvent.input(search, { target: { value: 'beta' } });
		expect(screen.getByText('beta 失败')).toBeInTheDocument();
		expect(screen.queryByText('alpha 启动完成')).not.toBeInTheDocument();
		expect(screen.getByText('1 lines')).toBeInTheDocument();

		await fireEvent.input(search, { target: { value: 'zzz-不存在' } });
		expect(screen.getByText('没有匹配项')).toBeInTheDocument();

		await fireEvent.click(screen.getByRole('button', { name: '清除筛选' }));
		expect(screen.getByText('2 lines')).toBeInTheDocument();
		expect((search as HTMLInputElement).value).toBe('');
	});

	it('dropped 横幅显示累计丢弃数', async () => {
		render(Page);
		expect(screen.queryByText(/事件缓冲溢出/)).not.toBeInTheDocument();

		logs.notifyDropped(7);
		await waitFor(() => {
			expect(screen.getByText(/累计丢弃 7 条事件/)).toBeInTheDocument();
		});
	});

	it('清空按钮重置日志', async () => {
		logs.add('some log', 'info');
		render(Page);
		await fireEvent.click(screen.getByRole('button', { name: '清空' }));
		expect(get(logs)).toHaveLength(0);
		expect(screen.getByText('还没有日志')).toBeInTheDocument();
	});

	it('导出生成下载链接', async () => {
		logs.add('导出我', 'info');
		const { create, revoke } = installObjectURLStub();
		const clickSpy = vi.spyOn(HTMLAnchorElement.prototype, 'click').mockImplementation(() => {});
		render(Page);

		await fireEvent.click(screen.getByRole('button', { name: '导出' }));
		expect(create).toHaveBeenCalledTimes(1);
		expect(clickSpy).toHaveBeenCalledTimes(1);
		expect(revoke).toHaveBeenCalledTimes(1);
		const link = clickSpy.mock.instances[0] as HTMLAnchorElement;
		expect(link.download).toMatch(/^wingman-logs-.+\.txt$/);
		clickSpy.mockRestore();
	});

	it('点击日志条目复制到剪贴板，剪贴板不可用时静默', async () => {
		const clip = installClipboardStub();
		logs.add('复制这行', 'warning');
		render(Page);
		await fireEvent.click(screen.getByText('复制这行'));
		await waitFor(() => {
			expect(clip.written).toHaveLength(1);
		});
		expect(clip.written[0]).toContain('[WARNING] [gui] 复制这行');

		// 移除剪贴板：copyEntry 的 catch 静默
		delete (navigator as any).clipboard;
		await fireEvent.click(screen.getByText('复制这行'));
		expect(clip.written).toHaveLength(1);
	});

	it('滚动离开底部后新日志显示跳转按钮', async () => {
		for (let i = 0; i < 5; i++) logs.add(`line-${i}`, 'info');
		const { container } = render(Page);
		const logBox = container.querySelector('.log-container') as HTMLElement;

		// stub 滚动几何：滚离底部
		Object.defineProperty(logBox, 'scrollHeight', { value: 1000, configurable: true });
		Object.defineProperty(logBox, 'clientHeight', { value: 200, configurable: true });
		logBox.scrollTop = 100;
		await fireEvent.scroll(logBox);

		logs.add('新日志 1', 'info');
		logs.add('新日志 2', 'info');
		await waitFor(() => {
			expect(screen.getByText(/2 条新日志/)).toBeInTheDocument();
		});

		// 点击回到底部，按钮消失
		await fireEvent.click(screen.getByText(/2 条新日志/));
		expect(screen.queryByText(/条新日志/)).not.toBeInTheDocument();
	});
});
