<script lang="ts">
	import { router } from '$lib/router.svelte';
	import { connection } from '$lib/stores/connection';
	import { logs } from '$lib/stores/logs';
	import { scripts, type ScriptInfo, type ScriptState } from '$lib/stores/scripts';
	import { scriptFiles, type ScriptFileEntry, type ScriptsRootInfo } from '$lib/stores/script-files';

	type StatusFilter = 'all' | ScriptState;

	let customPath = $state('scripts/example.lua');
	let busyId = $state<string | null>(null);
	let startingCustom = $state(false);
	let scriptQuery = $state('');
	let statusFilter = $state<StatusFilter>('all');
	let lastRefreshed = $state('-');
	/// 运行时长显示的秒级 tick
	let nowTick = $state(Date.now());

	// ---- 文件管理状态（本地文件系统，不依赖 runtime IPC） ----
	/// scriptFiles 是复合订阅形态（subscribe 为分片订阅函数集合），
	/// 无法用 $ 前缀自动订阅，这里手工汇入本地响应式快照
	let sf = $state<{
		entries: ScriptFileEntry[];
		truncated: boolean;
		root: ScriptsRootInfo;
		loading: boolean;
	}>({
		entries: [],
		truncated: false,
		root: { root: '', source: 'default' },
		loading: false,
	});
	let selectedPath = $state<string | null>(null);
	let preview = $state<{ path: string; content: string; size: number } | null>(null);
	let previewLoading = $state(false);
	let previewError = $state('');
	let newFilePath = $state('');
	let filesBusy = $state(false);
	/// 两段式删除确认：第一次点击进入待确认态
	let deleteArmed = $state<string | null>(null);

	const statusFilters: Array<{ value: StatusFilter; label: string }> = [
		{ value: 'all', label: '全部' },
		{ value: 'running', label: '运行中' },
		{ value: 'paused', label: '已暂停' },
		{ value: 'stopped', label: '已停止' },
		{ value: 'error', label: '错误' },
	];

	const stateLabels: Record<ScriptState, string> = {
		running: '运行中',
		paused: '已暂停',
		stopped: '已停止',
		loaded: '已加载',
		error: '错误',
		unknown: '未知',
	};

	let runningCount = $derived($scripts.filter(script => script.state === 'running').length);
	let pausedCount = $derived($scripts.filter(script => script.state === 'paused').length);
	let errorCount = $derived($scripts.filter(script => script.state === 'error').length);
	let filteredScripts = $derived($scripts.filter(script => {
		const query = scriptQuery.trim().toLowerCase();
		const matchesQuery = !query
			|| script.name.toLowerCase().includes(query)
			|| script.path.toLowerCase().includes(query);
		const matchesStatus = statusFilter === 'all' || script.state === statusFilter;
		return matchesQuery && matchesStatus;
	}));

	/// 可直接启动的脚本文件（.lua/.py），作为快捷卡数据源（取代旧硬编码三卡）
	const SCRIPT_EXTENSIONS = ['.lua', '.py', '.json', '.toml'];
	let runnableFiles = $derived(
		sf.entries.filter(item =>
			!item.is_dir
			&& SCRIPT_EXTENSIONS.some(ext => item.path.toLowerCase().endsWith(ext))
		)
	);
	let quickStartFiles = $derived(runnableFiles.slice(0, 6));

	$effect(() => {
		const timer = window.setInterval(() => { nowTick = Date.now(); }, 1000);
		return () => window.clearInterval(timer);
	});

	/// 订阅 scriptFiles 各分片 → 本地快照 sf
	$effect(() => {
		const unsubs = [
			scriptFiles.subscribe.entries(v => { sf.entries = v; }),
			scriptFiles.subscribe.truncated(v => { sf.truncated = v; }),
			scriptFiles.subscribe.root(v => { sf.root = v; }),
			scriptFiles.subscribe.loading(v => { sf.loading = v; }),
		];
		return () => unsubs.forEach(unsub => unsub());
	});

	/// 进入页面加载脚本根目录与文件列表（dev 模式填演示数据）
	$effect(() => {
		scriptFiles.loadRoot();
		scriptFiles.load();
	});

	function formatSize(bytes: number): string {
		if (bytes < 1024) return bytes + ' B';
		if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
		return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
	}

	function formatDuration(ms: number): string {
		if (ms < 0 || !Number.isFinite(ms)) return '-';
		const totalSeconds = Math.floor(ms / 1000);
		const days = Math.floor(totalSeconds / 86400);
		const hours = Math.floor((totalSeconds % 86400) / 3600);
		const minutes = Math.floor((totalSeconds % 3600) / 60);
		const seconds = totalSeconds % 60;
		if (days > 0) return `${days}d ${hours}h`;
		if (hours > 0) return `${hours}h ${minutes}m`;
		if (minutes > 0) return `${minutes}m ${seconds}s`;
		return `${seconds}s`;
	}

	/// 脚本已加载时长（自最近一次加载起）
	function scriptUptime(script: ScriptInfo): string {
		if (!script.loaded_at) return '-';
		return formatDuration(nowTick - script.loaded_at);
	}

	function fileName(path: string): string {
		return path.replace(/\\/g, '/').split('/').filter(Boolean).pop() || path;
	}

	function parentPath(path: string): string {
		const normalized = path.replace(/\\/g, '/');
		const index = normalized.lastIndexOf('/');
		return index > 0 ? normalized.slice(0, index) : '.';
	}

	type ScriptOp = 'start' | 'stop' | 'pause' | 'resume' | 'restart' | 'unload';

	/// 各状态下可用的操作（顺序即展示顺序）
	function opsFor(script: ScriptInfo): Array<{ op: ScriptOp; label: string; kind: 'primary' | 'plain' | 'danger' }> {
		switch (script.state) {
			case 'running':
				return [
					{ op: 'pause', label: '暂停', kind: 'plain' },
					{ op: 'stop', label: '停止', kind: 'danger' },
					{ op: 'restart', label: '重启', kind: 'plain' },
				];
			case 'paused':
				return [
					{ op: 'resume', label: '恢复', kind: 'primary' },
					{ op: 'stop', label: '停止', kind: 'danger' },
					{ op: 'restart', label: '重启', kind: 'plain' },
				];
			case 'loaded':
				return [
					{ op: 'start', label: '启动', kind: 'primary' },
					{ op: 'unload', label: '卸载', kind: 'plain' },
				];
			case 'error':
				return [
					{ op: 'restart', label: '重试', kind: 'primary' },
					{ op: 'unload', label: '卸载', kind: 'plain' },
				];
			default: // stopped / unknown
				return [
					{ op: 'start', label: '启动', kind: 'primary' },
					{ op: 'restart', label: '重启', kind: 'plain' },
					{ op: 'unload', label: '卸载', kind: 'plain' },
				];
		}
	}

	async function runOp(script: ScriptInfo, op: ScriptOp) {
		if (!$connection.connected) {
			logs.add('未连接到 runtime IPC', 'error');
			return;
		}
		busyId = script.id;
		try {
			switch (op) {
				case 'start': await scripts.start(script.id, script.path); break;
				case 'stop': await scripts.stop(script.id); break;
				case 'pause': await scripts.pause(script.id); break;
				case 'resume': await scripts.resume(script.id); break;
				case 'restart': await scripts.restart(script.id); break;
				case 'unload': await scripts.unload(script.id); break;
			}
			const label = opsFor(script).find(o => o.op === op)?.label || op;
			logs.add(`已${label}脚本: ${script.name || script.id}`, op === 'stop' || op === 'unload' ? 'info' : 'success');
			lastRefreshed = new Date().toLocaleTimeString();
		} catch (error: any) {
			const label = opsFor(script).find(o => o.op === op)?.label || op;
			logs.add(`${label}失败: ${error}`, 'error');
		} finally {
			busyId = null;
		}
	}

	async function bulkOp(op: 'pause' | 'resume' | 'stop') {
		if (!$connection.connected) {
			logs.add('未连接到 runtime IPC', 'error');
			return;
		}
		const targets = $scripts.filter(script =>
			op === 'pause' ? script.state === 'running'
			: op === 'resume' ? script.state === 'paused'
			: script.state === 'running' || script.state === 'paused'
		);
		if (targets.length === 0) {
			logs.add('没有符合条件的脚本', 'warning');
			return;
		}
		let done = 0;
		for (const script of targets) {
			try {
				if (op === 'pause') await scripts.pause(script.id);
				else if (op === 'resume') await scripts.resume(script.id);
				else await scripts.stop(script.id);
				done++;
			} catch { /* 单个失败继续 */ }
		}
		const label = op === 'pause' ? '暂停' : op === 'resume' ? '恢复' : '停止';
		logs.add(`批量${label}完成: ${done}/${targets.length}`, done === targets.length ? 'success' : 'warning');
		lastRefreshed = new Date().toLocaleTimeString();
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

	// ---- 文件管理（Rust 层 list/read/write/delete_script_file，路径约束在脚本根目录内） ----

	/// 文件树缩进深度（按路径段数）
	function depthOf(path: string): number {
		return path.replace(/\\/g, '/').split('/').length - 1;
	}

	function formatModified(ms: number): string {
		if (!ms) return '-';
		const diff = nowTick - ms;
		if (diff >= 0 && diff < 60_000) return '刚刚';
		return new Date(ms).toLocaleString();
	}

	async function selectFile(entry: ScriptFileEntry) {
		if (entry.is_dir) return;
		selectedPath = entry.path;
		deleteArmed = null;
		await previewFile(entry.path);
	}

	async function previewFile(path: string) {
		previewLoading = true;
		previewError = '';
		try {
			const content = await scriptFiles.read(path);
			preview = { path: content.path, content: content.content, size: content.size };
		} catch (error: any) {
			preview = null;
			previewError = String(error);
		} finally {
			previewLoading = false;
		}
	}

	async function refreshFiles() {
		filesBusy = true;
		try {
			await scriptFiles.load();
			await scriptFiles.loadRoot();
			logs.add('脚本文件列表已刷新', 'info');
		} catch (error: any) {
			logs.add(`刷新脚本文件失败: ${error}`, 'error');
		} finally {
			filesBusy = false;
		}
	}

	async function createFile() {
		const path = newFilePath.trim();
		if (!path) {
			logs.add('请输入新建文件的相对路径', 'warning');
			return;
		}
		filesBusy = true;
		try {
			await scriptFiles.create(path);
			newFilePath = '';
			selectedPath = path;
			await previewFile(path);
			logs.add(`已新建脚本文件: ${path}`, 'success');
		} catch (error: any) {
			logs.add(`新建文件失败: ${error}`, 'error');
		} finally {
			filesBusy = false;
		}
	}

	/// 两段式删除确认：第一次点击进入待确认，再次点击执行删除
	async function removeFile(path: string) {
		if (deleteArmed !== path) {
			deleteArmed = path;
			return;
		}
		deleteArmed = null;
		filesBusy = true;
		try {
			await scriptFiles.removeDev(path);
			if (selectedPath === path) {
				selectedPath = null;
				preview = null;
			}
			logs.add(`已删除脚本文件: ${path}`, 'info');
		} catch (error: any) {
			logs.add(`删除文件失败: ${error}`, 'error');
		} finally {
			filesBusy = false;
		}
	}

	async function runSelected() {
		if (!selectedPath) return;
		await startPath(selectedPath);
	}

	function changeRoot() {
		const next = window.prompt('设置脚本根目录（留空恢复默认 ./scripts）', sf.root.root);
		if (next === null) return;
		const trimmed = next.trim();
		if (!trimmed) {
			applyRoot('');
			return;
		}
		applyRoot(trimmed);
	}

	async function applyRoot(path: string) {
		filesBusy = true;
		try {
			await scriptFiles.setRoot(path);
			selectedPath = null;
			preview = null;
			logs.add(`脚本根目录已切换: ${sf.root.root}`, 'success');
		} catch (error: any) {
			logs.add(`设置脚本根目录失败: ${error}`, 'error');
		} finally {
			filesBusy = false;
		}
	}
</script>

<div class="scripts-page">
	<section class="page-header">
		<div>
			<h2 class="page-title">脚本管理</h2>
			<p class="page-subtitle">脚本生命周期管理 · 最近刷新 {lastRefreshed}</p>
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
			<span>已暂停</span>
			<strong class="yellow">{pausedCount}</strong>
			<small>paused</small>
		</div>
		<div class="metric-card">
			<span>错误</span>
			<strong class:red={errorCount > 0}>{$scripts.length > 0 ? errorCount : 0}</strong>
			<small>error</small>
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
				{#if quickStartFiles.length === 0}
					<div class="quick-empty">脚本目录暂无可启动文件，可在下方文件管理中新建</div>
				{:else}
					{#each quickStartFiles as item (item.path)}
						<button class="quick-card" onclick={() => startPath(item.path)} disabled={!$connection.connected || startingCustom}>
							<span class="quick-tag">{item.path.toLowerCase().endsWith('.py') ? 'python' : 'lua'}</span>
							<strong>{item.name}</strong>
							<span title={item.path}>{item.path}</span>
							<small>{formatSize(item.size)}</small>
						</button>
					{/each}
				{/if}
			</div>
		</div>

		<div class="runtime-panel card">
			<div class="panel-heading compact">
				<h3>批量操作</h3>
				<span class="status-note">{$connection.connected ? 'ready' : 'offline'}</span>
			</div>
			<div class="bulk-rows">
				<button class="bulk-btn" disabled={!$connection.connected || runningCount === 0} onclick={() => bulkOp('pause')}>
					<span class="bulk-icon pause">⏸</span>
					<span>全部暂停</span>
					<small>{runningCount} 个运行中</small>
				</button>
				<button class="bulk-btn" disabled={!$connection.connected || pausedCount === 0} onclick={() => bulkOp('resume')}>
					<span class="bulk-icon resume">▶</span>
					<span>全部恢复</span>
					<small>{pausedCount} 个已暂停</small>
				</button>
				<button class="bulk-btn danger" disabled={!$connection.connected || (runningCount === 0 && pausedCount === 0)} onclick={() => bulkOp('stop')}>
					<span class="bulk-icon stop">■</span>
					<span>全部停止</span>
					<small>{runningCount + pausedCount} 个活跃</small>
				</button>
			</div>
		</div>
	</section>

	<section class="card files-card">
		<div class="files-toolbar">
			<div class="panel-heading compact files-heading">
				<div>
					<span class="eyebrow">Files</span>
					<h3>脚本文件管理</h3>
				</div>
				<span class="root-path" title="脚本根目录（{sf.root.source}）">
					{sf.root.root || '未设置'}
				</span>
			</div>
			<div class="files-actions">
				<button class="btn btn-sm" onclick={refreshFiles} disabled={filesBusy}>
					<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<path d="M21 12a9 9 0 0 1-15.2 6.52"></path>
						<path d="M3 12A9 9 0 0 1 18.2 5.48"></path>
						<path d="M21 5v7h-7"></path>
						<path d="M3 19v-7h7"></path>
					</svg>
					刷新
				</button>
				<button class="btn btn-sm" onclick={changeRoot} disabled={filesBusy}>更改目录</button>
			</div>
		</div>

		<div class="files-body">
			<div class="files-tree">
				<div class="new-file-row">
					<input
						type="text"
						bind:value={newFilePath}
						placeholder="新建文件相对路径，如 scripts/new.lua"
						onkeydown={(event) => {
							if (event.key === 'Enter') createFile();
						}}
					/>
					<button class="btn btn-sm btn-primary" onclick={createFile} disabled={filesBusy || !newFilePath.trim()}>
						新建
					</button>
				</div>

				{#if sf.entries.length === 0}
					<div class="files-empty">
						<strong>{sf.loading ? '正在读取脚本目录…' : '脚本目录为空或不可访问'}</strong>
						<span>检查上方脚本根目录设置（默认 ./scripts）</span>
					</div>
				{:else}
					{#if sf.truncated}
						<div class="files-truncated">目录条目过多，列表已截断</div>
					{/if}
					<ul class="file-list">
						{#each sf.entries as entry (entry.path)}
							<li>
								<button
									class="file-row"
									class:dir={entry.is_dir}
									class:selected={selectedPath === entry.path}
									style="padding-left: {12 + depthOf(entry.path) * 14}px"
									onclick={() => selectFile(entry)}
									disabled={entry.is_dir}
									title={entry.path}
								>
									<span class="file-glyph">{entry.is_dir ? '▸' : '·'}</span>
									<span class="file-name">{entry.name}</span>
									<span class="file-meta">
										{#if !entry.is_dir}{formatSize(entry.size)} · {/if}{formatModified(entry.modified)}
									</span>
									{#if !entry.is_dir}
										<span
											class="file-delete"
											class:armed={deleteArmed === entry.path}
											role="button"
											tabindex="-1"
											title={deleteArmed === entry.path ? '再次点击确认删除' : '删除文件'}
											onclick={(event) => {
												event.stopPropagation();
												removeFile(entry.path);
											}}
											onkeydown={(event) => {
												if (event.key === 'Enter' || event.key === ' ') {
													event.stopPropagation();
													removeFile(entry.path);
												}
											}}
										>
											{deleteArmed === entry.path ? '确认' : '删'}
										</span>
									{/if}
								</button>
							</li>
						{/each}
					</ul>
				{/if}
			</div>

			<div class="files-preview">
				{#if !selectedPath}
					<div class="files-empty">
						<strong>未选择文件</strong>
						<span>点击左侧文件查看只读预览（编辑请用 VS Code）</span>
					</div>
				{:else}
					<div class="preview-toolbar">
						<span class="preview-path" title={selectedPath}>{selectedPath}</span>
						<div class="preview-actions">
							<button class="btn btn-sm btn-primary" onclick={runSelected} disabled={!$connection.connected || startingCustom || previewLoading}>
								启动
							</button>
							<button class="btn btn-sm" onclick={() => { if (selectedPath) previewFile(selectedPath); }} disabled={previewLoading}>
								重新读取
							</button>
							<button
								class="btn btn-sm {deleteArmed === selectedPath ? 'btn-danger' : ''}"
								onclick={() => { if (selectedPath) removeFile(selectedPath); }}
								disabled={filesBusy}
							>
								{deleteArmed === selectedPath ? '确认删除' : '删除'}
							</button>
						</div>
					</div>
					{#if previewLoading}
						<div class="files-empty"><strong>正在读取文件…</strong></div>
					{:else if previewError}
						<div class="preview-error">{previewError}</div>
					{:else if preview}
						<pre class="preview-code">{preview.content}</pre>
					{/if}
				{/if}
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
					<div class="script-item" class:error={script.state === 'error'}>
						<div class="file-badge" class:running={script.state === 'running'} class:paused={script.state === 'paused'} class:errored={script.state === 'error'}>
							{fileName(script.name || script.path).slice(0, 2).toUpperCase()}
						</div>
						<div class="script-info">
							<div class="script-title-row">
								<strong>{script.name || fileName(script.path)}</strong>
								<span class="status-chip st-{script.state}">
									<span class="status-dot"></span>
									{stateLabels[script.state]}
								</span>
								{#if script.loaded_at}
									<span class="uptime" title="自最近一次加载起的时长">⏱ {scriptUptime(script)}</span>
								{/if}
							</div>
							<div class="script-meta">
								<span title={script.path}>{parentPath(script.path)}/{fileName(script.path)}</span>
								{#if script.size > 0}
									<span>{formatSize(script.size)}</span>
								{/if}
								<span>ID {script.id}</span>
							</div>
							{#if script.error}
								<div class="script-error" title={script.error}>
									<span class="error-icon">!</span>
									<span class="error-text">{script.error}</span>
								</div>
							{/if}
						</div>
						<div class="script-actions">
							{#each opsFor(script) as entry}
								<button
									class="btn btn-sm {entry.kind === 'primary' ? 'btn-primary' : entry.kind === 'danger' ? 'btn-danger' : ''}"
									disabled={!$connection.connected || busyId === script.id}
									onclick={() => runOp(script, entry.op)}
								>
									{busyId === script.id ? '…' : entry.label}
								</button>
							{/each}
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

	.connection-pill.connected {
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

	.connection-pill.connected .dot {
		background: var(--accent-green);
	}

	.status-chip {
		min-height: 24px;
		padding: 3px 8px;
		border-radius: 6px;
		background: var(--bg-secondary);
	}

	.st-running { color: var(--accent-green); border-color: rgba(63, 185, 80, 0.45); background: rgba(63, 185, 80, 0.08); }
	.st-running .status-dot { background: var(--accent-green); }
	.st-paused { color: var(--accent-yellow); border-color: rgba(210, 153, 34, 0.45); background: rgba(210, 153, 34, 0.08); }
	.st-paused .status-dot { background: var(--accent-yellow); }
	.st-error { color: var(--accent-red); border-color: rgba(248, 81, 73, 0.45); background: rgba(248, 81, 73, 0.08); }
	.st-error .status-dot { background: var(--accent-red); }
	.st-loaded { color: var(--accent-blue); border-color: rgba(88, 166, 255, 0.45); background: rgba(88, 166, 255, 0.08); }
	.st-loaded .status-dot { background: var(--accent-blue); }
	.st-stopped .status-dot, .st-unknown .status-dot { background: var(--text-secondary); }

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
	.red { color: var(--accent-red) !important; }

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

	.bulk-rows {
		display: grid;
		gap: 8px;
	}

	.bulk-btn {
		display: grid;
		grid-template-columns: 30px minmax(0, 1fr) auto;
		gap: 8px;
		align-items: center;
		min-height: 46px;
		padding: 8px 12px;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		font-size: 13px;
		text-align: left;
		cursor: pointer;
		transition: all 0.18s;
	}

	.bulk-btn:hover:not(:disabled) {
		background: var(--surface-hover);
		border-color: var(--accent-blue);
	}

	.bulk-btn:disabled {
		opacity: 0.5;
		cursor: not-allowed;
	}

	.bulk-btn.danger:hover:not(:disabled) {
		border-color: var(--accent-red);
	}

	.bulk-icon {
		display: inline-flex;
		align-items: center;
		justify-content: center;
		width: 26px;
		height: 26px;
		border-radius: 6px;
		font-size: 12px;
	}

	.bulk-icon.pause { color: var(--accent-yellow); background: rgba(210, 153, 34, 0.12); }
	.bulk-icon.resume { color: var(--accent-green); background: rgba(63, 185, 80, 0.12); }
	.bulk-icon.stop { color: var(--accent-red); background: rgba(248, 81, 73, 0.12); }

	.bulk-btn small {
		color: var(--text-secondary);
		font-size: 11px;
		white-space: nowrap;
	}

	.files-card {
		overflow: hidden;
	}

	.files-toolbar {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 10px;
		flex-wrap: wrap;
		padding: 14px 16px;
		border-bottom: 1px solid var(--border-color);
	}

	.files-heading {
		margin-bottom: 0;
		min-width: 0;
	}

	.root-path {
		max-width: 420px;
		overflow: hidden;
		padding: 3px 8px;
		border: 1px solid var(--border-color);
		border-radius: 999px;
		background: var(--bg-tertiary);
		color: var(--text-secondary);
		font-size: 11px;
		font-family: var(--font-mono, monospace);
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.files-actions {
		display: flex;
		gap: 6px;
		flex-shrink: 0;
	}

	.files-body {
		display: grid;
		grid-template-columns: minmax(0, 1.1fr) minmax(0, 1fr);
		gap: 0;
	}

	.files-tree {
		display: flex;
		flex-direction: column;
		gap: 10px;
		padding: 14px 16px 16px;
		border-right: 1px solid var(--border-color);
		min-width: 0;
	}

	.new-file-row {
		display: grid;
		grid-template-columns: minmax(0, 1fr) auto;
		gap: 8px;
	}

	.new-file-row input {
		min-width: 0;
		min-height: 34px;
		padding: 6px 10px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		color: var(--text-primary);
		font-size: 12px;
	}

	.new-file-row input:focus {
		outline: none;
		border-color: var(--accent-blue);
		box-shadow: 0 0 0 3px var(--focus-ring);
	}

	.file-list {
		list-style: none;
		margin: 0;
		padding: 0;
		display: flex;
		flex-direction: column;
		gap: 2px;
		max-height: 380px;
		overflow-y: auto;
	}

	.file-row {
		display: grid;
		grid-template-columns: 16px minmax(0, 1fr) auto auto;
		gap: 8px;
		align-items: center;
		width: 100%;
		min-height: 32px;
		padding: 4px 10px;
		border: 1px solid transparent;
		border-radius: 6px;
		background: transparent;
		color: var(--text-primary);
		font-size: 12px;
		text-align: left;
		cursor: pointer;
		transition: background 0.15s, border-color 0.15s;
	}

	.file-row:hover:not(:disabled) {
		background: var(--surface-hover);
	}

	.file-row.selected {
		background: rgba(88, 166, 255, 0.1);
		border-color: rgba(88, 166, 255, 0.45);
	}

	.file-row:disabled {
		cursor: default;
	}

	.file-row.dir {
		color: var(--text-secondary);
		font-weight: 600;
	}

	.file-glyph {
		color: var(--text-secondary);
		text-align: center;
	}

	.file-name {
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.file-meta {
		color: var(--text-secondary);
		font-size: 11px;
		white-space: nowrap;
	}

	.file-delete {
		display: inline-flex;
		align-items: center;
		justify-content: center;
		min-width: 22px;
		min-height: 22px;
		padding: 0 5px;
		border: 1px solid var(--border-color);
		border-radius: 5px;
		background: var(--bg-secondary);
		color: var(--text-secondary);
		font-size: 11px;
		transition: all 0.15s;
	}

	.file-delete:hover {
		color: var(--accent-red);
		border-color: var(--accent-red);
	}

	.file-delete.armed {
		background: var(--accent-red);
		border-color: var(--accent-red);
		color: #fff;
	}

	.files-preview {
		display: flex;
		flex-direction: column;
		gap: 10px;
		padding: 14px 16px 16px;
		min-width: 0;
	}

	.preview-toolbar {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 8px;
		flex-wrap: wrap;
	}

	.preview-path {
		min-width: 0;
		overflow: hidden;
		color: var(--text-secondary);
		font-size: 12px;
		font-family: var(--font-mono, monospace);
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.preview-actions {
		display: flex;
		gap: 6px;
		flex-shrink: 0;
	}

	.preview-code {
		margin: 0;
		padding: 12px;
		max-height: 340px;
		overflow: auto;
		background: var(--bg-primary, var(--bg-tertiary));
		border: 1px solid var(--border-color);
		border-radius: 6px;
		color: var(--text-primary);
		font-size: 12px;
		line-height: 1.55;
		font-family: var(--font-mono, monospace);
		white-space: pre;
	}

	.preview-error {
		padding: 10px 12px;
		background: rgba(248, 81, 73, 0.08);
		border: 1px solid rgba(248, 81, 73, 0.45);
		border-radius: 6px;
		color: var(--accent-red);
		font-size: 12px;
	}

	.files-empty {
		display: grid;
		place-items: center;
		gap: 6px;
		min-height: 120px;
		padding: 20px;
		text-align: center;
		color: var(--text-secondary);
		background: var(--bg-tertiary);
		border: 1px dashed var(--border-color);
		border-radius: 8px;
		font-size: 12px;
	}

	.files-empty strong {
		color: var(--text-primary);
		font-size: 13px;
	}

	.files-truncated {
		padding: 5px 10px;
		background: rgba(210, 153, 34, 0.1);
		border: 1px solid rgba(210, 153, 34, 0.4);
		border-radius: 6px;
		color: var(--accent-yellow);
		font-size: 11px;
	}

	.quick-empty {
		grid-column: 1 / -1;
		display: grid;
		place-items: center;
		min-height: 84px;
		padding: 12px;
		border: 1px dashed var(--border-color);
		border-radius: 8px;
		color: var(--text-secondary);
		font-size: 12px;
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

	.script-item.error {
		border-color: rgba(248, 81, 73, 0.45);
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

	.file-badge.paused {
		color: var(--accent-yellow);
		border-color: rgba(210, 153, 34, 0.45);
		background: rgba(210, 153, 34, 0.08);
	}

	.file-badge.errored {
		color: var(--accent-red);
		border-color: rgba(248, 81, 73, 0.45);
		background: rgba(248, 81, 73, 0.08);
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
		flex-wrap: wrap;
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

	.uptime {
		color: var(--text-secondary);
		font-size: 11px;
		font-variant-numeric: tabular-nums;
		white-space: nowrap;
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

	.script-error {
		display: flex;
		align-items: center;
		gap: 6px;
		margin-top: 6px;
		max-width: 560px;
	}

	.error-icon {
		display: inline-flex;
		align-items: center;
		justify-content: center;
		width: 14px;
		height: 14px;
		flex-shrink: 0;
		border-radius: 50%;
		background: var(--accent-red);
		color: #fff;
		font-size: 10px;
		font-weight: 700;
	}

	.error-text {
		overflow: hidden;
		color: var(--accent-red);
		font-size: 12px;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.script-actions {
		display: flex;
		justify-content: flex-end;
		gap: 6px;
		flex-wrap: wrap;
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

		.files-body {
			grid-template-columns: 1fr;
		}

		.files-tree {
			border-right: none;
			border-bottom: 1px solid var(--border-color);
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
			flex: 1;
		}
	}

	@media (max-width: 560px) {
		.summary-grid {
			grid-template-columns: 1fr;
		}

		.script-title-row {
			align-items: flex-start;
			flex-direction: column;
			gap: 4px;
		}

		.script-meta {
			flex-direction: column;
			gap: 4px;
		}
	}
</style>
