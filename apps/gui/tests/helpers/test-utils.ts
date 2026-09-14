import { expect, vi } from 'vitest';
import { screen, fireEvent, waitFor } from '@testing-library/svelte';

/// 让 jsdom 中的 <img> 表现为“已加载完成”，使组件的 prepareCanvas/draw 逻辑可被触发
export function makeImgLoaded(img: Element, width = 64, height = 64) {
	Object.defineProperty(img, 'naturalWidth', { value: width, configurable: true });
	Object.defineProperty(img, 'naturalHeight', { value: height, configurable: true });
	Object.defineProperty(img, 'complete', { value: true, configurable: true });
}

/// jsdom 的 getBoundingClientRect 全 0，指针坐标换算需要手动指定矩形
export function stubRect(el: Element, left = 0, top = 0, width = 64, height = 64) {
	(el as any).getBoundingClientRect = () => ({
		left, top, width, height,
		right: left + width, bottom: top + height,
		x: left, y: top,
		toJSON: () => ({}),
	});
}

export interface FakeCanvasContext {
	drawImage: ReturnType<typeof vi.fn>;
	clearRect: ReturnType<typeof vi.fn>;
	strokeRect: ReturnType<typeof vi.fn>;
	putImageData: ReturnType<typeof vi.fn>;
	getImageData: ReturnType<typeof vi.fn> & ((x: number, y: number, w: number, h: number) => { data: Uint8ClampedArray; width: number; height: number });
	createImageData: (w: number, h: number) => { data: Uint8ClampedArray; width: number; height: number };
	imageSmoothingEnabled: boolean;
	strokeStyle: string;
	lineWidth: number;
}

/**
 * 替换 HTMLCanvasElement.prototype.getContext，返回可控的 2D 上下文 stub。
 * 每次调用 getContext 都会新建上下文（组件可能在同一画布上反复获取），
 * 可通过 getImageData 为所有上下文指定统一的读像素实现（如返回特定颜色）。
 * 返回所有创建过的上下文，测试可读取/覆写。
 */
export function installCanvasStub(getImageData?: (x: number, y: number, w: number, h: number) => { data: Uint8ClampedArray; width: number; height: number }): FakeCanvasContext[] {
	const contexts: FakeCanvasContext[] = [];
	(window as any).HTMLCanvasElement.prototype.getContext = vi.fn(function (this: HTMLCanvasElement) {
		const ctx = {
			drawImage: vi.fn(),
			clearRect: vi.fn(),
			strokeRect: vi.fn(),
			putImageData: vi.fn(),
			getImageData: vi.fn(getImageData ?? ((x: number, y: number, w: number, h: number) => ({
				data: new Uint8ClampedArray(w * h * 4),
				width: w,
				height: h,
			}))),
			createImageData: (w: number, h: number) => ({
				data: new Uint8ClampedArray(w * h * 4),
				width: w,
				height: h,
			}),
			imageSmoothingEnabled: true,
			strokeStyle: '',
			lineWidth: 1,
		};
		contexts.push(ctx as unknown as FakeCanvasContext);
		return ctx;
	});
	return contexts;
}

/// jsdom 未实现 setPointerCapture/releasePointerCapture，拖拽拾取组件会调用它们
export function installPointerCaptureStub() {
	(window as any).Element.prototype.setPointerCapture = vi.fn();
	(window as any).Element.prototype.releasePointerCapture = vi.fn();
}

const ORIGINAL_SET_ATTRIBUTE = (window as any).Element.prototype.setAttribute;
const CSS_TEXT_DESC = Object.getOwnPropertyDescriptor(
	(window as any).CSSStyleDeclaration.prototype,
	'cssText'
);
const MODERN_CSS_RE = /\bmin\(|\bmax\(|\bclamp\(/;

/**
 * jsdom 的 cssstyle 无法解析 min()/max()/clamp() 等现代 CSS 函数，
 * Svelte 5 经 style.cssText 设置内联样式时会抛 SyntaxError 中断渲染。
 * 这些内联样式仅为视觉定位，测试中直接跳过设置即可。
 */
export function installTolerantStyleAttrStub() {
	(window as any).Element.prototype.setAttribute = function setAttribute(name: string, value: unknown) {
		if (name === 'style' && MODERN_CSS_RE.test(String(value))) {
			return;
		}
		return ORIGINAL_SET_ATTRIBUTE.call(this, name, value);
	};
	Object.defineProperty((window as any).CSSStyleDeclaration.prototype, 'cssText', {
		configurable: true,
		get: CSS_TEXT_DESC!.get,
		set(value: string) {
			if (MODERN_CSS_RE.test(String(value))) return;
			CSS_TEXT_DESC!.set!.call(this, value);
		},
	});
}

export function uninstallTolerantStyleAttrStub() {
	(window as any).Element.prototype.setAttribute = ORIGINAL_SET_ATTRIBUTE;
	Object.defineProperty((window as any).CSSStyleDeclaration.prototype, 'cssText', CSS_TEXT_DESC!);
}

/// 安装 navigator.clipboard stub（jsdom 未实现），返回已写入的文本
export function installClipboardStub() {
	const written: string[] = [];
	let nextRead = '';
	Object.defineProperty(navigator, 'clipboard', {
		configurable: true,
		value: {
			writeText: vi.fn(async (text: string) => {
				if ((navigator.clipboard.writeText as any).__reject) throw new Error('clipboard blocked');
				written.push(text);
			}),
			readText: vi.fn(async () => nextRead),
		},
	});
	return {
		written,
		setNextRead(text: string) { nextRead = text; },
		rejectWrites() { (navigator.clipboard.writeText as any).__reject = true; },
	};
}

/// 安装 URL.createObjectURL/revokeObjectURL stub（logs 导出下载用）
export function installObjectURLStub() {
	const create = vi.fn(() => `blob:mock-${create.mock.calls.length}`);
	const revoke = vi.fn();
	(URL as any).createObjectURL = create;
	(URL as any).revokeObjectURL = revoke;
	return { create, revoke };
}

export const INVOKE_KEY = '__TAURI_INVOKE__';

export function setInvokeMock(handler: (cmd: string, args?: Record<string, unknown>) => unknown) {
	(window as any)[INVOKE_KEY] = vi.fn(handler);
	return (window as any)[INVOKE_KEY] as ReturnType<typeof vi.fn>;
}

export function deleteInvokeMock() {
	delete (window as any)[INVOKE_KEY];
}

/**
 * 等待 ScreenPickerModal（或内嵌它的组件）截图渲染完成，把 <img> 置为已加载
 * 并触发 load 事件使组件 prepareCanvas 进入 ready 状态。
 * 默认按容器 128x72 显示 1280x720 截图（1px 容器 = 10px 图像坐标）。
 */
export async function readyPickerModal(rectWidth = 128, rectHeight = 72) {
	await waitFor(() => {
		expect(screen.getByRole('img', { name: '屏幕截图' })).toBeInTheDocument();
	});
	const img = screen.getByRole('img', { name: '屏幕截图' }) as HTMLImageElement;
	makeImgLoaded(img, 1280, 720);
	img.decode = () => Promise.resolve();
	stubRect(img, 0, 0, rectWidth, rectHeight);
	await fireEvent.load(img);
	await new Promise(r => setTimeout(r, 0));
	await new Promise(r => setTimeout(r, 0));
	return img;
}
