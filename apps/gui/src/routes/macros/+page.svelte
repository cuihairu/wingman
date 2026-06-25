<script lang="ts">
	import { onMount } from 'svelte';
	import { connection } from '$lib/stores/connection';
	import { macros } from '$lib/stores/macros';
	import { logs } from '$lib/stores/logs';

	let speed = $state(100);
	let repeat = $state(1);
	let savePath = $state('');
	let loadPath = $state('');
	let playing = $state(false);

	let canPlay = $derived(!$macros.recording && $macros.eventCount > 0 && !playing);
	let timelineSummary = $derived([
		{ label: '已捕获', value: `${$macros.eventCount}`, hint: '事件' },
		{ label: '速度', value: `${speed}%`, hint: 'playback' },
		{ label: '重复', value: `${repeat}`, hint: 'cycles' },
		{ label: '状态', value: $macros.recording ? '录制中' : playing ? '回放中' : '空闲', hint: 'session' },
	]);

	onMount(() => {
		macros.refresh();
	});

	async function handlePlay() {
		if (playing) return;
		playing = true;
		try {
			await macros.play(speed, repeat);
		} finally {
			playing = false;
		}
	}

	async function saveMacro() {
		if (!savePath.trim()) {
			logs.add('请输入保存路径', 'warning');
			return;
		}
		await macros.save(savePath.trim());
	}

	async function loadMacro() {
		if (!loadPath.trim()) {
			logs.add('请输入载入路径', 'warning');
			return;
		}
		await macros.load(loadPath.trim());
	}
</script>

