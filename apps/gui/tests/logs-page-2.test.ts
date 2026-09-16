import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/logs/+page.svelte';
import { logs } from '$lib/stores/logs';

/**
 * 日志页第二组：智能滚动的滚回底部重置、自动滚动开关、rAF 跟随滚动。
 * 筛选/导出/复制与跳转按钮见 logs-page.test.ts。
 */

beforeEach(() => {
	logs.clear();
});

afterEach(() => {
	vi.restoreAllMocks();
});

function stubGeometry(el: HTMLElement, scrollHeight: number, clientHeight: number) {
	Object.defineProperty(el, 'scrollHeight', { value: scrollHeight, configurable: true });
	Object.defineProperty(el, 'clientHeight', { value: clientHeight, configurable: true });
}

async function nextFrame() {
	await new Promise(r => requestAnimationFrame(() => r()));
	await new Promise(r => setTimeout(r, 0));
}

describe('日志页：智能滚动', () => {
	it('滚回底部后清除未读计数与跳转按钮', async () => {
		for (let i = 0; i < 5; i++) logs.add(`line-${i}`, 'info');
		const { container } = render(Page);
		const logBox = container.querySelector('.log-container') as HTMLElement;

		// 上滚离开底部 → userScrolled
		stubGeometry(logBox, 1000, 200);
		logBox.scrollTop = 100;
		await fireEvent.scroll(logBox);

		logs.add('新日志', 'info');
		await waitFor(() => {
			expect(screen.getByText(/1 条新日志/)).toBeInTheDocument();
		});

		// 滚回底部 → handleScroll 重置 userScrolled/newCount，按钮消失
		logBox.scrollTop = 1000 - 200;
		await fireEvent.scroll(logBox);
		expect(screen.queryByText(/条新日志/)).not.toBeInTheDocument();
	});

	it('关闭自动滚动后新日志不触发跟随也不计数', async () => {
		for (let i = 0; i < 3; i++) logs.add(`base-${i}`, 'info');
		const { container } = render(Page);
		const logBox = container.querySelector('.log-container') as HTMLElement;
		stubGeometry(logBox, 1000, 200);

		const auto = screen.getByRole('checkbox', { name: '自动滚动' }) as HTMLInputElement;
		expect(auto.checked).toBe(true);
		await fireEvent.click(auto);
		expect(auto.checked).toBe(false);

		// 即使滚离底部，关闭自动滚动后 effect 直接返回：无未读计数
		logBox.scrollTop = 0;
		await fireEvent.scroll(logBox);
		logs.add('静默日志', 'info');
		await nextFrame();
		expect(screen.queryByText(/条新日志/)).not.toBeInTheDocument();
	});

	it('未上滚时新日志经 rAF 跟随滚动到底部', async () => {
		logs.add('初始', 'info');
		const { container } = render(Page);
		const logBox = container.querySelector('.log-container') as HTMLElement;
		stubGeometry(logBox, 1000, 200);
		expect(logBox.scrollTop).toBe(0);

		logs.add('追加', 'info');
		await waitFor(() => {
			expect(screen.getByText('追加')).toBeInTheDocument();
		});
		await nextFrame();
		expect(logBox.scrollTop).toBe(1000);
	});
});
