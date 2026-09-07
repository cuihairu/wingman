<script lang="ts">
	import RegionPicker from '$lib/components/RegionPicker.svelte';
	import ColorPicker from '$lib/components/ColorPicker.svelte';
	import { connection } from '$lib/stores/connection';
	import { logs } from '$lib/stores/logs';
	import { DEFAULT_REGION, screen, type ScreenRegion } from '$lib/stores/screen';

	const PREVIEW_REGION: ScreenRegion = { x: 0, y: 0, width: 0, height: 0 };
	const DRAG_THRESHOLD = 4;
	const MAG_PIXELS = 9;
	const MAG_CELL = 12;
	const MAG_SIZE = MAG_PIXELS * MAG_CELL;

	interface Point { x: number; y: number; }

	let region = $state<ScreenRegion>({ ...DEFAULT_REGION });
	let targetColor = $state('#3fb950');
	let tolerance = $state(12);
	// null = primary (backwards compatible); number = monitor index from listMonitors
	let selectedDisplayId = $state<number | null>(null);
	let autoRefresh = $state(false);

	let imgEl = $state<HTMLImageElement | null>(null);
	let offscreen = $state<HTMLCanvasElement | null>(null);
	let magnifierEl = $state<HTMLCanvasElement | null>(null);
	let matchEl = $state<HTMLCanvasElement | null>(null);

	/// 悬停读数（图片坐标 + 真实像素色）
	let hover = $state<(Point & { color: string | null }) | null>(null);
	/// 点击拾取的坐标点（图片坐标 + 真实像素色）
	let pointer = $state<(Point & { color: string | null }) | null>(null);
	/// 拖拽选区状态（图片坐标）
	let dragSel = $state<{ start: Point; cur: Point } | null>(null);
	let matchEnabled = $state(false);
	let matchCount = $state(0);

	const hasImage = $derived(!!$screen.current && !!offscreen);

	function rgbToHex(r: number, g: number, b: number): string {
		return `#${[r, g, b].map(v => v.toString(16).padStart(2, '0')).join('')}`;
	}

	function parseHexColor(value: string): { r: number; g: number; b: number } | null {
		const m = /^#?([0-9a-fA-F]{6})$/.exec(value.trim());
		if (!m) return null;
		const num = parseInt(m[1], 16);
		return { r: (num >> 16) & 0xff, g: (num >> 8) & 0xff, b: num & 0xff };
	}

	function readPixel(p: Point): string | null {
		if (!offscreen) return null;
		const shot = $screen.current;
		if (!shot || p.x < 0 || p.y < 0 || p.x >= shot.width || p.y >= shot.height) return null;
		const ctx = offscreen.getContext('2d', { willReadFrequently: true });
		if (!ctx) return null;
		const d = ctx.getImageData(p.x, p.y, 1, 1).data;
		return rgbToHex(d[0], d[1], d[2]);
	}

	/// 截图加载完成后重建离屏画布（读像素 / 高亮 / 放大镜共用）
	function prepareCanvas() {
		const shot = $screen.current;
		const img = imgEl;
		if (!shot || !img) return;
		const draw = () => {
			if (img.naturalWidth === 0) return;
			const canvas = document.createElement('canvas');
			canvas.width = shot.width;
			canvas.height = shot.height;
			const ctx = canvas.getContext('2d', { willReadFrequently: true });
			if (!ctx) return;
			ctx.drawImage(img, 0, 0);
			offscreen = canvas;
		};
		if (img.complete && img.naturalWidth > 0) {
			draw();
		} else {
			img.onload = draw;
		}
	}

	function eventToImage(e: PointerEvent): Point | null {
		const shot = $screen.current;
		if (!shot || !imgEl) return null;
		const rect = imgEl.getBoundingClientRect();
		if (rect.width === 0 || rect.height === 0) return null;
		const x = Math.round(((e.clientX - rect.left) / rect.width) * shot.width);
		const y = Math.round(((e.clientY - rect.top) / rect.height) * shot.height);
		return {
			x: Math.min(shot.width - 1, Math.max(0, x)),
			y: Math.min(shot.height - 1, Math.max(0, y)),
		};
	}

	/// 图片坐标 → 屏幕绝对坐标（触发器/脚本使用绝对坐标）
	function toScreen(p: Point): Point {
		const shot = $screen.current;
		if (!shot) return p;
		return { x: shot.region.x + p.x, y: shot.region.y + p.y };
	}

	function handlePointerDown(e: PointerEvent) {
		if (!hasImage) return;
		const p = eventToImage(e);
		if (!p) return;
		(e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
		dragSel = { start: p, cur: p };
	}

	function handlePointerMove(e: PointerEvent) {
		if (!hasImage) return;
		const p = eventToImage(e);
		if (!p) return;
		if (dragSel) {
			dragSel = { ...dragSel, cur: p };
		}
		hover = { ...p, color: readPixel(p) };
	}

	function handlePointerUp() {
		if (!dragSel) return;
		const { start, cur } = dragSel;
		const moved = Math.abs(start.x - cur.x) > DRAG_THRESHOLD || Math.abs(start.y - cur.y) > DRAG_THRESHOLD;
		if (moved) {
			const shot = $screen.current;
			if (shot) {
				const x = Math.min(start.x, cur.x);
				const y = Math.min(start.y, cur.y);
				region = {
					x: shot.region.x + x,
					y: shot.region.y + y,
					width: Math.abs(start.x - cur.x),
					height: Math.abs(start.y - cur.y),
				};
			}
		} else {
			pointer = { ...cur, color: readPixel(cur) };
		}
		dragSel = null;
	}

	function handlePointerLeave() {
		hover = null;
	}

	function drawMagnifier() {
		if (!offscreen || !magnifierEl || !hover) return;
		const ctx = magnifierEl.getContext('2d');
		if (!ctx) return;
		ctx.imageSmoothingEnabled = false;
		ctx.clearRect(0, 0, MAG_SIZE, MAG_SIZE);
		const sx = hover.x - Math.floor(MAG_PIXELS / 2);
		const sy = hover.y - Math.floor(MAG_PIXELS / 2);
		ctx.drawImage(offscreen, sx, sy, MAG_PIXELS, MAG_PIXELS, 0, 0, MAG_SIZE, MAG_SIZE);
		ctx.strokeStyle = '#ffffff';
		ctx.lineWidth = 2;
		const c = Math.floor(MAG_PIXELS / 2) * MAG_CELL;
		ctx.strokeRect(c + 1, c + 1, MAG_CELL - 2, MAG_CELL - 2);
		ctx.strokeStyle = '#000000';
		ctx.lineWidth = 1;
		ctx.strokeRect(c + 2.5, c + 2.5, MAG_CELL - 5, MAG_CELL - 5);
	}

	/// 颜色匹配高亮：与 runtime Color::matches 一致（欧氏距离平方 ≤ tolerance²）
	$effect(() => {
		if (!matchEnabled || !offscreen || !matchEl) {
			matchCount = 0;
			return;
		}
		const target = parseHexColor(targetColor);
		const shot = $screen.current;
		if (!target || !shot) {
			matchCount = 0;
			return;
		}
		const ctx = offscreen.getContext('2d', { willReadFrequently: true });
		if (!ctx) return;
		const src = ctx.getImageData(0, 0, shot.width, shot.height);
		const out = ctx.createImageData(shot.width, shot.height);
		const sd = src.data;
		const od = out.data;
		const t2 = tolerance * tolerance;
		let count = 0;
		for (let i = 0; i < sd.length; i += 4) {
			const dr = sd[i] - target.r;
			const dg = sd[i + 1] - target.g;
			const db = sd[i + 2] - target.b;
			if (dr * dr + dg * dg + db * db <= t2) {
				od[i] = 255;
				od[i + 1] = 0;
				od[i + 2] = 255;
				od[i + 3] = 120;
				count++;
			}
		}
		matchCount = count;
		matchEl.width = shot.width;
		matchEl.height = shot.height;
		matchEl.getContext('2d')?.putImageData(out, 0, 0);
	});

	/// 悬停放大镜重绘
	$effect(() => {
		if (hover) drawMagnifier();
	});

	// region（屏幕绝对坐标）→ 预览图上的百分比定位
	const overlayStyle = $derived.by(() => {
		const shot = $screen.current;
		if (!shot || shot.width <= 0 || shot.height <= 0) return 'display: none;';
		const x = region.x - shot.region.x;
		const y = region.y - shot.region.y;
		if (region.width <= 0 || region.height <= 0) return 'display: none;';
		return [
			`left:${Math.max(0, (x / shot.width) * 100)}%`,
			`top:${Math.max(0, (y / shot.height) * 100)}%`,
			`width:${(region.width / shot.width) * 100}%`,
			`height:${(region.height / shot.height) * 100}%`,
		].join(';');
	});

	/// 拖拽中的临时选框
	const dragStyle = $derived.by(() => {
		const shot = $screen.current;
		if (!dragSel || !shot) return '';
		const { start, cur } = dragSel;
		return [
			`left:${(Math.min(start.x, cur.x) / shot.width) * 100}%`,
			`top:${(Math.min(start.y, cur.y) / shot.height) * 100}%`,
			`width:${(Math.abs(start.x - cur.x) / shot.width) * 100}%`,
			`height:${(Math.abs(start.y - cur.y) / shot.height) * 100}%`,
		].join(';');
	});

	const pointerStyle = $derived.by(() => {
		const shot = $screen.current;
		if (!shot || !pointer) return 'display: none;';
		return `left:${(pointer.x / shot.width) * 100}%;top:${(pointer.y / shot.height) * 100}%;`;
	});

	const hoverStyle = $derived.by(() => {
		const shot = $screen.current;
		if (!shot || !hover) return '';
		const left = (hover.x / shot.width) * 100;
		const top = (hover.y / shot.height) * 100;
		return [
			`left: min(calc(${left}% + 18px), calc(100% - 230px))`,
			`top: min(calc(${top}% + 18px), calc(100% - 56px))`,
		].join(';');
	});

	const screenPointer = $derived(pointer ? toScreen(pointer) : null);
	const screenHover = $derived(hover ? toScreen(hover) : null);

	async function capture(showLog = false) {
		const result = await screen.capture(PREVIEW_REGION, selectedDisplayId);
		if (result && showLog) {
			logs.add(`已刷新屏幕预览: ${result.width}x${result.height}`, 'success');
		} else if (!result && $screen.error) {
			logs.add(`屏幕预览刷新失败: ${$screen.error}`, 'error');
		}
	}

	function fitFullImage() {
		const shot = $screen.current;
		if (!shot) return;
		region = { ...shot.region };
	}

	function selectPointAsRegion() {
		if (!pointer) return;
		const width = Math.max(64, region.width || 240);
		const height = Math.max(64, region.height || 160);
		region = {
			x: Math.max(0, pointer.x - Math.round(width / 2)),
			y: Math.max(0, pointer.y - Math.round(height / 2)),
			width,
			height,
		};
	}

	/// 用拾取到的真实颜色更新 ColorPicker
	function usePickedColor() {
		if (pointer?.color) {
			targetColor = pointer.color;
		}
	}

	function copyPickedColor() {
		if (pointer?.color) {
			void copyText(pointer.color, '颜色');
		}
	}

	async function copyText(text: string, label: string) {
		try {
			await navigator.clipboard.writeText(text);
			logs.add(`已复制${label}: ${text}`, 'info');
		} catch {
			logs.add('复制失败（剪贴板不可用）', 'warning');
		}
	}

	$effect(() => {
		// Load monitor list on mount / when connection state changes.
		screen.listMonitors().then(monitors => {
			// If the previously selected id is no longer valid, reset to primary.
			if (selectedDisplayId !== null && !monitors.some(m => m.id === selectedDisplayId)) {
				selectedDisplayId = null;
			}
		});
	});

	$effect(() => {
		if (!$screen.current) {
			capture();
		}
	});

	$effect(() => {
		if (!autoRefresh) return;
		const interval = window.setInterval(() => capture(), 1500);
		return () => window.clearInterval(interval);
	});
</script>

<div class="screen-page">
	<section class="page-header">
		<div>
			<h2 class="page-title">屏幕预览</h2>
			<p class="page-subtitle">截图预览、拖拽框选区域、坐标与真实像素取色、颜色匹配高亮</p>
		</div>
		<div class="header-actions">
			<label class="toggle">
				<input type="checkbox" bind:checked={autoRefresh} />
				<span>实时刷新</span>
			</label>
			<button class="btn btn-primary" onclick={() => capture(true)} disabled={$screen.loading}>
				{$screen.loading ? '刷新中' : '刷新截图'}
			</button>
		</div>
	</section>

	<div class="screen-layout">
		<section class="preview-card">
			<div class="preview-toolbar">
				<div>
					<strong>画面</strong>
					<span>
						{$screen.current ? `${$screen.current.width}x${$screen.current.height} @ ${$screen.current.region.x},${$screen.current.region.y}` : '等待截图'}
						· {$screen.lastUpdated}
					</span>
				</div>
				<div class="preview-status" class:connected={$connection.connected}>
					{$connection.connected ? 'runtime IPC' : 'dev fallback'}
				</div>
			</div>

			<div class="preview-stage">
				{#if $screen.current}
					<!-- svelte-ignore a11y_no_static_element_interactions -->
					<div
						class="image-frame"
						role="img"
						aria-label="屏幕截图预览：拖拽框选区域，单击拾取坐标与颜色"
						onpointerdown={handlePointerDown}
						onpointermove={handlePointerMove}
						onpointerup={handlePointerUp}
						onpointerleave={handlePointerLeave}
					>
						<img
							bind:this={imgEl}
							src={$screen.current.image}
							alt="屏幕截图"
							draggable="false"
							onload={prepareCanvas}
						/>
						{#if matchEnabled}
							<canvas class="match-layer" bind:this={matchEl}></canvas>
						{/if}
						<div class="region-overlay" style={overlayStyle}>
							<span>{region.x},{region.y} · {region.width}x{region.height}</span>
						</div>
						{#if dragSel}
							<div class="drag-overlay" style={dragStyle}>
								<span>{Math.abs(dragSel.start.x - dragSel.cur.x)}×{Math.abs(dragSel.start.y - dragSel.cur.y)}</span>
							</div>
						{/if}
						{#if pointer}
							<div class="pointer" style={pointerStyle}></div>
						{/if}
						{#if hover && hasImage}
							<canvas
								class="magnifier"
								bind:this={magnifierEl}
								width={MAG_SIZE}
								height={MAG_SIZE}
							></canvas>
							<div class="hover-readout" style={hoverStyle}>
								<span class="coord">{screenHover?.x},{screenHover?.y}</span>
								{#if hover.color}
									<span class="swatch" style={`background: ${hover.color}`}></span>
									<span class="hex">{hover.color}</span>
								{/if}
							</div>
						{/if}
					</div>
				{:else}
					<div class="empty-preview">
						<span>尚无截图</span>
						<button class="btn" onclick={() => capture(true)}>立即捕获</button>
					</div>
				{/if}
			</div>

			<div class="preview-hints">
				<span>单击：拾取坐标与真实颜色</span>
				<span>拖拽：框选搜索区域（同步右侧）</span>
				<span>坐标为屏幕绝对坐标</span>
			</div>

			{#if $screen.error}
				<div class="error-panel">
					<span>{$screen.error}</span>
					<button class="btn btn-sm" onclick={() => capture(true)} disabled={$screen.loading}>
						{$screen.loading ? '重试中' : '重试'}
					</button>
				</div>
			{/if}
		</section>

		<aside class="tools-card">
			<div class="tool-section">
				<div class="section-heading">
					<span>显示器</span>
					<span class="muted">{$screen.monitors.length > 0 ? `${$screen.monitors.length} 个` : '加载中'}</span>
				</div>
				<select class="form-input" bind:value={selectedDisplayId}>
					<option value={null}>主显示器</option>
					{#each $screen.monitors as monitor (monitor.id)}
						<option value={monitor.id}>
							{monitor.name || `显示器 ${monitor.id}`}
							{monitor.isPrimary ? '（主）' : ''}
							· {monitor.bounds.width}x{monitor.bounds.height}
						</option>
					{/each}
				</select>
				{#if $screen.monitorsError}
					<div class="monitor-hint">{$screen.monitorsError}</div>
				{/if}
			</div>

			<div class="tool-section">
				<div class="section-heading">
					<span>区域</span>
					<button class="link-btn" onclick={fitFullImage} disabled={!$screen.current}>全图</button>
				</div>
				<RegionPicker bind:value={region} />
				<div class="copy-row">
					<button class="btn tool-btn" onclick={selectPointAsRegion} disabled={!pointer}>以坐标为中心</button>
					<button
						class="link-btn"
						onclick={() => copyText(`${region.x},${region.y},${region.width},${region.height}`, '区域')}
						disabled={region.width <= 0}
					>复制区域</button>
				</div>
			</div>

			<div class="tool-section">
				<div class="section-heading">
					<span>颜色</span>
					<span class="muted">用于触发器配置</span>
				</div>
				<ColorPicker bind:value={targetColor} bind:tolerance />
				<div class="color-summary">
					<span class="swatch" style="background: {targetColor}"></span>
					<code>{targetColor}</code>
					<span>容差 {tolerance}</span>
				</div>
				<label class="toggle match-toggle">
					<input type="checkbox" bind:checked={matchEnabled} disabled={!hasImage} />
					<span>在画面中高亮匹配像素</span>
				</label>
				{#if matchEnabled}
					<div class="match-result">
						{#if matchCount > 0}
							匹配 <strong>{matchCount.toLocaleString()}</strong> 个像素
						{:else}
							无匹配像素
						{/if}
					</div>
				{/if}
			</div>

			<div class="tool-section">
				<div class="section-heading">
					<span>坐标</span>
					<span class="muted">点击画面拾取</span>
				</div>
				{#if pointer && screenPointer}
					<div class="coordinate-card">
						<div class="coordinate-main">
							<strong>{screenPointer.x}, {screenPointer.y}</strong>
							<button class="link-btn" onclick={() => copyText(`${screenPointer.x},${screenPointer.y}`, '坐标')}>复制</button>
						</div>
						{#if pointer.color}
							<div class="picked-color">
								<span class="swatch" style={`background: ${pointer.color}`}></span>
								<code>{pointer.color}</code>
								<button class="link-btn" onclick={usePickedColor}>设为查找颜色</button>
								<button class="link-btn" onclick={copyPickedColor}>复制</button>
							</div>
						{/if}
						<span class="coord-note">截图内坐标 {pointer.x}, {pointer.y}</span>
					</div>
				{:else}
					<div class="empty-tool">点击截图区域以记录坐标。</div>
				{/if}
			</div>
		</aside>
	</div>
</div>

<style>
	.screen-page {
		display: flex;
		flex-direction: column;
		gap: 20px;
	}

	.page-header {
		display: flex;
		align-items: flex-start;
		justify-content: space-between;
		gap: 16px;
	}

	.page-title {
		font-size: 24px;
		font-weight: 600;
		color: var(--text-primary);
		margin-bottom: 8px;
	}

	.page-subtitle,
	.muted {
		font-size: 13px;
		color: var(--text-secondary);
	}

	.header-actions {
		display: flex;
		align-items: center;
		gap: 12px;
	}

	.screen-layout {
		display: grid;
		grid-template-columns: minmax(0, 1fr) 360px;
		gap: 16px;
		align-items: start;
	}

	.preview-card,
	.tools-card {
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 10px;
		overflow: hidden;
	}

	.preview-toolbar {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 12px;
		padding: 14px 16px;
		border-bottom: 1px solid var(--border-color);
	}

	.preview-toolbar div:first-child {
		display: flex;
		flex-direction: column;
		gap: 4px;
	}

	.preview-toolbar strong {
		font-size: 15px;
	}

	.preview-toolbar span {
		font-size: 12px;
		color: var(--text-secondary);
	}

	.preview-status {
		padding: 4px 8px;
		border-radius: 999px;
		background: rgba(210, 153, 34, 0.12);
		color: var(--accent-yellow);
		border: 1px solid rgba(210, 153, 34, 0.35);
		font-size: 12px;
	}

	.preview-status.connected {
		background: rgba(63, 185, 80, 0.1);
		color: var(--accent-green);
		border-color: rgba(63, 185, 80, 0.4);
	}

	.preview-stage {
		position: relative;
		display: grid;
		place-items: center;
		min-height: 560px;
		background:
			radial-gradient(circle at 20% 20%, rgba(88, 166, 255, 0.12), transparent 34%),
			linear-gradient(135deg, #05070b, #0d1117);
		overflow: hidden;
	}

	.image-frame {
		position: relative;
		display: inline-flex;
		max-width: 100%;
		max-height: 72vh;
		cursor: crosshair;
		touch-action: none;
		user-select: none;
	}

	.image-frame:focus-visible {
		outline: 2px solid var(--accent-blue);
		outline-offset: 4px;
	}

	.image-frame img {
		display: block;
		max-width: 100%;
		max-height: 72vh;
		object-fit: contain;
		pointer-events: none;
	}

	.match-layer {
		position: absolute;
		inset: 0;
		width: 100%;
		height: 100%;
		pointer-events: none;
		z-index: 2;
	}

	.region-overlay {
		position: absolute;
		border: 2px solid var(--accent-green);
		box-shadow: 0 0 0 9999px rgba(0, 0, 0, 0.28), 0 0 18px rgba(63, 185, 80, 0.45);
		pointer-events: none;
		z-index: 3;
	}

	.region-overlay span {
		position: absolute;
		top: -28px;
		left: 0;
		padding: 4px 8px;
		border-radius: 6px;
		background: rgba(13, 17, 23, 0.92);
		color: var(--accent-green);
		font-size: 12px;
		white-space: nowrap;
	}

	.drag-overlay {
		position: absolute;
		border: 1px dashed var(--accent-blue);
		background: rgba(88, 166, 255, 0.12);
		pointer-events: none;
		z-index: 4;
	}

	.drag-overlay span {
		position: absolute;
		top: -22px;
		left: 0;
		font-size: 11px;
		line-height: 1.4;
		color: #fff;
		background: var(--accent-blue);
		padding: 1px 6px;
		border-radius: 3px;
		white-space: nowrap;
	}

	.pointer {
		position: absolute;
		width: 18px;
		height: 18px;
		border: 2px solid var(--accent-yellow);
		border-radius: 50%;
		transform: translate(-50%, -50%);
		box-shadow: 0 0 14px rgba(210, 153, 34, 0.8);
		pointer-events: none;
		z-index: 5;
	}

	.pointer::before,
	.pointer::after {
		content: '';
		position: absolute;
		background: var(--accent-yellow);
	}

	.pointer::before {
		width: 28px;
		height: 1px;
		left: -7px;
		top: 7px;
	}

	.pointer::after {
		width: 1px;
		height: 28px;
		left: 7px;
		top: -7px;
	}

	.magnifier {
		position: absolute;
		left: 0;
		top: 0;
		border: 1px solid var(--border-color);
		border-radius: 4px;
		background: #000;
		box-shadow: 0 8px 20px rgba(0, 0, 0, 0.5);
		pointer-events: none;
		image-rendering: pixelated;
		z-index: 6;
		transform: translate(18px, 18px);
	}

	.hover-readout {
		position: absolute;
		display: flex;
		align-items: center;
		gap: 8px;
		padding: 4px 10px;
		background: rgba(13, 17, 23, 0.94);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		font-size: 12px;
		font-family: monospace;
		color: var(--text-primary);
		pointer-events: none;
		z-index: 7;
		white-space: nowrap;
	}

	.hover-readout .coord { color: var(--text-secondary); }
	.hover-readout .hex { color: var(--accent-blue); }

	.swatch {
		width: 14px;
		height: 14px;
		border-radius: 3px;
		border: 1px solid var(--border-color);
		flex-shrink: 0;
	}

	.preview-hints {
		display: flex;
		flex-wrap: wrap;
		gap: 6px 20px;
		padding: 10px 16px;
		border-top: 1px solid var(--border-color);
		font-size: 12px;
		color: var(--text-secondary);
	}

	.empty-preview,
	.empty-tool {
		display: grid;
		place-items: center;
		gap: 12px;
		min-height: 180px;
		color: var(--text-secondary);
		font-size: 13px;
	}

	.error-panel {
		margin: 12px;
		padding: 10px 12px;
		border-radius: 8px;
		color: var(--accent-red);
		background: rgba(248, 81, 73, 0.08);
		border: 1px solid rgba(248, 81, 73, 0.3);
		font-size: 13px;
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 12px;
	}

	.error-panel .btn-sm {
		min-height: 28px;
		padding: 4px 10px;
		font-size: 12px;
		border-color: rgba(248, 81, 73, 0.4);
		color: var(--accent-red);
		background: transparent;
	}

	.tools-card {
		padding: 16px;
		display: flex;
		flex-direction: column;
		gap: 18px;
	}

	.tool-section {
		padding-bottom: 18px;
		border-bottom: 1px solid var(--border-color);
	}

	.tool-section:last-child {
		padding-bottom: 0;
		border-bottom: 0;
	}

	.section-heading {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 12px;
		margin-bottom: 12px;
		font-size: 14px;
		font-weight: 600;
	}

	.form-input {
		width: 100%;
		padding: 10px 12px;
		background: var(--bg-primary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		color: var(--text-primary);
		font-size: 14px;
	}

	.monitor-hint {
		margin-top: 8px;
		font-size: 12px;
		color: var(--accent-yellow);
		line-height: 1.5;
	}

	.toggle {
		display: flex;
		align-items: center;
		gap: 8px;
		font-size: 13px;
		color: var(--text-secondary);
		cursor: pointer;
	}

	.match-toggle { margin-top: 12px; }
	.match-toggle:has(input:disabled) { opacity: 0.5; cursor: not-allowed; }

	.match-result {
		margin-top: 8px;
		padding: 8px 10px;
		border-radius: 6px;
		background: rgba(255, 0, 255, 0.08);
		border: 1px solid rgba(255, 0, 255, 0.3);
		font-size: 12px;
		color: var(--text-primary);
	}

	.match-result strong { color: var(--accent-blue); }

	.copy-row {
		display: flex;
		align-items: center;
		gap: 10px;
		margin-top: 12px;
	}

	.color-summary,
	.coordinate-card {
		display: flex;
		align-items: center;
		gap: 10px;
		margin-top: 12px;
		padding: 10px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 8px;
		font-size: 12px;
		color: var(--text-secondary);
	}

	.coordinate-card {
		align-items: flex-start;
		flex-direction: column;
		gap: 8px;
	}

	.coordinate-main {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 8px;
		width: 100%;
	}

	.coordinate-card strong {
		color: var(--text-primary);
		font-size: 20px;
	}

	.picked-color {
		display: flex;
		align-items: center;
		gap: 8px;
		flex-wrap: wrap;
	}

	.picked-color code { color: var(--accent-blue); font-size: 13px; }

	.coord-note { font-size: 11px; }

	.btn,
	.link-btn {
		display: inline-flex;
		align-items: center;
		justify-content: center;
		gap: 6px;
		min-height: 36px;
		padding: 8px 14px;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		font-size: 13px;
		cursor: pointer;
		transition: all 0.2s;
	}

	.btn:hover,
	.link-btn:hover {
		background: var(--border-color);
	}

	.btn:disabled,
	.link-btn:disabled {
		cursor: not-allowed;
		opacity: 0.5;
	}

	.btn-primary {
		background: var(--accent-green);
		color: white;
		border-color: var(--accent-green);
	}

	.tool-btn {
		flex: 1;
		margin-top: 0;
		min-height: 32px;
		padding: 6px 10px;
		font-size: 12px;
	}

	.link-btn {
		min-height: 28px;
		padding: 4px 8px;
		color: var(--accent-blue);
		background: transparent;
	}

	@media (max-width: 1180px) {
		.screen-layout {
			grid-template-columns: 1fr;
		}

		.tools-card {
			order: -1;
		}
	}

	@media (max-width: 720px) {
		.page-header,
		.header-actions {
			align-items: stretch;
			flex-direction: column;
		}

		.preview-stage {
			min-height: 340px;
		}
	}
</style>
