<script lang="ts">
	import RegionPicker from '$lib/components/RegionPicker.svelte';
	import ColorPicker from '$lib/components/ColorPicker.svelte';
	import { connection } from '$lib/stores/connection';
	import { logs } from '$lib/stores/logs';
	import { DEFAULT_REGION, screen, type ScreenRegion } from '$lib/stores/screen';

	const PREVIEW_REGION: ScreenRegion = { x: 0, y: 0, width: 0, height: 0 };

	let region = $state<ScreenRegion>({ ...DEFAULT_REGION });
	let targetColor = $state('#3fb950');
	let tolerance = $state(12);
	let selectedMonitor = $state('primary');
	let autoRefresh = $state(false);
	let pointer = $state<{ x: number; y: number; color: string } | null>(null);

	let overlayStyle = $derived.by(() => {
		const shot = $screen.current;
		if (!shot || shot.width <= 0 || shot.height <= 0) {
			return 'display: none;';
		}
		const left = Math.max(0, (region.x / shot.width) * 100);
		const top = Math.max(0, (region.y / shot.height) * 100);
		const width = Math.max(0, (region.width / shot.width) * 100);
		const height = Math.max(0, (region.height / shot.height) * 100);
		return `left:${left}%;top:${top}%;width:${width}%;height:${height}%;`;
	});

	let pointerStyle = $derived.by(() => {
		const shot = $screen.current;
		if (!shot || !pointer) return 'display: none;';
		return `left:${(pointer.x / shot.width) * 100}%;top:${(pointer.y / shot.height) * 100}%;`;
	});

	async function capture(showLog = false) {
		const result = await screen.capture(PREVIEW_REGION);
		if (result && showLog) {
			logs.add(`已刷新屏幕预览: ${result.width}x${result.height}`, 'success');
		} else if (!result && $screen.error) {
			logs.add(`屏幕预览刷新失败: ${$screen.error}`, 'error');
		}
	}

	function fitFullImage() {
		if (!$screen.current) return;
		region = {
			x: 0,
			y: 0,
			width: $screen.current.width,
			height: $screen.current.height,
		};
	}

	function handlePreviewClick(event: MouseEvent) {
		const shot = $screen.current;
		if (!shot) return;
		const target = event.currentTarget as HTMLElement;
		const rect = target.getBoundingClientRect();
		const x = Math.round(((event.clientX - rect.left) / rect.width) * shot.width);
		const y = Math.round(((event.clientY - rect.top) / rect.height) * shot.height);
		pointer = {
			x: Math.max(0, Math.min(shot.width, x)),
			y: Math.max(0, Math.min(shot.height, y)),
			color: targetColor,
		};
	}

	function handlePreviewKeydown(event: KeyboardEvent) {
		if (event.key !== 'Enter' && event.key !== ' ') return;
		event.preventDefault();
		if (!$screen.current) {
			capture(true);
		}
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
			<p class="page-subtitle">截图预览、区域框选、坐标和颜色拾取</p>
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
					<span>{$screen.current ? `${$screen.current.width}x${$screen.current.height}` : '等待截图'} · {$screen.lastUpdated}</span>
				</div>
				<div class="preview-status" class:connected={$connection.connected}>
					{$connection.connected ? 'runtime IPC' : 'dev fallback'}
				</div>
			</div>

			<div class="preview-stage">
				{#if $screen.current}
					<div
						class="image-frame"
						role="button"
						tabindex="0"
						aria-label="屏幕截图预览，点击拾取坐标"
						onclick={handlePreviewClick}
						onkeydown={handlePreviewKeydown}
					>
						<img
							src={$screen.current.image}
							alt="屏幕截图"
						/>
						<div class="region-overlay" style={overlayStyle}>
							<span>{region.x},{region.y} · {region.width}x{region.height}</span>
						</div>
						{#if pointer}
							<div class="pointer" style={pointerStyle}></div>
						{/if}
					</div>
				{:else}
					<div class="empty-preview">
						<span>尚无截图</span>
						<button class="btn" onclick={() => capture(true)}>立即捕获</button>
					</div>
				{/if}
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
					<span class="muted">基础选择</span>
				</div>
				<select class="form-input" bind:value={selectedMonitor}>
					<option value="primary">主显示器</option>
					<option value="virtual" disabled>虚拟桌面（待 runtime 支持）</option>
				</select>
			</div>

			<div class="tool-section">
				<div class="section-heading">
					<span>区域</span>
					<button class="link-btn" onclick={fitFullImage} disabled={!$screen.current}>全图</button>
				</div>
				<RegionPicker bind:value={region} />
				<button class="btn tool-btn" onclick={selectPointAsRegion} disabled={!pointer}>以坐标为中心</button>
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
			</div>

			<div class="tool-section">
				<div class="section-heading">
					<span>坐标</span>
					<span class="muted">点击画面拾取</span>
				</div>
				{#if pointer}
					<div class="coordinate-card">
						<strong>{pointer.x}, {pointer.y}</strong>
						<span>相对当前截图</span>
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
		cursor: crosshair;
		overflow: hidden;
	}

	.image-frame {
		position: relative;
		display: inline-flex;
		max-width: 100%;
		max-height: 72vh;
		cursor: crosshair;
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
	}

	.region-overlay {
		position: absolute;
		border: 2px solid var(--accent-green);
		box-shadow: 0 0 0 9999px rgba(0, 0, 0, 0.28), 0 0 18px rgba(63, 185, 80, 0.45);
		pointer-events: none;
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

	.pointer {
		position: absolute;
		width: 18px;
		height: 18px;
		border: 2px solid var(--accent-yellow);
		border-radius: 50%;
		transform: translate(-50%, -50%);
		box-shadow: 0 0 14px rgba(210, 153, 34, 0.8);
		pointer-events: none;
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

	.toggle {
		display: flex;
		align-items: center;
		gap: 8px;
		font-size: 13px;
		color: var(--text-secondary);
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
	}

	.coordinate-card strong {
		color: var(--text-primary);
		font-size: 20px;
	}

	.swatch {
		width: 18px;
		height: 18px;
		border-radius: 4px;
		border: 1px solid var(--border-color);
	}

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
		width: 100%;
		margin-top: 12px;
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