<div class="macros-page">
	<section class="page-header">
		<div>
			<h2 class="page-title">宏录制</h2>
			<p class="page-subtitle">捕获全局输入事件，保存、载入并按原始时间轴回放</p>
		</div>
		<div class="header-state">
			<div class="status-pill" class:recording={$macros.recording} class:offline={!$connection.connected}>
				<span class="status-dot"></span>
				{!$connection.connected ? 'runtime IPC 未连接' : $macros.recording ? '录制进行中' : playing ? '宏回放中' : '准备就绪'}
			</div>
		</div>
	</section>

	<section class="summary-grid">
		{#each timelineSummary as item}
			<div class="metric-card">
				<span>{item.label}</span>
				<strong>{item.value}</strong>
				<small>{item.hint}</small>
			</div>
		{/each}
	</section>

	{#if !$connection.connected}
		<div class="notice-card">
			<strong>宏功能当前不可用</strong>
			<span>该页依赖 runtime IPC。连接建立后才能开始录制、回放和保存。</span>
		</div>
	{:else}
		<section class="workspace-grid">
			<div class="card recorder-card">
				<div class="panel-heading">
					<div>
						<span class="eyebrow">Capture</span>
						<h3>录制控制</h3>
					</div>
					<span class="inline-count">{$macros.eventCount} events</span>
				</div>
				<div class="recording-banner" class:active={$macros.recording}>
					<div class="banner-dot"></div>
					<div>
						<strong>{$macros.recording ? '正在捕获全局输入' : '当前未录制'}</strong>
						<span>{$macros.recording ? 'runtime 正在收集鼠标和键盘事件时间戳' : '开始录制后，事件会进入当前会话缓存'}</span>
					</div>
				</div>
				<div class="action-row">
					{#if !$macros.recording}
						<button class="btn btn-primary" onclick={() => macros.record()}>
							<svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor">
								<circle cx="12" cy="12" r="8"></circle>
							</svg>
							开始录制
						</button>
					{:else}
						<button class="btn btn-danger" onclick={() => macros.stop()}>
							<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
								<rect x="6" y="6" width="12" height="12"></rect>
							</svg>
							停止录制
						</button>
					{/if}
					<button class="btn" onclick={() => macros.clear()} disabled={$macros.recording || $macros.eventCount === 0}>
						清空会话
					</button>
					<button class="btn" onclick={() => macros.refresh()}>
						刷新状态
					</button>
				</div>
			</div>

			<div class="card playback-card">
				<div class="panel-heading compact">
					<div>
						<span class="eyebrow">Playback</span>
						<h3>回放参数</h3>
					</div>
				</div>
				<div class="control-grid">
					<label class="field">
						<span>速度 (%)</span>
						<input type="number" bind:value={speed} min="10" max="500" step="10" />
					</label>
					<label class="field">
						<span>重复次数</span>
						<input type="number" bind:value={repeat} min="1" max="100" />
					</label>
				</div>
				<button class="btn btn-success wide-btn" onclick={handlePlay} disabled={!canPlay}>
					<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<polygon points="5 3 19 12 5 21 5 3"></polygon>
					</svg>
					{playing ? '回放中' : '开始回放'}
				</button>
				<div class="helper-copy">
					速度小于 `100%` 会放慢时间轴，大于 `100%` 会压缩间隔。重复次数按整轮事件序列执行。
				</div>
			</div>
		</section>

		<section class="card io-card">
			<div class="panel-heading">
				<div>
					<span class="eyebrow">Storage</span>
					<h3>保存与载入</h3>
				</div>
			</div>
			<div class="io-grid">
				<div class="io-block">
					<label class="field">
						<span>保存路径</span>
						<input type="text" bind:value={savePath} placeholder="D:/macros/m1.json" />
					</label>
					<button class="btn" onclick={saveMacro} disabled={$macros.eventCount === 0 || $macros.recording}>
						保存为 JSON
					</button>
				</div>
				<div class="io-block">
					<label class="field">
						<span>载入路径</span>
						<input type="text" bind:value={loadPath} placeholder="D:/macros/m1.json" />
					</label>
					<button class="btn" onclick={loadMacro}>
						载入宏文件
					</button>
				</div>
			</div>
		</section>

		<section class="hint-grid">
			<div class="hint-card">
				<strong>录制范围</strong>
				<span>运行时通过全局钩子捕获鼠标和键盘事件，并保留相邻事件的时间间隔。</span>
			</div>
			<div class="hint-card">
				<strong>回放模型</strong>
				<span>回放直接使用 runtime 执行事件队列。需要更复杂的编排时，可转回脚本层使用 `wingman.input`。</span>
			</div>
		</section>
	{/if}
</div>

<style>
	.macros-page {
		display: flex;
		flex-direction: column;
		gap: 18px;
	}

	.page-header {
		display: flex;
		align-items: flex-start;
		justify-content: space-between;
		gap: 16px;
	}

	.page-title {
		margin-bottom: 8px;
		color: var(--text-primary);
		font-size: 24px;
		font-weight: 600;
	}

	.page-subtitle {
		color: var(--text-secondary);
		font-size: 13px;
	}

	.status-pill {
		display: inline-flex;
		align-items: center;
		gap: 8px;
		min-height: 34px;
		padding: 7px 11px;
		border: 1px solid rgba(88, 166, 255, 0.32);
		border-radius: 999px;
		background: rgba(88, 166, 255, 0.08);
		color: var(--accent-blue);
		font-size: 12px;
		white-space: nowrap;
	}

	.status-pill.recording {
		border-color: rgba(248, 81, 73, 0.36);
		background: rgba(248, 81, 73, 0.1);
		color: var(--accent-red);
	}

	.status-pill.offline {
		border-color: var(--border-color);
		background: var(--bg-secondary);
		color: var(--text-secondary);
	}

	.status-dot,
	.banner-dot {
		width: 8px;
		height: 8px;
		border-radius: 50%;
		background: currentColor;
		flex-shrink: 0;
	}

	.summary-grid {
		display: grid;
		grid-template-columns: repeat(4, minmax(0, 1fr));
		gap: 12px;
	}

	.metric-card,
	.card,
	.notice-card,
	.hint-card {
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 8px;
	}

	.metric-card {
		display: flex;
		flex-direction: column;
		gap: 8px;
		min-height: 104px;
		padding: 14px;
	}

	.metric-card span,
	.metric-card small {
		color: var(--text-secondary);
		font-size: 12px;
	}

	.metric-card strong {
		color: var(--text-primary);
		font-size: 28px;
		line-height: 1;
		font-variant-numeric: tabular-nums;
	}

	.notice-card {
		display: flex;
		flex-direction: column;
		gap: 6px;
		padding: 16px;
		color: var(--text-secondary);
		font-size: 13px;
	}

	.notice-card strong {
		color: var(--text-primary);
		font-size: 14px;
	}

	.workspace-grid {
		display: grid;
		grid-template-columns: minmax(0, 1.2fr) minmax(320px, 0.8fr);
		gap: 12px;
	}

	.card {
		padding: 16px;
	}

	.panel-heading {
		display: flex;
		align-items: flex-start;
		justify-content: space-between;
		gap: 12px;
		margin-bottom: 14px;
	}

	.panel-heading.compact {
		align-items: center;
	}

	.eyebrow,
	.inline-count {
		color: var(--text-secondary);
		font-size: 11px;
		text-transform: uppercase;
	}

	.panel-heading h3 {
		margin-top: 3px;
		color: var(--text-primary);
		font-size: 16px;
		font-weight: 600;
	}

	.recording-banner {
		display: grid;
		grid-template-columns: auto minmax(0, 1fr);
		gap: 12px;
		align-items: flex-start;
		padding: 14px;
		border: 1px solid var(--border-color);
		border-radius: 8px;
		background: var(--bg-tertiary);
	}

	.recording-banner.active {
		border-color: rgba(248, 81, 73, 0.34);
		background: rgba(248, 81, 73, 0.08);
	}

	.recording-banner strong {
		display: block;
		margin-bottom: 4px;
		color: var(--text-primary);
		font-size: 14px;
	}

	.recording-banner span,
	.helper-copy,
	.hint-card span {
		color: var(--text-secondary);
		font-size: 12px;
		line-height: 1.6;
	}

	.action-row {
		display: flex;
		flex-wrap: wrap;
		gap: 8px;
		margin-top: 14px;
	}

	.control-grid,
	.io-grid {
		display: grid;
		grid-template-columns: repeat(2, minmax(0, 1fr));
		gap: 12px;
	}

	.field {
		display: flex;
		flex-direction: column;
		gap: 6px;
	}

	.field span {
		color: var(--text-secondary);
		font-size: 12px;
	}

	.field input {
		min-height: 38px;
		padding: 8px 10px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		color: var(--text-primary);
		font-size: 13px;
	}

	.field input:focus {
		outline: none;
		border-color: var(--accent-blue);
		box-shadow: 0 0 0 3px var(--focus-ring);
	}

	.wide-btn {
		width: 100%;
		margin-top: 14px;
	}

	.helper-copy {
		margin-top: 12px;
	}

	.io-block {
		display: flex;
		flex-direction: column;
		gap: 10px;
	}

	.hint-grid {
		display: grid;
		grid-template-columns: repeat(2, minmax(0, 1fr));
		gap: 12px;
	}

	.hint-card {
		display: flex;
		flex-direction: column;
		gap: 6px;
		padding: 14px;
	}

	.hint-card strong {
		color: var(--text-primary);
		font-size: 13px;
	}

	.btn {
		display: inline-flex;
		align-items: center;
		justify-content: center;
		gap: 6px;
		min-height: 36px;
		padding: 8px 12px;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		font-size: 13px;
		transition: all 0.18s;
		white-space: nowrap;
	}

	.btn:hover {
		background: var(--surface-hover);
	}

	.btn:disabled {
		opacity: 0.5;
		cursor: not-allowed;
	}

	.btn-primary {
		background: var(--accent-red);
		border-color: var(--accent-red);
		color: white;
	}

	.btn-primary:hover {
		background: var(--accent-red-hover);
	}

	.btn-success {
		background: var(--accent-green);
		border-color: var(--accent-green);
		color: white;
	}

	.btn-success:hover {
		background: var(--accent-green-hover);
	}

	.btn-danger {
		background: var(--accent-red);
		border-color: var(--accent-red);
		color: white;
	}

	.btn-danger:hover {
		background: var(--accent-red-hover);
	}

	@media (max-width: 1080px) {
		.summary-grid {
			grid-template-columns: repeat(2, minmax(0, 1fr));
		}

		.workspace-grid {
			grid-template-columns: 1fr;
		}
	}

	@media (max-width: 720px) {
		.page-header {
			flex-direction: column;
		}

		.control-grid,
		.io-grid,
		.hint-grid {
			grid-template-columns: 1fr;
		}
	}

	@media (max-width: 520px) {
		.summary-grid {
			grid-template-columns: 1fr;
		}

		.action-row {
			flex-direction: column;
		}

		.action-row .btn,
		.io-block .btn {
			width: 100%;
		}
	}
</style>
