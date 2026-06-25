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
		const keyword = query.trim().toLowerCase();
		const matchesQuery = !keyword
			|| log.message.toLowerCase().includes(keyword)
			|| log.time.includes(query.trim());
		return matchesLevel && matchesQuery;
	}));

	let counts = $derived({
		total: $logs.length,
		info: $logs.filter(log => log.type === 'info').length,
		error: $logs.filter(log => log.type === 'error').length,
	});

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

	function clearFilters() {
		query = '';
		selectedLevel = 'all';
	}

	$effect(() => {
		if (!autoScroll || !logContainer) return;
		filteredLogs.length;
		requestAnimationFrame(() => {
			if (logContainer) logContainer.scrollTop = logContainer.scrollHeight;
		});
	});
</script>

<div class="logs-page">
	<section class="page-header">
		<div>
			<h2 class="page-title">系统日志</h2>
			<p class="page-subtitle">本地 GUI 与 runtime 事件流。支持按等级筛选、搜索和导出。</p>
		</div>
		<div class="header-actions">
			<button class="btn" onclick={exportLogs} disabled={filteredLogs.length === 0}>导出</button>
			<button class="btn" onclick={() => logs.clear()} disabled={$logs.length === 0}>清空</button>
		</div>
	</section>

	<section class="summary-grid">
		<div class="metric-card">
			<span>总量</span>
			<strong>{counts.total}</strong>
			<small>buffered</small>
		</div>
		<div class="metric-card">
			<span>信息</span>
			<strong class="blue">{counts.info}</strong>
			<small>info</small>
		</div>
		<div class="metric-card">
			<span>错误</span>
			<strong class="red">{counts.error}</strong>
			<small>error</small>
		</div>
		<div class="metric-card">
			<span>匹配结果</span>
			<strong>{filteredLogs.length}</strong>
			<small>filtered</small>
		</div>
	</section>

	<section class="card">
		<div class="toolbar">
			<div class="search-box">
				<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<circle cx="11" cy="11" r="8"></circle>
					<path d="m21 21-4.35-4.35"></path>
				</svg>
				<input type="text" placeholder="搜索日志内容或时间" bind:value={query} />
			</div>
			<div class="level-filter">
				{#each levels as level}
					<button
						class:active={selectedLevel === level.value}
						onclick={() => selectedLevel = level.value}
					>
						{level.label}
					</button>
				{/each}
			</div>
			<label class="toggle-row">
				<input type="checkbox" bind:checked={autoScroll} />
				<span>自动滚动</span>
			</label>
		</div>

		<div class="terminal-frame">
			<div class="terminal-head">
				<div class="terminal-dots">
					<span></span><span></span><span></span>
				</div>
				<div class="terminal-meta">
					<span>{filteredLogs.length} lines</span>
					{#if query.trim() || selectedLevel !== 'all'}
						<button class="link-btn" onclick={clearFilters}>清除筛选</button>
					{/if}
				</div>
			</div>
			<div class="log-container" bind:this={logContainer}>
				{#if filteredLogs.length === 0}
					<div class="empty-state">
						<strong>{$logs.length === 0 ? '还没有日志' : '没有匹配项'}</strong>
						<span>{$logs.length === 0 ? '运行操作后，新日志会出现在这里。' : '调整关键字或日志等级后再试。'}</span>
					</div>
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
	</section>
</div>

<style>
	.logs-page {
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

	.header-actions {
		display: flex;
		gap: 8px;
	}

	.summary-grid {
		display: grid;
		grid-template-columns: repeat(4, minmax(0, 1fr));
		gap: 12px;
	}

	.metric-card,
	.card {
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

	.blue { color: var(--accent-blue) !important; }
	.red { color: var(--accent-red) !important; }

	.card {
		overflow: hidden;
	}

	.toolbar {
		display: grid;
		grid-template-columns: minmax(240px, 1fr) auto auto;
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
		gap: 3px;
		padding: 3px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
	}

	.level-filter button {
		min-width: 52px;
		min-height: 28px;
		padding: 4px 8px;
		border-radius: 4px;
		background: transparent;
		color: var(--text-secondary);
		font-size: 12px;
	}

	.level-filter button:hover,
	.level-filter button.active {
		background: var(--surface-hover);
		color: var(--text-primary);
	}

	.toggle-row {
		display: inline-flex;
		align-items: center;
		gap: 6px;
		color: var(--text-secondary);
		font-size: 12px;
		white-space: nowrap;
	}

	.terminal-frame {
		padding: 16px;
	}

	.terminal-head {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 12px;
		padding: 8px 12px;
		border: 1px solid var(--border-color);
		border-bottom: none;
		border-radius: 8px 8px 0 0;
		background: var(--bg-tertiary);
	}

	.terminal-dots {
		display: flex;
		gap: 6px;
	}

	.terminal-dots span {
		width: 10px;
		height: 10px;
		border-radius: 50%;
		background: var(--border-color);
	}

	.terminal-dots span:nth-child(1) { background: #ff5f57; }
	.terminal-dots span:nth-child(2) { background: #febc2e; }
	.terminal-dots span:nth-child(3) { background: #28c840; }

	.terminal-meta {
		display: flex;
		align-items: center;
		gap: 10px;
		color: var(--text-secondary);
		font-size: 12px;
	}

	.log-container {
		height: min(620px, calc(100vh - 290px));
		min-height: 360px;
		overflow-y: auto;
		padding: 12px;
		border: 1px solid var(--border-color);
		border-radius: 0 0 8px 8px;
		background:
			linear-gradient(180deg, rgba(255, 255, 255, 0.01), rgba(255, 255, 255, 0)),
			var(--bg-primary);
		font-family: 'SFMono-Regular', Consolas, 'Liberation Mono', Menlo, monospace;
		font-size: 12px;
	}

	.log-entry {
		display: grid;
		grid-template-columns: 90px 64px minmax(0, 1fr);
		gap: 8px;
		align-items: start;
		min-height: 22px;
		padding: 3px 0;
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

	.empty-state {
		display: grid;
		place-items: center;
		gap: 8px;
		min-height: 220px;
		padding: 24px;
		text-align: center;
		color: var(--text-secondary);
	}

	.empty-state strong {
		color: var(--text-primary);
		font-size: 14px;
	}

	.btn,
	.link-btn {
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
		transition: all 0.18s;
	}

	.btn:hover,
	.link-btn:hover {
		background: var(--surface-hover);
	}

	.btn:disabled {
		cursor: not-allowed;
		opacity: 0.45;
	}

	.link-btn {
		min-height: 26px;
		padding: 4px 8px;
		background: transparent;
		color: var(--accent-blue);
	}

	@media (max-width: 1080px) {
		.summary-grid {
			grid-template-columns: repeat(2, minmax(0, 1fr));
		}
	}

	@media (max-width: 900px) {
		.toolbar {
			grid-template-columns: 1fr;
		}

		.level-filter {
			overflow-x: auto;
		}
	}

	@media (max-width: 720px) {
		.page-header {
			flex-direction: column;
		}

		.terminal-head {
			align-items: flex-start;
			flex-direction: column;
		}
	}

	@media (max-width: 520px) {
		.summary-grid {
			grid-template-columns: 1fr;
		}

		.log-entry {
			grid-template-columns: 82px 54px minmax(0, 1fr);
		}
	}
</style>
