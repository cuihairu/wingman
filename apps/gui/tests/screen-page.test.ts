import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import { get } from 'svelte/store';
import Page from '../src/routes/screen/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { screen as screenStore } from '$lib/stores/screen';
import {
	installCanvasStub,
	installClipboardStub,
	installPointerCaptureStub,
	installTolerantStyleAttrStub,
	makeImgLoaded,
	stubRect,
	uninstallTolerantStyleAttrStub,
} from './helpers/test-utils';

/// 容器 128x72 表示 1280x720 截图（1px 容器 = 10px 图像坐标）
const RECT_W = 128;
const RECT_H = 72;

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	installPointerCaptureStub();
	installTolerantStyleAttrStub();
	installCanvasStub();
	logs.clear();
	screenStore.clear();
	await connection.disconnect();
});

afterEach(() => {
	uninstallTolerantStyleAttrStub();
	screenStore.clear();
});

/// 等待自动截图出现并把 img 置为已加载，触发 prepareCanvas
async function readyPage() {
	await waitFor(() => {
		expect(screen.getByRole('img', { name: '屏幕截图' })).toBeInTheDocument();
	});
	const img = screen.getByRole('img', { name: '屏幕截图' }) as HTMLImageElement;
	makeImgLoaded(img, 1280, 720);
	stubRect(img, 0, 0, RECT_W, RECT_H);
	await fireEvent.load(img);
	await new Promise(r => setTimeout(r, 0));
	return img;
}

