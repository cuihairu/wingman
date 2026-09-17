import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/triggers/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { triggers, type TriggerConfig } from '$lib/stores/triggers';

/** 触发器页第五组：最近命中的 title 提示、名称清空后的标题后缀、延时动作的 0 回退。 */

function makeTrigger(extra: Partial<TriggerConfig> = {}): TriggerConfig {
	return {
		id: 't1',
		name: '回血',
		enabled: true,
		condition: { type: 'color_found', value: '#ff0000', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 10, interval: 1000 },
		actions: [],
		...extra,
	} as TriggerConfig;
}

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	await connection.connect();
});

afterEach(() => {
	vi.restoreAllMocks();
});

describe('触发器页：最近命中提示', () => {
	it('last_triggered_at 有值时圆点点亮并显示最近命中时间', async () => {
		triggers.set([makeTrigger({ last_triggered: true, last_triggered_at: Date.now() } as Partial<TriggerConfig>)]);
		render(Page);
		const item = screen.getByText('回血').closest('.trigger-item') as HTMLElement;
		const dot = item.querySelector('.fired-dot') as HTMLElement;
		expect(dot.className).toContain('lit');
		expect(dot.getAttribute('title')).toContain('最近命中');
	});

	it('last_triggered_at 无效时间显示为空 title 回退', async () => {
		triggers.set([makeTrigger({ last_triggered: true, last_triggered_at: Number.NaN } as Partial<TriggerConfig>)]);
		render(Page);
		const item = screen.getByText('回血').closest('.trigger-item') as HTMLElement;
		const dot = item.querySelector('.fired-dot') as HTMLElement;
		expect(dot.getAttribute('title')).toBe('尚未命中');
	});
});

describe('触发器页：编辑标题', () => {
	it('清空名称后标题不带冒号后缀', async () => {
		triggers.set([makeTrigger()]);
		render(Page);
		await fireEvent.click(screen.getByText('回血'));
		await waitFor(() => {
			expect(screen.getByText('编辑触发器: 回血')).toBeInTheDocument();
		});
		const nameInput = screen.getByLabelText('名称') as HTMLInputElement;
		await fireEvent.input(nameInput, { target: { value: '' } });
		await waitFor(() => {
			expect(screen.getByText('编辑触发器')).toBeInTheDocument();
		});
	});
});

describe('触发器页：延时动作', () => {
	it('延时输入清零后显示回退 0', async () => {
		triggers.set([makeTrigger()]);
		render(Page);
		await fireEvent.click(screen.getByText('回血'));
		await fireEvent.click(screen.getByRole('button', { name: '+ 添加动作' }));
		await waitFor(() => {
			expect(screen.getByText('1')).toBeInTheDocument();
		});
		// 默认 log 动作切到 delay
		const select = screen.getByDisplayValue('日志') as HTMLSelectElement;
		await fireEvent.change(select, { target: { value: 'delay' } });
		await waitFor(() => {
			expect(screen.getByText('延时 (ms)')).toBeInTheDocument();
		});
		const delayInput = screen.getByLabelText('延时 (ms)') as HTMLInputElement;
		expect(delayInput.value).toBe('500');
		await fireEvent.input(delayInput, { target: { value: '' } });
		await waitFor(() => {
			expect((screen.getByLabelText('延时 (ms)') as HTMLInputElement).value).toBe('0');
		});
	});
});
