<script module lang="ts">
	import type { ScreenRegion } from '$lib/stores/screen';

	interface Point { x: number; y: number; }

	export interface PickerResult {
		region?: ScreenRegion;
		color?: string;
		position?: Point;
	}
</script>

<script lang="ts">
	import { onMount } from 'svelte';
	import { screen, type Screenshot } from '$lib/stores/screen';

	type PickerMode = 'region' | 'color';

	let {
		mode = 'region' as PickerMode,
		onconfirm,
		onclose,
	}: {
		mode?: PickerMode;
		onconfirm: (result: PickerResult) => void;
		onclose: () => void;
	} = $props();

	let screenshot = $state<Screenshot | null>(null);
	let loading = $state(false);
	let error = $state('');
	let ready = $state(false);

	// 区域拖拽状态（图片像素坐标）
	let dragStart = $state<Point | null>(null);
	let dragCurrent = $state<Point | null>(null);
	let selectedRegion = $state<ScreenRegion | null>(null);

	// 取色状态
	let hoverPos = $state<Point | null>(null);
	let pickedColor = $state<string | null>(null);
	let pickedPos = $state<Point | null>(null);

	let containerEl = $state<HTMLDivElement | null>(null);
	let imgEl = $state<HTMLImageElement | null>(null);
	let offscreen = $state<HTMLCanvasElement | null>(null);
	let magnifierEl = $state<HTMLCanvasElement | null>(null);

	const MAG_PIXELS = 9;
	const MAG_CELL = 15;
	const MAG_SIZE = MAG_PIXELS * MAG_CELL;

	onMount(() => {
		void refresh();
	});

	async function refresh() {
		loading = true;
		error = '';
		ready = false;
		selectedRegion = null;
		pickedColor = null;
		pickedPos = null;
		const shot = await screen.capture({ x: 0, y: 0, width: 0, height: 0 }, null);
		loading = false;
		if (!shot) {
			error = '截图失败，请确认 runtime 已连接';
			return;
		}
		screenshot = shot;
	}

	/// 截图加载完成后绘制到离屏 canvas（用于读像素）
	async function prepareCanvas() {
		if (!imgEl || !screenshot) return;
		try {
			await imgEl.decode();
		} catch {
			// decode 不可用时依赖 load 事件已完成
		}
		const canvas = document.createElement('canvas');
		canvas.width = screenshot.width;
		canvas.height = screenshot.height;
		const ctx = canvas.getContext('2d', { willReadFrequently: true });
		if (!ctx) {
			error = '无法创建像素画布';
			return;
		}
		ctx.drawImage(imgEl, 0, 0);
		offscreen = canvas;
		ready = true;
	}

	function eventToImage(e: PointerEvent): Point | null {
		if (!imgEl || !screenshot) return null;
		const rect = imgEl.getBoundingClientRect();
		if (rect.width === 0 || rect.height === 0) return null;
		const x = ((e.clientX - rect.left) / rect.width) * screenshot.width;
		const y = ((e.clientY - rect.top) / rect.height) * screenshot.height;
		return {
			x: Math.min(screenshot.width - 1, Math.max(0, Math.round(x))),
			y: Math.min(screenshot.height - 1, Math.max(0, Math.round(y))),
		};
	}

	function imageToScreenRegion(a: Point, b: Point): ScreenRegion | null {
		if (!screenshot) return null;
		const x = Math.min(a.x, b.x);
		const y = Math.min(a.y, b.y);
		const width = Math.abs(a.x - b.x);
		const height = Math.abs(a.y - b.y);
		if (width < 2 || height < 2) return null;
		return {
			x: screenshot.region.x + x,
			y: screenshot.region.y + y,
			width,
			height,
		};
	}

	function pixelToPercent(p: Point): { left: number; top: number } {
		if (!screenshot) return { left: 0, top: 0 };
		return {
			left: (p.x / screenshot.width) * 100,
			top: (p.y / screenshot.height) * 100,
		};
	}

	function readPixel(p: Point): string | null {
		if (!offscreen) return null;
		const ctx = offscreen.getContext('2d', { willReadFrequently: true });
		if (!ctx) return null;
		const data = ctx.getImageData(p.x, p.y, 1, 1).data;
		return `#${[data[0], data[1], data[2]].map(v => v.toString(16).padStart(2, '0')).join('')}`;
	}

	function handlePointerDown(e: PointerEvent) {
		if (!ready || !screenshot) return;
		const p = eventToImage(e);
		if (!p) return;
		containerEl?.setPointerCapture(e.pointerId);
		if (mode === 'region') {
			dragStart = p;
			dragCurrent = p;
			selectedRegion = null;
		} else {
			pickedColor = readPixel(p);
			pickedPos = p;
		}
	}

	function handlePointerMove(e: PointerEvent) {
		if (!ready || !screenshot) return;
		const p = eventToImage(e);
		if (!p) return;
		if (mode === 'color') {
			hoverPos = p;
			drawMagnifier();
		} else if (dragStart) {
			dragCurrent = p;
		}
	}

	function handlePointerUp(e: PointerEvent) {
		if (mode !== 'region' || !dragStart || !dragCurrent) return;
		selectedRegion = imageToScreenRegion(dragStart, dragCurrent);
		dragStart = null;
		dragCurrent = null;
	}

	function drawMagnifier() {
		if (!offscreen || !magnifierEl || !hoverPos || !screenshot) return;
		const ctx = magnifierEl.getContext('2d');
		if (!ctx) return;
		ctx.imageSmoothingEnabled = false;
		ctx.clearRect(0, 0, MAG_SIZE, MAG_SIZE);
		const sx = hoverPos.x - Math.floor(MAG_PIXELS / 2);
		const sy = hoverPos.y - Math.floor(MAG_PIXELS / 2);
		ctx.drawImage(offscreen, sx, sy, MAG_PIXELS, MAG_PIXELS, 0, 0, MAG_SIZE, MAG_SIZE);
		// 中心格描边
		ctx.strokeStyle = '#ffffff';
		ctx.lineWidth = 2;
		ctx.strokeRect(
			Math.floor(MAG_PIXELS / 2) * MAG_CELL + 1,
			Math.floor(MAG_PIXELS / 2) * MAG_CELL + 1,
			MAG_CELL - 2,
			MAG_CELL - 2
		);
		ctx.strokeStyle = '#000000';
		ctx.lineWidth = 1;
		ctx.strokeRect(
			Math.floor(MAG_PIXELS / 2) * MAG_CELL + 2.5,
			Math.floor(MAG_PIXELS / 2) * MAG_CELL + 2.5,
			MAG_CELL - 5,
			MAG_CELL - 5
		);
	}

	function confirm() {
		if (mode === 'region') {
			if (!selectedRegion) return;
			onconfirm({ region: selectedRegion });
		} else {
			if (!pickedColor || !pickedPos) return;
			const result: PickerResult = { color: pickedColor };
			if (screenshot) {
				result.position = {
					x: screenshot.region.x + pickedPos.x,
					y: screenshot.region.y + pickedPos.y,
				};
			}
			onconfirm(result);
		}
	}

	function handleKeydown(e: KeyboardEvent) {
		if (e.key === 'Escape') {
			e.preventDefault();
			onclose();
		} else if (e.key === 'Enter') {
			e.preventDefault();
			confirm();
		}
	}

	// 选区（百分比定位）
	const selectionStyle = $derived.by(() => {
		if (mode !== 'region' || !dragStart || !dragCurrent || !screenshot) return '';
		const a = dragStart;
		const b = dragCurrent;
		return [
			`left: ${(Math.min(a.x, b.x) / screenshot.width) * 100}%`,
			`top: ${(Math.min(a.y, b.y) / screenshot.height) * 100}%`,
			`width: ${(Math.abs(a.x - b.x) / screenshot.width) * 100}%`,
			`height: ${(Math.abs(a.y - b.y) / screenshot.height) * 100}%`,
		].join('; ');
	});

	const fixedSelectionStyle = $derived.by(() => {
		if (mode !== 'region' || !selectedRegion || !screenshot) return '';
		const x = selectedRegion.x - screenshot.region.x;
		const y = selectedRegion.y - screenshot.region.y;
		return [
			`left: ${(x / screenshot.width) * 100}%`,
			`top: ${(y / screenshot.height) * 100}%`,
			`width: ${(selectedRegion.width / screenshot.width) * 100}%`,
			`height: ${(selectedRegion.height / screenshot.height) * 100}%`,
		].join('; ');
	});

	const hoverCrossStyle = $derived.by(() => {
		if (!hoverPos || !screenshot) return null;
		const { left, top } = pixelToPercent(hoverPos);
		return { left: `${left}%`, top: `${top}%` };
	});

	const pickedMarkStyle = $derived.by(() => {
		if (!pickedPos || !screenshot) return null;
		const { left, top } = pixelToPercent(pickedPos);
		return { left: `${left}%`, top: `${top}%` };
	});

	const dragSizeLabel = $derived.by(() => {
		if (!dragStart || !dragCurrent) return '';
		return `${Math.abs(dragStart.x - dragCurrent.x)}×${Math.abs(dragStart.y - dragCurrent.y)}`;
	});

	const hoverColor = $derived.by(() => {
		if (!hoverPos || !offscreen) return null;
		return readPixel(hoverPos);
	});

	const canConfirm = $derived(
		mode === 'region' ? !!selectedRegion : !!pickedColor
	);

	const hintText = $derived(
		mode === 'region'
			? '在截图上按住鼠标拖拽选择区域，松开后可调整或重新拖拽'
			: '移动鼠标查看放大像素，点击拾取颜色'
	);
