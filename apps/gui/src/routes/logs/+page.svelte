<script lang="ts">
	import { logs, type LogEntry, type LogSource } from '$lib/stores/logs';

	let query = $state('');
	let selectedLevel = $state<LogEntry['type'] | 'all'>('all');
	let selectedSource = $state<LogSource | 'all'>('all');
	let autoScroll = $state(true);
	let logContainer = $state<HTMLDivElement | null>(null);

	/// 智能滚动：用户上滚离开底部时暂停跟随，并统计未读新日志
	let userScrolled = $state(false);
	let newCount = $state(0);
	let lastLength = 0;

	const dropped = logs.dropped;

	const levels: Array<{ value: LogEntry['type'] | 'all'; label: string }> = [
		{ value: 'all', label: '全部' },
		{ value: 'debug', label: '调试' },
		{ value: 'info', label: '信息' },
		{ value: 'success', label: '成功' },
		{ value: 'warning', label: '警告' },
		{ value: 'error', label: '错误' },
	];

	const sources: Array<{ value: LogSource | 'all'; label: string }> = [
		{ value: 'all', label: '全部' },
		{ value: 'gui', label: 'GUI' },
		{ value: 'runtime', label: 'Runtime' },
	];

	let filteredLogs = $derived($logs.filter(log => {
		const matchesLevel = selectedLevel === 'all' || log.type === selectedLevel;
		const matchesSource = selectedSource === 'all' || log.source === selectedSource;
		const keyword = query.trim().toLowerCase();
		const matchesQuery = !keyword
			|| log.message.toLowerCase().includes(keyword)
			|| log.time.includes(query.trim());
		return matchesLevel && matchesSource && matchesQuery;
	}));

	let counts = $derived({
		total: $logs.length,
		error: $logs.filter(log => log.type === 'error').length,
		warning: $logs.filter(log => log.type === 'warning').length,
		runtime: $logs.filter(log => log.source === 'runtime').length,
	});

	function exportLogs() {
		const content = filteredLogs
			.map(log => `${log.time} [${log.type.toUpperCase()}] [${log.source}] ${log.message}`)
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
		selectedSource = 'all';
	}

	function isAtBottom(el: HTMLElement): boolean {
		return el.scrollHeight - el.scrollTop - el.clientHeight < 24;
	}

	function handleScroll() {
		if (!logContainer) return;
		const atBottom = isAtBottom(logContainer);
		if (atBottom) {
			if (userScrolled) {
				userScrolled = false;
				newCount = 0;
			}
		} else {
			userScrolled = true;
		}
	}

	function scrollToBottom() {
		if (!logContainer) return;
		logContainer.scrollTop = logContainer.scrollHeight;
		userScrolled = false;
		newCount = 0;
	}

	async function copyEntry(log: LogEntry) {
		try {
			await navigator.clipboard.writeText(`${log.time} [${log.type.toUpperCase()}] [${log.source}] ${log.message}`);
		} catch {
			// 剪贴板不可用时静默忽略
		}
	}

	$effect(() => {
		const len = filteredLogs.length;
		const added = Math.max(0, len - lastLength);
		lastLength = len;
		if (!logContainer) return;
		if (!autoScroll) return;
		if (userScrolled) {
			if (added > 0) newCount += added;
			return;
		}
		requestAnimationFrame(() => {
			if (logContainer && !userScrolled) {
				logContainer.scrollTop = logContainer.scrollHeight;
			}
		});
	});
</script>

