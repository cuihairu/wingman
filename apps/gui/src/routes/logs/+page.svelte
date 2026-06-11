<script lang="ts">
	import { logs, type LogEntry } from '$lib/stores/logs';

	let query = $state('');
	let selectedLevel = $state<LogEntry['type'] | 'all'>('all');
	let autoScroll = $state(true);
	let logContainer = $state<HTMLDivElement | null>(null);

	const levels: Array<{ value: LogEntry['type'] | 'all'; label: string }> = [
		{ value: 'all', label: '全部' },
		{ value: 'info', label: '信息' },
		{ value: 'success', label: '成功' },
		{ value: 'warning', label: '警告' },
		{ value: 'error', label: '错误' },
	];

	let filteredLogs = $derived($logs.filter(log => {
		const matchesLevel = selectedLevel === 'all' || log.type === selectedLevel;
		const matchesQuery = !query.trim()
			|| log.message.toLowerCase().includes(query.trim().toLowerCase())
			|| log.time.includes(query.trim());
		return matchesLevel && matchesQuery;
	}));

	function exportLogs() {
		const content = filteredLogs
			.map(log => `${log.time} [${log.type.toUpperCase()}] ${log.message}`)
			.join('\n');
		const blob = new Blob([content], { type: 'text/plain;charset=utf-8' });
		const url = URL.createObjectURL(blob);
		const link = document.createElement('a');
		link.href = url;
		link.download = `wingman-logs-${new Date().toISOString().replace(/[:.]/g, '-')}.txt`;
		link.click();
		URL.revokeObjectURL(url);
	}

	$effect(() => {
		if (!autoScroll || !logContainer) return;
		filteredLogs.length;
		requestAnimationFrame(() => {
			if (logContainer) logContainer.scrollTop = logContainer.scrollHeight;
		});
	});
</script>

<div class="page-header">
	<div>
		<h2 class="page-title">系统日志</h2>
		<p class="page-subtitle">查看运行日志和错误信息</p>
	</div>
	<div class="header-actions">
		<button class="btn" onclick={exportLogs} disabled={filteredLogs.length === 0}>导出</button>
		<button class="btn" onclick={() => logs.clear()}>清空</button>
	</div>
</div>

<div class="card">
	<div class="toolbar">
		<div class="search-box">
			<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
				<circle cx="11" cy="11" r="8"></circle>
				<path d="m21 21-4.35-4.35"></path>
			</svg>
			<input type="text" placeholder="搜索日志" bind:value={query} />
		</div>
		<div class="level-filter">
			{#each levels as level}
				<button
					class="filter-btn"
					class:active={selectedLevel === level.value}
					onclick={() => selectedLevel = level.value}
				>
					{level.label}
				</button>
			{/each}
		</div>
		<label class="checkbox-label">
			<input type="checkbox" bind:checked={autoScroll} />
			<span>自动滚动</span>
		</label>
	</div>

	<div class="card-body">
		<div class="log-container" bind:this={logContainer}>
			{#if filteredLogs.length === 0}
				<div class="log-entry muted"><span class="log-time">[00:00:00]</span> 暂无匹配日志</div>
			{:else}
				{#each filteredLogs as log}
					<div class="log-entry">
						<span class="log-time">{log.time}</span>
						<span class="log-level log-{log.type}">{log.type.toUpperCase()}</span>
						<span class="log-message">{log.message}</span>
					</div>
				{/each}
			{/if}
		</div>
	</div>
</div>

<style>
	.page-header {
		display: flex;
		align-items: flex-start;
		justify-content: space-between;
		gap: 16px;
		margin-bottom: 24px;
	}

	.page-title {
		font-size: 24px;
		font-weight: 600;
		color: var(--text-primary);
		margin-bottom: 8px;
	}

	.page-subtitle {
		font-size: 14px;
		color: var(--text-secondary);
	}

	.header-actions {
		display: flex;
		gap: 8px;
	}

	.card {
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 8px;
		margin-bottom: 16px;
		overflow: hidden;
	}

	.toolbar {
		display: grid;
		grid-template-columns: minmax(220px, 1fr) auto auto;
		align-items: center;
		gap: 12px;
		padding: 14px 16px;
		border-bottom: 1px solid var(--border-color);
	}

	.search-box {
		display: flex;
		align-items: center;
		gap: 8px;
		min-height: 36px;
		padding: 0 10px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		color: var(--text-secondary);
	}

	.search-box input {
		width: 100%;
		background: transparent;
		border: none;
		outline: none;
		color: var(--text-primary);
		font-size: 13px;
	}

	.level-filter {
		display: flex;
		gap: 4px;
		padding: 3px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
	}

	.filter-btn {
		min-width: 48px;
		min-height: 28px;
		padding: 4px 8px;
		border-radius: 4px;
		background: transparent;
		color: var(--text-secondary);
		font-size: 12px;
	}

	.filter-btn:hover,
	.filter-btn.active {
		background: var(--border-color);
		color: var(--text-primary);
	}

	.checkbox-label {
		display: inline-flex;
		align-items: center;
		gap: 6px;
		color: var(--text-secondary);
		font-size: 12px;
		white-space: nowrap;
	}

	.card-body {
		padding: 16px;
	}

	.btn {
		display: inline-flex;
		align-items: center;
		justify-content: center;
		min-height: 34px;
		padding: 7px 12px;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		font-size: 12px;
		cursor: pointer;
		transition: all 0.2s;
	}

	.btn:hover {
		background: var(--border-color);
	}

	.btn:disabled {
		cursor: not-allowed;
		opacity: 0.45;
	}

	.log-container {
		background: var(--bg-primary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		height: min(620px, calc(100vh - 250px));
		min-height: 360px;
		overflow-y: auto;
		padding: 12px;
		font-family: Consolas, Monaco, monospace;
		font-size: 12px;
	}

	.log-entry {
		display: grid;
		grid-template-columns: 90px 64px minmax(0, 1fr);
		gap: 8px;
		align-items: start;
		min-height: 22px;
		padding: 2px 0;
	}

	.log-entry.muted {
		grid-template-columns: 90px 1fr;
		color: var(--text-secondary);
	}

	.log-time {
		color: var(--text-secondary);
	}

	.log-level {
		font-weight: 600;
	}

	.log-message {
		color: var(--text-primary);
		white-space: pre-wrap;
		word-break: break-word;
	}

	.log-info { color: var(--accent-blue); }
	.log-success { color: var(--accent-green); }
	.log-warning { color: var(--accent-yellow); }
	.log-error { color: var(--accent-red); }

	@media (max-width: 900px) {
		.toolbar {
			grid-template-columns: 1fr;
		}

		.level-filter {
			overflow-x: auto;
		}
	}

	@media (max-width: 640px) {
		.page-header {
			flex-direction: column;
		}

		.log-entry {
			grid-template-columns: 82px 54px minmax(0, 1fr);
		}
	}
</style>