</script>

<svelte:window onkeydown={handleKeydown} />

<div class="picker-overlay" role="dialog" aria-modal="true">
	<div class="picker-modal">
		<div class="picker-header">
			<span class="picker-title">
				{mode === 'region' ? '拾取屏幕区域' : '拾取屏幕颜色'}
			</span>
			<button class="close-btn" onclick={onclose} title="关闭 (Esc)">✕</button>
		</div>

		<div class="picker-body">
			{#if loading}
				<div class="picker-status">正在捕获屏幕...</div>
			{:else if error}
				<div class="picker-status error">{error}</div>
				<button class="btn" onclick={() => void refresh()}>重新截图</button>
			{:else if screenshot}
				<div
					class="image-container"
					role="img"
					aria-label={mode === 'region' ? '屏幕截图，拖拽选择区域' : '屏幕截图，点击拾取颜色'}
					bind:this={containerEl}
					class:picking-region={mode === 'region'}
					class:picking-color={mode === 'color'}
					onpointerdown={handlePointerDown}
					onpointermove={handlePointerMove}
					onpointerup={handlePointerUp}
				>
					<img
						bind:this={imgEl}
						src={screenshot.image}
						alt="屏幕截图"
						draggable="false"
						onload={() => void prepareCanvas()}
					/>

					{#if mode === 'region'}
						{#if selectionStyle}
							<div class="sel-rect dragging" style={selectionStyle}>
								<span class="sel-size">{dragSizeLabel}</span>
							</div>
						{/if}
						{#if fixedSelectionStyle && !dragStart}
							<div class="sel-rect" style={fixedSelectionStyle}>
								<span class="sel-label">
									{selectedRegion?.x},{selectedRegion?.y} · {selectedRegion?.width}×{selectedRegion?.height}
								</span>
							</div>
						{/if}
					{/if}

					{#if mode === 'color' && hoverCrossStyle && ready}
						<div class="crosshair-v" style={`left: ${hoverCrossStyle.left}`}></div>
						<div class="crosshair-h" style={`top: ${hoverCrossStyle.top}`}></div>
						<canvas
							class="magnifier"
							bind:this={magnifierEl}
							width={MAG_SIZE}
							height={MAG_SIZE}
							style={`left: min(calc(${hoverCrossStyle.left}% + 24px), calc(100% - ${MAG_SIZE + 16}px)); top: min(${hoverCrossStyle.top}%, calc(100% - ${MAG_SIZE + 16}px));`}
						></canvas>
						<div
							class="hover-readout"
							style={`left: min(calc(${hoverCrossStyle.left}% + ${MAG_SIZE + 32}px), calc(100% - 200px)); top: ${hoverCrossStyle.top};`}
						>
							{#if hoverPos && screenshot}
								<span class="readout-coord">{screenshot.region.x + hoverPos.x},{screenshot.region.y + hoverPos.y}</span>
							{/if}
							{#if hoverColor}
								<span class="readout-swatch" style={`background: ${hoverColor}`}></span>
								<span class="readout-hex">{hoverColor}</span>
							{/if}
						</div>
						{#if pickedMarkStyle}
							<div class="picked-mark" style={`left: ${pickedMarkStyle.left}; top: ${pickedMarkStyle.top};`}></div>
						{/if}
					{/if}
				</div>
			{/if}
		</div>

		<div class="picker-footer">
			<div class="footer-info">
				<span class="hint">{hintText}</span>
				{#if mode === 'color' && pickedColor}
					<span class="picked-preview">
						<span class="readout-swatch large" style={`background: ${pickedColor}`}></span>
						<span class="readout-hex">{pickedColor}</span>
						{#if pickedPos && screenshot}
							<span class="readout-coord">@ {screenshot.region.x + pickedPos.x},{screenshot.region.y + pickedPos.y}</span>
						{/if}
					</span>
				{/if}
			</div>
			<div class="footer-actions">
				<button class="btn" onclick={() => void refresh()} disabled={loading}>重新截图</button>
				<button class="btn" onclick={onclose}>取消</button>
				<button class="btn btn-primary" onclick={confirm} disabled={!canConfirm}>
					确定
				</button>
			</div>
		</div>
	</div>
</div>

<style>
	.picker-overlay {
		position: fixed;
		inset: 0;
		background: rgba(0, 0, 0, 0.6);
		display: flex;
		align-items: center;
		justify-content: center;
		z-index: 200;
	}

	.picker-modal {
		display: flex;
		flex-direction: column;
		width: min(1100px, calc(100vw - 48px));
		max-height: calc(100vh - 48px);
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 10px;
		box-shadow: 0 24px 60px var(--shadow-color);
		overflow: hidden;
	}

	.picker-header {
		display: flex;
		align-items: center;
		justify-content: space-between;
		padding: 12px 16px;
		border-bottom: 1px solid var(--border-color);
	}

	.picker-title { font-size: 15px; font-weight: 600; color: var(--text-primary); }

	.close-btn {
		background: transparent;
		border: none;
		color: var(--text-secondary);
		font-size: 16px;
		cursor: pointer;
		padding: 4px 8px;
		border-radius: 4px;
	}
	.close-btn:hover { background: var(--bg-tertiary); color: var(--text-primary); }

	.picker-body {
		flex: 1;
		min-height: 0;
		display: flex;
		align-items: center;
		justify-content: center;
		padding: 16px;
		overflow: auto;
		background: var(--bg-primary);
	}

	.picker-status { color: var(--text-secondary); font-size: 14px; padding: 60px 0; }
	.picker-status.error { color: var(--accent-red); }

	.image-container {
		position: relative;
		line-height: 0;
		max-width: 100%;
		user-select: none;
		touch-action: none;
	}

	.image-container.picking-region { cursor: crosshair; }
	.image-container.picking-color { cursor: none; }

	.image-container img {
		max-width: 100%;
		max-height: calc(100vh - 240px);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		pointer-events: none;
	}

	.sel-rect {
		position: absolute;
		border: 1px solid var(--accent-blue);
		background: rgba(88, 166, 255, 0.12);
		box-shadow: 0 0 0 9999px rgba(0, 0, 0, 0.5);
		pointer-events: none;
	}

	.sel-rect .sel-size,
	.sel-rect .sel-label {
		position: absolute;
		top: -24px;
		left: 0;
		font-size: 11px;
		line-height: 1.4;
		color: #fff;
		background: var(--accent-blue);
		padding: 1px 6px;
		border-radius: 3px;
		white-space: nowrap;
	}

	.crosshair-v,
	.crosshair-h {
		position: absolute;
		background: rgba(255, 255, 255, 0.85);
		mix-blend-mode: difference;
		pointer-events: none;
	}

	.crosshair-v { top: 0; bottom: 0; width: 1px; }
	.crosshair-h { left: 0; right: 0; height: 1px; }

	.magnifier {
		position: absolute;
		border: 1px solid var(--border-color);
		border-radius: 4px;
		background: #000;
		box-shadow: 0 8px 20px rgba(0, 0, 0, 0.5);
		pointer-events: none;
		image-rendering: pixelated;
		z-index: 5;
	}

	.hover-readout {
		position: absolute;
		display: flex;
		align-items: center;
		gap: 6px;
		padding: 3px 8px;
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 4px;
		font-size: 11px;
		font-family: monospace;
		color: var(--text-primary);
		pointer-events: none;
		z-index: 6;
		transform: translateY(-50%);
	}

	.readout-swatch {
		width: 12px;
		height: 12px;
		border-radius: 2px;
		border: 1px solid var(--border-color);
		flex-shrink: 0;
	}

	.readout-swatch.large { width: 18px; height: 18px; }

	.readout-hex { color: var(--text-primary); }
	.readout-coord { color: var(--text-secondary); }

	.picked-mark {
		position: absolute;
		width: 14px;
		height: 14px;
		border: 2px solid #fff;
		outline: 1px solid #000;
		border-radius: 50%;
		transform: translate(-50%, -50%);
		pointer-events: none;
		z-index: 4;
	}

	.picker-footer {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 12px;
		padding: 12px 16px;
		border-top: 1px solid var(--border-color);
	}

	.footer-info {
		display: flex;
		align-items: center;
		gap: 12px;
		min-width: 0;
		flex-wrap: wrap;
	}

	.hint { font-size: 12px; color: var(--text-secondary); }

	.picked-preview {
		display: flex;
		align-items: center;
		gap: 6px;
		font-size: 12px;
		font-family: monospace;
		color: var(--text-primary);
	}

	.footer-actions { display: flex; gap: 8px; flex-shrink: 0; }

	.btn {
		padding: 7px 14px;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		font-size: 13px;
		cursor: pointer;
		transition: all 0.15s;
	}
	.btn:disabled { opacity: 0.5; cursor: not-allowed; }
	.btn:hover:not(:disabled) { background: var(--surface-hover, var(--border-color)); }

	.btn-primary {
		background: var(--accent-blue);
		color: white;
		border-color: var(--accent-blue);
	}
	.btn-primary:hover:not(:disabled) { background: var(--accent-blue-hover, #388bfd); }
</style>
