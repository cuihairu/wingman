<script lang="ts">
	import { router } from '$lib/router.svelte';
	import { connection } from '$lib/stores/connection';
	import { logs } from '$lib/stores/logs';
	import { scripts } from '$lib/stores/scripts';

	type StatusFilter = 'all' | 'running' | 'stopped';

	let customPath = $state('scripts/example.lua');
	let busyId = $state<string | null>(null);
	let startingCustom = $state(false);
	let scriptQuery = $state('');
	let statusFilter = $state<StatusFilter>('all');
	let lastRefreshed = $state('-');

	const quickScripts = [
		{ name: 'Lua 示例', path: 'scripts/example.lua', hint: 'Lua', tag: 'default' },
		{ name: '触发器配置', path: 'config/triggers.lua', hint: 'Trigger', tag: 'config' },
		{ name: 'Python 示例', path: 'scripts/example.py', hint: 'Python', tag: 'optional' },
	];

	const statusFilters: Array<{ value: StatusFilter; label: string }> = [
		{ value: 'all', label: '全部' },
		{ value: 'running', label: '运行中' },
		{ value: 'stopped', label: '已停止' },
	];

	let runningCount = $derived($scripts.filter(script => script.is_running).length);
	let stoppedCount = $derived(Math.max($scripts.length - runningCount, 0));
	let filteredScripts = $derived($scripts.filter(script => {
		const query = scriptQuery.trim().toLowerCase();
		const matchesQuery = !query
			|| script.name.toLowerCase().includes(query)
			|| script.path.toLowerCase().includes(query);
		const matchesStatus = statusFilter === 'all'
			|| (statusFilter === 'running' && script.is_running)
			|| (statusFilter === 'stopped' && !script.is_running);
		return matchesQuery && matchesStatus;
	}));

	function formatSize(bytes: number): string {
		if (bytes < 1024) return bytes + ' B';
		if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
		return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
	}

	function fileName(path: string): string {
		return path.replace(/\\/g, '/').split('/').filter(Boolean).pop() || path;
	}

	function parentPath(path: string): string {
		const normalized = path.replace(/\\/g, '/');
		const index = normalized.lastIndexOf('/');
		return index > 0 ? normalized.slice(0, index) : '.';
	}

	async function toggleScript(id: string, isRunning: boolean) {
		if (!$connection.connected) {
			logs.add('未连接到 runtime IPC', 'error');
			return;
		}
		const script = $scripts.find(item => item.id === id);
		busyId = id;
		try {
			if (isRunning) {
				await scripts.stop(id);
				logs.add(`已停止脚本: ${script?.name || id}`, 'info');
			} else {
				await scripts.start(id, script?.path);
				logs.add(`已启动脚本: ${script?.name || id}`, 'success');
			}
			lastRefreshed = new Date().toLocaleTimeString();
		} catch (error: any) {
			logs.add(`操作失败: ${error}`, 'error');
		} finally {
			busyId = null;
		}
	}

	async function refreshScripts() {
		await scripts.load();
		lastRefreshed = new Date().toLocaleTimeString();
		logs.add('脚本列表已刷新', 'info');
	}

	async function startPath(path = customPath) {
		const scriptPath = path.trim();
		if (!scriptPath) {
			logs.add('请输入脚本路径', 'warning');
			return;
		}
		if (!$connection.connected) {
			logs.add('未连接到 runtime IPC', 'error');
			return;
		}
		startingCustom = true;
		try {
			await scripts.start(scriptPath, scriptPath);
			customPath = scriptPath;
			lastRefreshed = new Date().toLocaleTimeString();
			logs.add(`已启动脚本: ${scriptPath}`, 'success');
		} catch (error: any) {
			logs.add(`启动失败: ${error}`, 'error');
		} finally {
			startingCustom = false;
		}
	}

	function openLogs() {
		router.navigate('logs');
	}

	function clearFilters() {
		scriptQuery = '';
		statusFilter = 'all';
	}
</script>