<div class="logs-page">
	<section class="page-header">
		<div>
			<h2 class="page-title">系统日志</h2>
			<p class="page-subtitle">GUI 与 runtime 日志实时流（runtime 事件 500ms 轮询拉取）。支持等级/来源筛选、搜索和导出。</p>
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
			<small>buffered (max 1000)</small>
		</div>
		<div class="metric-card">
			<span>Runtime 来源</span>
			<strong class="blue">{counts.runtime}</strong>
			<small>runtime events</small>
		</div>
		<div class="metric-card">
			<span>警告</span>
			<strong class="yellow">{counts.warning}</strong>
			<small>warning</small>
		</div>
		<div class="metric-card">
			<span>错误</span>
			<strong class="red">{counts.error}</strong>
			<small>error</small>
		</div>
	</section>

	<section class="card">
		{#if $dropped > 0}
			<div class="dropped-banner" title="runtime EventBuffer 有界，溢出时优先丢弃高频 log.line 事件">
				<span>runtime 事件缓冲溢出，累计丢弃 {$dropped} 条事件（log.line 优先）。提高日志级别或减少高频输出可缓解。</span>
			</div>
		{/if}

		<div class="toolbar">
			<div class="search-box">
				<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<circle cx="11" cy="11" r="8"></circle>
					<path d="m21 21-4.35-4.35"></path>
				</svg>
				<input type="text" placeholder="搜索日志内容或时间" bind:value={query} />
			</div>
			<div class="level-filter" role="group" aria-label="日志等级筛选">
				{#each levels as level}
					<button
						class:active={selectedLevel === level.value}
						onclick={() => selectedLevel = level.value}
					>
						{level.label}
					</button>
				{/each}
			</div>
			<div class="level-filter" role="group" aria-label="日志来源筛选">
				{#each sources as source}
					<button
						class:active={selectedSource === source.value}
						onclick={() => selectedSource = source.value}
					>
						{source.label}
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
					{#if query.trim() || selectedLevel !== 'all' || selectedSource !== 'all'}
						<button class="link-btn" onclick={clearFilters}>清除筛选</button>
					{/if}
				</div>
			</div>
			<div class="log-container" bind:this={logContainer} onscroll={handleScroll}>
				{#if filteredLogs.length === 0}
					<div class="empty-state">
						<strong>{$logs.length === 0 ? '还没有日志' : '没有匹配项'}</strong>
						<span>{$logs.length === 0 ? '连接 runtime 后，实时日志会出现在这里。' : '调整关键字、等级或来源后再试。'}</span>
					</div>
				{:else}
					{#each filteredLogs as log}
						<button class="log-entry" onclick={() => copyEntry(log)} title="点击复制整行">
							<span class="log-time">{log.time}</span>
							<span class="log-level log-{log.type}">{log.type.toUpperCase()}</span>
							<span class="log-source src-{log.source}">{log.source === 'gui' ? 'GUI' : 'RT'}</span>
							<span class="log-message">{log.message}</span>
						</button>
					{/each}
				{/if}
			</div>
			{#if autoScroll && userScrolled && newCount > 0}
				<button class="new-logs-jump" onclick={scrollToBottom}>
					↓ {newCount} 条新日志
				</button>
			{/if}
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
	.yellow { color: var(--accent-yellow) !important; }
	.red { color: var(--accent-red) !important; }

	.card {
		overflow: visible;
	}

	.dropped-banner {
		display: flex;
		align-items: center;
		gap: 8px;
		margin: 12px 16px 0;
		padding: 9px 12px;
		border: 1px solid rgba(210, 153, 34, 0.42);
		border-radius: 6px;
		background: rgba(210, 153, 34, 0.08);
		color: var(--accent-yellow);
		font-size: 12px;
		line-height: 1.5;
	}

	.toolbar {
		display: flex;
		flex-wrap: wrap;
		align-items: center;
		gap: 12px;
		padding: 14px 16px;
		border-bottom: 1px solid var(--border-color);
	}

	.search-box {
		display: flex;
		align-items: center;
		gap: 8px;
		flex: 1 1 240px;
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
		min-width: 44px;
		min-height: 28px;
		padding: 4px 8px;
		border-radius: 4px;
		background: transparent;
		color: var(--text-secondary);
		font-size: 12px;
		cursor: pointer;
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
		position: relative;
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
		height: min(620px, calc(100vh - 330px));
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
		grid-template-columns: 82px 54px 30px minmax(0, 1fr);
		gap: 8px;
		align-items: start;
		min-height: 22px;
		padding: 3px 6px;
		margin: 0 -6px;
		width: calc(100% + 12px);
		border: none;
		background: transparent;
		text-align: left;
		font: inherit;
		border-radius: 4px;
		cursor: copy;
	}

	.log-entry:hover {
		background: var(--bg-tertiary);
	}

	.log-time {
		color: var(--text-secondary);
	}

	.log-level {
		font-weight: 600;
	}

	.log-source {
		font-size: 10px;
		line-height: 18px;
		padding: 0 4px;
		border-radius: 3px;
		border: 1px solid var(--border-color);
		color: var(--text-secondary);
		text-align: center;
	}

	.src-runtime {
		color: var(--accent-blue);
		border-color: rgba(88, 166, 255, 0.4);
	}

	.log-message {
		color: var(--text-primary);
		white-space: pre-wrap;
		word-break: break-word;
	}

	.log-debug { color: var(--text-secondary); }
	.log-info { color: var(--accent-blue); }
	.log-success { color: var(--accent-green); }
	.log-warning { color: var(--accent-yellow); }
	.log-error { color: var(--accent-red); }

	.new-logs-jump {
		position: absolute;
		bottom: 28px;
		left: 50%;
		transform: translateX(-50%);
		display: inline-flex;
		align-items: center;
		gap: 6px;
		padding: 6px 14px;
		border: 1px solid var(--accent-blue);
		border-radius: 999px;
		background: var(--accent-blue);
		color: #fff;
		font-size: 12px;
		cursor: pointer;
		box-shadow: 0 6px 18px rgba(0, 0, 0, 0.4);
	}

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
			grid-template-columns: 74px 46px 26px minmax(0, 1fr);
		}
	}
</style>