describe('屏幕预览页面（dev 模式）', () => {
	it('挂载自动截图并显示预览信息与 dev 状态', async () => {
		// dev fallback 截图在挂载 effect 内同步完成，无"等待截图"空态（空态由单独用例覆盖）
		render(Page);
		await waitFor(() => {
			// 工具栏 span 还带有 "· 时间" 后缀
			expect(screen.getByText(/1280x720 @ 0,0/)).toBeInTheDocument();
		});
		expect(screen.getByText('dev fallback')).toBeInTheDocument();
		// dev 下 listMonitors 提示
		expect(screen.getByText('runtime 未连接，仅显示开发预览')).toBeInTheDocument();
	});

	it('截图就绪后可勾选高亮匹配；黑像素匹配黑色目标统计全部像素', async () => {
		const contexts = installCanvasStub();
		render(Page);
		const img = await readyPage();

		const checkbox = screen.getByRole('checkbox', { name: '在画面中高亮匹配像素' });
		expect(checkbox).toBeEnabled();
		expect(checkbox).not.toBeChecked();

		// 默认目标 #3fb950 vs 黑像素 → 无匹配
		await fireEvent.click(checkbox);
		await waitFor(() => {
			expect(screen.getByText('无匹配像素')).toBeInTheDocument();
		});

		// 目标改为黑色 → 1280*720 全部匹配
		const colorInput = screen.getByPlaceholderText('#ff0000') as HTMLInputElement;
		await fireEvent.input(colorInput, { target: { value: '#000000' } });
		await waitFor(() => {
			expect(screen.getByText('921,600')).toBeInTheDocument();
		});
		// 高亮层 putImageData 已执行
		await waitFor(() => {
			expect(contexts.some(c => c.putImageData.mock.calls.length > 0)).toBe(true);
		});
		expect(img).toBeInTheDocument();
	});

	it('拖拽框选区域同步 RegionPicker 与 overlay 标签', async () => {
		render(Page);
		await readyPage();

		const surface = screen.getByRole('img', { name: '屏幕截图预览：拖拽框选区域，单击拾取坐标与颜色' });
		await fireEvent.pointerDown(surface, { clientX: 10, clientY: 5, pointerId: 1 });
		await fireEvent.pointerMove(surface, { clientX: 30, clientY: 15, pointerId: 1 });
		// 拖拽中的尺寸标签（图内坐标差 200×100）
		expect(screen.getByText('200×100')).toBeInTheDocument();

		await fireEvent.pointerUp(surface, { pointerId: 1 });
		await waitFor(() => {
			expect(screen.getByLabelText('X')).toHaveValue(100);
		});
		expect(screen.getByLabelText('Y')).toHaveValue(50);
		expect(screen.getByLabelText('W')).toHaveValue(200);
		expect(screen.getByLabelText('H')).toHaveValue(100);
		expect(screen.getByText('100,50 · 200x100')).toBeInTheDocument();
	});

	it('单击拾取坐标与颜色，可复制并设为查找颜色', async () => {
		const clip = installClipboardStub();
		render(Page);
		await readyPage();

		const surface = screen.getByRole('img', { name: '屏幕截图预览：拖拽框选区域，单击拾取坐标与颜色' });
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerUp(surface, { pointerId: 1 });

		// 屏幕绝对坐标卡片（默认黑像素）
		expect(screen.getByText('128, 72')).toBeInTheDocument();
		expect(screen.queryByText('点击截图区域以记录坐标。')).not.toBeInTheDocument();

		// 复制坐标
		const copyButtons = screen.getAllByRole('button', { name: '复制' });
		await fireEvent.click(copyButtons[0]); // 坐标复制
		await waitFor(() => {
			expect(clip.written).toContain('128,72');
		});

		// 设为查找颜色 → ColorPicker 目标色输入同步
		await fireEvent.click(screen.getByRole('button', { name: '设为查找颜色' }));
		expect((screen.getByPlaceholderText('#ff0000') as HTMLInputElement).value).toBe('#000000');

		// 复制拾取的颜色（剪贴板收纯色值，"已复制颜色"进日志）
		const colorCopyButtons = screen.getAllByRole('button', { name: '复制' });
		await fireEvent.click(colorCopyButtons[colorCopyButtons.length - 1]);
		await waitFor(() => {
			expect(clip.written).toContain('#000000');
		});
		expect(get(logs).some(e => e.message === '已复制颜色: #000000')).toBe(true);
	});

	it('以坐标为中心生成区域，全图按钮恢复完整截图区域', async () => {
		render(Page);
		await readyPage();

		const surface = screen.getByRole('img', { name: '屏幕截图预览：拖拽框选区域，单击拾取坐标与颜色' });
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerUp(surface, { pointerId: 1 });

		// 先把区域缩到 240x160（RegionPicker 走 oninput），再以坐标为中心：围绕 (128,72)，且不低于 0
		await fireEvent.input(screen.getByLabelText('W'), { target: { value: '240' } });
		await fireEvent.input(screen.getByLabelText('H'), { target: { value: '160' } });
		await fireEvent.click(screen.getByRole('button', { name: '以坐标为中心' }));
		expect(screen.getByLabelText('X')).toHaveValue(8);
		expect(screen.getByLabelText('Y')).toHaveValue(0);
		expect(screen.getByLabelText('W')).toHaveValue(240);
		expect(screen.getByLabelText('H')).toHaveValue(160);

		// 全图
		await fireEvent.click(screen.getByRole('button', { name: '全图' }));
		expect(screen.getByLabelText('X')).toHaveValue(0);
		expect(screen.getByLabelText('W')).toHaveValue(1280);
		expect(screen.getByLabelText('H')).toHaveValue(720);
		expect(screen.getByText('0,0 · 1280x720')).toBeInTheDocument();

		// 复制区域
		const clip = installClipboardStub();
		await fireEvent.click(screen.getByRole('button', { name: '复制区域' }));
		await waitFor(() => {
			expect(clip.written).toContain('0,0,1280,720');
		});
	});

	it('刷新截图按钮写成功日志', async () => {
		render(Page);
		await readyPage();

		await fireEvent.click(screen.getByRole('button', { name: '刷新截图' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'success' && e.message === '已刷新屏幕预览: 1280x720')).toBe(true);
		});
	});

	it('自动刷新开关建立与清理轮询定时器', async () => {
		// 在安装 fake timers 之前绑定真实现；spy 直接透传，
		// 避免 fake timers 与「挂载 effect → capture → store.set → effect 重入」
		// 的真实异步链路相互干扰导致微任务风暴（OOM）。
		const realCapture = screenStore.capture.bind(screenStore);
		vi.useFakeTimers();
		try {
			const captureSpy = vi.spyOn(screenStore, 'capture').mockImplementation(async (...args: any[]) => {
				return (realCapture as any)(...args);
			});
			render(Page);
			// 挂载自动截图 1 次（真实 timer 下已完成，这里再等挂载后首帧）
			await vi.advanceTimersByTimeAsync(0);
			captureSpy.mockClear();

			const toggle = screen.getByRole('checkbox', { name: '实时刷新' });
			await fireEvent.click(toggle);
			await vi.advanceTimersByTimeAsync(1500);
			await vi.advanceTimersByTimeAsync(1500);
			expect(captureSpy).toHaveBeenCalledTimes(2);

			await fireEvent.click(toggle); // 关闭 → 清理
			await vi.advanceTimersByTimeAsync(5000);
			expect(captureSpy).toHaveBeenCalledTimes(2);
		} finally {
			vi.useRealTimers();
			vi.mocked(screenStore.capture).mockRestore();
		}
	});

	it('无截图时空态提供立即捕获按钮', async () => {
		let release!: (v: any) => void;
		const gate = new Promise(resolve => { release = resolve; });
		const originalCapture = screenStore.capture.bind(screenStore);
		vi.spyOn(screenStore, 'capture').mockImplementation(async (...args: any[]) => {
			await gate;
			return originalCapture(...(args as []));
		});

		render(Page);
		expect(screen.getByText('尚无截图')).toBeInTheDocument();

		release(undefined);
		await waitFor(() => {
			expect(screen.getByRole('img', { name: '屏幕截图' })).toBeInTheDocument();
		});
	});
});