<div class="scripts-page">
	<section class="page-header">
		<div>
			<h2 class="page-title">脚本管理</h2>
			<p class="page-subtitle">本地脚本启动器 · 最近刷新 {lastRefreshed}</p>
		</div>
		<div class="header-actions">
			<div class="connection-pill" class:connected={$connection.connected}>
				<span class="dot"></span>
				{$connection.connected ? 'runtime IPC 已连接' : 'runtime IPC 未连接'}
			</div>
			<button class="btn" onclick={openLogs}>
				<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path>
					<path d="M14 2v6h6"></path>
					<path d="M8 13h8"></path>
					<path d="M8 17h5"></path>
				</svg>
				日志
			</button>
		</div>
	</section>

	<section class="summary-grid">
		<div class="metric-card">
			<span>已加载</span>
			<strong>{$scripts.length}</strong>
			<small>脚本总数</small>
		</div>
		<div class="metric-card">
			<span>运行中</span>
			<strong class="green">{runningCount}</strong>
			<small>active</small>
		</div>
		<div class="metric-card">
			<span>已停止</span>
			<strong>{stoppedCount}</strong>
			<small>idle</small>
		</div>
		<div class="metric-card">
			<span>筛选结果</span>
			<strong class="blue">{filteredScripts.length}</strong>
			<small>当前列表</small>
		</div>
	</section>

	<section class="launcher-panel">
		<div class="launcher-main card">
			<div class="panel-heading">
				<div>
					<span class="eyebrow">Launch</span>
					<h3>按路径启动</h3>
				</div>
				<span class="status-note">{$connection.connected ? 'ready' : 'offline'}</span>
			</div>
			<div class="path-launcher">
				<input
					type="text"
					bind:value={customPath}
					placeholder="scripts/example.lua 或绝对路径"
					onkeydown={(event) => {
						if (event.key === 'Enter') startPath();
					}}
				/>
				<button
					class="btn btn-primary"
					disabled={!$connection.connected || startingCustom}
					onclick={() => startPath()}
				>
					<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<polygon points="5 3 19 12 5 21 5 3"></polygon>
					</svg>
					{startingCustom ? '启动中' : '启动脚本'}
				</button>
			</div>
			<div class="quick-list">
				{#each quickScripts as item}
					<button class="quick-card" onclick={() => startPath(item.path)} disabled={!$connection.connected || startingCustom}>
						<span class="quick-tag">{item.tag}</span>
						<strong>{item.name}</strong>
						<span title={item.path}>{item.path}</span>
						<small>{item.hint}</small>
					</button>
				{/each}
			</div>
		</div>

		<div class="runtime-panel card">
			<div class="panel-heading compact">
				<h3>运行态</h3>
				<button class="link-btn" onclick={refreshScripts}>刷新</button>
			</div>
			<div class="runtime-rows">
				<div>
					<span>连接</span>
					<strong class:green={$connection.connected}>{$connection.connected ? '已连接' : '未连接'}</strong>
				</div>
				<div>
					<span>版本</span>
					<strong title={$connection.version}>{$connection.version}</strong>
				</div>
				<div>
					<span>暂停</span>
					<strong class:yellow={$connection.paused}>{$connection.paused ? '是' : '否'}</strong>
				</div>
			</div>
		</div>
	</section>

	<section class="card list-card">
		<div class="list-toolbar">
			<div class="search-box">
				<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<circle cx="11" cy="11" r="8"></circle>
					<path d="m21 21-4.35-4.35"></path>
				</svg>
				<input type="text" placeholder="搜索名称或路径" bind:value={scriptQuery} />
			</div>
			<div class="segmented-control">
				{#each statusFilters as filter}
					<button
						class:active={statusFilter === filter.value}
						onclick={() => statusFilter = filter.value}
					>
						{filter.label}
					</button>
				{/each}
			</div>
			<button class="btn" onclick={refreshScripts}>
				<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<path d="M21 12a9 9 0 0 1-15.2 6.52"></path>
					<path d="M3 12A9 9 0 0 1 18.2 5.48"></path>
					<path d="M21 5v7h-7"></path>
					<path d="M3 19v-7h7"></path>
				</svg>
				刷新
			</button>
		</div>

		<div class="script-list">
			{#if $scripts.length === 0}
				<div class="empty-state">
					<div class="empty-icon">
						<svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
							<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path>
							<path d="M14 2v6h6"></path>
						</svg>
					</div>
					<strong>{$connection.connected ? '还没有加载脚本' : '等待 runtime IPC 连接'}</strong>
					<span>{$connection.connected ? '从上方启动一个路径后会出现在这里' : '连接后可读取 runtime 中的脚本列表'}</span>
				</div>
			{:else if filteredScripts.length === 0}
				<div class="empty-state">
					<strong>没有匹配脚本</strong>
					<span>调整搜索词或状态筛选</span>
					<button class="btn btn-sm" onclick={clearFilters}>清除筛选</button>
				</div>
			{:else}
				{#each filteredScripts as script (script.id)}
					<div class="script-item">
						<div class="file-badge" class:running={script.is_running}>
							{fileName(script.name || script.path).slice(0, 2).toUpperCase()}
						</div>
						<div class="script-info">
							<div class="script-title-row">
								<strong>{script.name || fileName(script.path)}</strong>
								<span class="status-chip" class:running={script.is_running}>
									<span class="status-dot"></span>
									{script.is_running ? '运行中' : '已停止'}
								</span>
							</div>
							<div class="script-meta">
								<span title={script.path}>{parentPath(script.path)}/{fileName(script.path)}</span>
								<span>{formatSize(script.size)}</span>
								<span>ID {script.id}</span>
							</div>
						</div>
						<div class="script-actions">
							<button
								class="btn {script.is_running ? 'btn-danger' : 'btn-primary'}"
								disabled={!$connection.connected || busyId === script.id}
								onclick={() => toggleScript(script.id, script.is_running)}
							>
								{#if script.is_running}
									<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
										<rect x="6" y="6" width="12" height="12"></rect>
									</svg>
								{:else}
									<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
										<polygon points="5 3 19 12 5 21 5 3"></polygon>
									</svg>
								{/if}
								{busyId === script.id ? '处理中' : script.is_running ? '停止' : '启动'}
							</button>
						</div>
					</div>
				{/each}
			{/if}
		</div>
	</section>
</div>

<style>
	.scripts-page {
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
		font-size: 24px;
		font-weight: 600;
		color: var(--text-primary);
		margin-bottom: 8px;
	}

	.page-subtitle {
		font-size: 13px;
		color: var(--text-secondary);
	}

	.header-actions {
		display: flex;
		align-items: center;
		gap: 8px;
		flex-wrap: wrap;
		justify-content: flex-end;
	}

	.connection-pill,
	.status-chip {
		display: inline-flex;
		align-items: center;
		gap: 7px;
		min-height: 32px;
		padding: 6px 10px;
		border: 1px solid var(--border-color);
		border-radius: 999px;
		background: var(--bg-secondary);
		color: var(--text-secondary);
		font-size: 12px;
		white-space: nowrap;
	}

	.connection-pill.connected,
	.status-chip.running {
		color: var(--accent-green);
		border-color: rgba(63, 185, 80, 0.45);
		background: rgba(63, 185, 80, 0.08);
	}

	.dot,
	.status-dot {
		width: 7px;
		height: 7px;
		border-radius: 50%;
		background: var(--accent-red);
		flex-shrink: 0;
	}

	.connection-pill.connected .dot,
	.status-chip.running .status-dot {
		background: var(--accent-green);
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

	.green { color: var(--accent-green) !important; }
	.blue { color: var(--accent-blue) !important; }
	.yellow { color: var(--accent-yellow) !important; }

	.launcher-panel {
		display: grid;
		grid-template-columns: minmax(0, 1.7fr) minmax(280px, 0.7fr);
		gap: 12px;
		align-items: stretch;
	}

	.launcher-main,
	.runtime-panel {
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
	.status-note {
		display: block;
		color: var(--text-secondary);
		font-size: 11px;
		text-transform: uppercase;
	}

	.panel-heading h3 {
		margin-top: 3px;
		font-size: 16px;
		font-weight: 600;
		color: var(--text-primary);
	}

	.status-note {
		padding: 3px 7px;
		border: 1px solid var(--border-color);
		border-radius: 999px;
		background: var(--bg-tertiary);
	}

	.path-launcher {
		display: grid;
		grid-template-columns: minmax(0, 1fr) auto;
		gap: 8px;
	}

	.path-launcher input,
	.search-box input {
		min-width: 0;
		background: transparent;
		border: none;
		outline: none;
		color: var(--text-primary);
		font-size: 13px;
	}

	.path-launcher input {
		width: 100%;
		min-height: 38px;
		padding: 8px 10px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
	}

	.path-launcher input:focus {
		border-color: var(--accent-blue);
		box-shadow: 0 0 0 3px var(--focus-ring);
	}

	.quick-list {
		display: grid;
		grid-template-columns: repeat(3, minmax(0, 1fr));
		gap: 8px;
		margin-top: 12px;
	}

	.quick-card {
		position: relative;
		display: flex;
		flex-direction: column;
		gap: 5px;
		min-height: 102px;
		padding: 12px;
		text-align: left;
		border: 1px solid var(--border-color);
		border-radius: 8px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		transition: border-color 0.18s, background 0.18s;
	}

	.quick-card:hover:not(:disabled) {
		background: var(--surface-hover);
		border-color: var(--accent-blue);
	}

	.quick-card:disabled {
		opacity: 0.55;
		cursor: not-allowed;
	}

	.quick-card strong,
	.quick-card span:not(.quick-tag),
	.quick-card small {
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.quick-card strong {
		font-size: 13px;
	}

	.quick-card span:not(.quick-tag),
	.quick-card small {
		color: var(--text-secondary);
		font-size: 11px;
	}

	.quick-tag {
		align-self: flex-start;
		max-width: 100%;
		padding: 2px 6px;
		border-radius: 999px;
		background: rgba(88, 166, 255, 0.1);
		color: var(--accent-blue);
		font-size: 10px;
	}

	.runtime-rows {
		display: grid;
		gap: 8px;
	}

	.runtime-rows div {
		display: grid;
		grid-template-columns: 64px minmax(0, 1fr);
		gap: 10px;
		align-items: center;
		min-height: 36px;
		padding: 8px 10px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
	}

	.runtime-rows span {
		color: var(--text-secondary);
		font-size: 12px;
	}

	.runtime-rows strong {
		overflow: hidden;
		color: var(--text-primary);
		font-size: 13px;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.list-card {
		overflow: hidden;
	}

	.list-toolbar {
		display: grid;
		grid-template-columns: minmax(220px, 1fr) auto auto;
		gap: 10px;
		align-items: center;
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

	.segmented-control {
		display: flex;
		gap: 3px;
		padding: 3px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
	}

	.segmented-control button {
		min-width: 54px;
		min-height: 28px;
		padding: 4px 8px;
		border-radius: 4px;
		background: transparent;
		color: var(--text-secondary);
		font-size: 12px;
	}

	.segmented-control button:hover,
	.segmented-control button.active {
		background: var(--surface-hover);
		color: var(--text-primary);
	}

	.script-list {
		display: flex;
		flex-direction: column;
		gap: 8px;
		padding: 14px 16px 16px;
	}

	.script-item {
		display: grid;
		grid-template-columns: 40px minmax(0, 1fr) auto;
		gap: 12px;
		align-items: center;
		min-height: 70px;
		padding: 12px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 8px;
		transition: border-color 0.18s, background 0.18s;
	}

	.script-item:hover {
		background: var(--bg-elevated);
		border-color: var(--text-secondary);
	}

	.file-badge {
		display: flex;
		align-items: center;
		justify-content: center;
		width: 40px;
		height: 40px;
		border: 1px solid var(--border-color);
		border-radius: 8px;
		background: var(--bg-secondary);
		color: var(--text-secondary);
		font-size: 11px;
		font-weight: 700;
	}

	.file-badge.running {
		color: var(--accent-green);
		border-color: rgba(63, 185, 80, 0.45);
		background: rgba(63, 185, 80, 0.08);
	}

	.script-info {
		min-width: 0;
	}

	.script-title-row {
		display: flex;
		align-items: center;
		gap: 10px;
		margin-bottom: 6px;
		min-width: 0;
	}

	.script-title-row strong {
		min-width: 0;
		overflow: hidden;
		color: var(--text-primary);
		font-size: 14px;
		font-weight: 600;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.status-chip {
		min-height: 24px;
		padding: 3px 8px;
		border-radius: 6px;
		background: var(--bg-secondary);
	}

	.script-meta {
		display: flex;
		flex-wrap: wrap;
		gap: 8px 12px;
		color: var(--text-secondary);
		font-size: 12px;
	}

	.script-meta span {
		max-width: 420px;
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.script-actions {
		display: flex;
		justify-content: flex-end;
	}

	.btn,
	.link-btn {
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
		cursor: pointer;
		transition: all 0.18s;
		white-space: nowrap;
	}

	.btn:hover,
	.link-btn:hover {
		background: var(--surface-hover);
	}

	.btn:disabled {
		opacity: 0.5;
		cursor: not-allowed;
	}

	.btn-sm {
		min-height: 30px;
		padding: 5px 10px;
		font-size: 12px;
	}

	.link-btn {
		min-height: 28px;
		padding: 4px 8px;
		background: transparent;
		color: var(--accent-blue);
	}

	.btn-primary {
		background: var(--accent-green);
		color: white;
		border-color: var(--accent-green);
	}

	.btn-primary:hover {
		background: var(--accent-green-hover);
	}

	.btn-danger {
		background: var(--accent-red);
		color: white;
		border-color: var(--accent-red);
	}

	.btn-danger:hover {
		background: var(--accent-red-hover);
	}

	.empty-state {
		display: grid;
		place-items: center;
		gap: 8px;
		min-height: 210px;
		padding: 28px;
		text-align: center;
		color: var(--text-secondary);
		background: var(--bg-tertiary);
		border: 1px dashed var(--border-color);
		border-radius: 8px;
		font-size: 13px;
	}

	.empty-state strong {
		color: var(--text-primary);
		font-size: 14px;
	}

	.empty-icon {
		display: grid;
		place-items: center;
		width: 48px;
		height: 48px;
		border: 1px solid var(--border-color);
		border-radius: 8px;
		color: var(--text-secondary);
		background: var(--bg-secondary);
	}

	@media (max-width: 1080px) {
		.summary-grid {
			grid-template-columns: repeat(2, minmax(0, 1fr));
		}

		.launcher-panel {
			grid-template-columns: 1fr;
		}
	}

	@media (max-width: 820px) {
		.page-header,
		.header-actions {
			align-items: stretch;
			flex-direction: column;
		}

		.header-actions {
			justify-content: flex-start;
		}

		.quick-list,
		.path-launcher,
		.list-toolbar {
			grid-template-columns: 1fr;
		}

		.segmented-control {
			width: 100%;
		}

		.segmented-control button {
			flex: 1;
		}

		.script-item {
			grid-template-columns: 40px minmax(0, 1fr);
		}

		.script-actions {
			grid-column: 1 / -1;
			justify-content: stretch;
		}

		.script-actions .btn {
			width: 100%;
		}
	}

	@media (max-width: 560px) {
		.summary-grid {
			grid-template-columns: 1fr;
		}

		.script-title-row {
			align-items: flex-start;
			flex-direction: column;
		}

		.script-meta {
			flex-direction: column;
			gap: 4px;
		}
	}
</style>
