<script lang="ts">
	import { macros } from '$lib/stores/macros';
	import { connection } from '$lib/stores/connection';
	import { onMount } from 'svelte';

	let speed = $state(100);
	let repeat = $state(1);
	let savePath = $state('');
	let loadPath = $state('');
	let playing = $state(false);

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
</script>

<div class="macros-page">
	<div class="page-header">
		<h1>宏录制</h1>
		<p>录制鼠标键盘操作并回放。录制期间 runtime 通过低层钩子捕获全局输入事件。</p>
	</div>

	{#if !$connection.connected}
		<div class="notice">
			未连接到 runtime IPC，宏功能不可用。请先连接。
		</div>
	{:else}
		<div class="card recording-card">
			<div class="card-title">录制控制</div>
			<div class="status-row">
				<span class="status-dot" class:recording={$macros.recording}></span>
				<span>{$macros.recording ? '录制中…' : '空闲'}</span>
				<span class="event-count">已捕获 {$macros.eventCount} 个事件</span>
			</div>
			<div class="btn-row">
				{#if !$macros.recording}
					<button class="btn btn-primary" onclick={() => macros.record()}>
						● 开始录制
					</button>
				{:else}
					<button class="btn btn-danger" onclick={() => macros.stop()}>
						■ 停止录制
					</button>
				{/if}
				<button class="btn btn-secondary" onclick={() => macros.clear()} disabled={$macros.recording}>
					清空
				</button>
			</div>
		</div>

		<div class="card playback-card">
			<div class="card-title">回放</div>
			<div class="form-row">
				<label>
					速度 (%)
					<input type="number" bind:value={speed} min="10" max="500" step="10" />
				</label>
				<label>
					重复次数
					<input type="number" bind:value={repeat} min="1" max="100" />
				</label>
			</div>
			<button class="btn btn-success" onclick={handlePlay} disabled={playing || $macros.recording || $macros.eventCount === 0}>
				{playing ? '回放中…' : '▶ 回放'}
			</button>
		</div>

		<div class="card io-card">
			<div class="card-title">保存 / 载入</div>
			<div class="form-row">
				<input type="text" bind:value={savePath} placeholder="保存路径，如 D:/macros/m1.json" />
				<button class="btn btn-secondary" onclick={() => savePath && macros.save(savePath)} disabled={$macros.eventCount === 0}>
					保存为 JSON
				</button>
			</div>
			<div class="form-row">
				<input type="text" bind:value={loadPath} placeholder="载入路径，如 D:/macros/m1.json" />
				<button class="btn btn-secondary" onclick={() => loadPath && macros.load(loadPath)}>
					载入
				</button>
			</div>
		</div>

		<div class="hint">
			<strong>说明：</strong>录制时所有鼠标键盘操作会被全局钩子捕获（含时间戳）。
			回放按原始时间间隔重放；速度 &lt; 100 放慢，&gt; 100 加快。保存为 JSON 后可后续载入回放，
			亦可由脚本通过 <code>wingman.input</code> 手动编排。
		</div>
	{/if}
</div>

<style>
	.macros-page {
		padding: 24px 32px;
		max-width: 720px;
	}

	.page-header h1 {
		font-size: 22px;
		margin-bottom: 6px;
	}

	.page-header p {
		color: var(--text-secondary);
		font-size: 13px;
		margin-bottom: 20px;
	}

	.notice {
		padding: 16px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 8px;
		color: var(--text-secondary);
		font-size: 14px;
	}

	.card {
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 10px;
		padding: 18px 20px;
		margin-bottom: 16px;
	}

	.card-title {
		font-size: 14px;
		font-weight: 600;
		margin-bottom: 14px;
		color: var(--text-primary);
	}

	.status-row {
		display: flex;
		align-items: center;
		gap: 10px;
		font-size: 13px;
		margin-bottom: 16px;
		color: var(--text-secondary);
	}

	.status-dot {
		width: 10px;
		height: 10px;
		border-radius: 50%;
		background: var(--text-secondary);
	}

	.status-dot.recording {
		background: var(--accent-red);
		box-shadow: 0 0 8px rgba(248, 81, 73, 0.6);
		animation: pulse 1s infinite;
	}

	@keyframes pulse {
		0%, 100% { opacity: 1; }
		50% { opacity: 0.4; }
	}

	.event-count {
		margin-left: auto;
		font-variant-numeric: tabular-nums;
	}

	.btn-row,
	.form-row {
		display: flex;
		gap: 10px;
		align-items: center;
	}

	.form-row {
		margin-bottom: 12px;
	}

	.form-row label {
		display: flex;
		flex-direction: column;
		gap: 4px;
		font-size: 12px;
		color: var(--text-secondary);
	}

	.btn {
		padding: 8px 16px;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		font-size: 13px;
		cursor: pointer;
		transition: all 0.2s;
	}

	.btn:hover:not(:disabled) {
		background: var(--border-color);
	}

	.btn:disabled {
		opacity: 0.45;
		cursor: not-allowed;
	}

	.btn-primary {
		background: var(--accent-red);
		border-color: var(--accent-red);
		color: white;
	}

	.btn-success {
		background: var(--accent-green);
		border-color: var(--accent-green);
		color: white;
	}

	.btn-danger {
		background: var(--accent-red);
		border-color: var(--accent-red);
		color: white;
	}

	input[type='text'],
	input[type='number'] {
		flex: 1;
		padding: 7px 10px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		color: var(--text-primary);
		font-size: 13px;
		font-family: inherit;
	}

	input[type='number'] {
		width: 90px;
		flex: none;
	}

	input:focus {
		outline: none;
		border-color: var(--accent-blue);
	}

	.hint {
		margin-top: 8px;
		padding: 14px 16px;
		background: var(--bg-tertiary);
		border-radius: 8px;
		font-size: 12px;
		color: var(--text-secondary);
		line-height: 1.6;
	}

	.hint code {
		background: var(--bg-primary);
		padding: 1px 5px;
		border-radius: 3px;
		font-size: 11px;
	}

	@media (max-width: 600px) {
		.macros-page {
			padding: 16px;
		}
		.form-row {
			flex-direction: column;
			align-items: stretch;
		}
	}
</style>
