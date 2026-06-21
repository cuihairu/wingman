<script lang="ts">
	import { scripts } from '$lib/stores/scripts';
	import { connection } from '$lib/stores/connection';
	import { logs } from '$lib/stores/logs';
	import { router } from '$lib/router.svelte';

	let customPath = $state('scripts/example.lua');
	let busyId = $state<string | null>(null);
	let startingCustom = $state(false);

	const quickScripts = [
		{ name: 'Lua 示例', path: 'scripts/example.lua', hint: '默认 Lua 自动化入口' },
		{ name: '触发器配置', path: 'config/triggers.lua', hint: '本地触发器配置脚本' },
		{ name: 'Python 示例', path: 'scripts/example.py', hint: '需启用 Python 引擎' },
	];

	function formatSize(bytes: number): string {
		if (bytes < 1024) return bytes + ' B';
		if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
		return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
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
		} catch (error: any) {
			logs.add(`操作失败: ${error}`, 'error');
		} finally {
			busyId = null;
		}
	}

	async function refreshScripts() {
		await scripts.load();
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
</script>

<div class="page-header">
	<div>
		<h2 class="page-title">脚本管理</h2>
		<p class="page-subtitle">从 runtime IPC 启动、停止和观察本地自动化脚本</p>
	</div>
	<div class="connection-pill" class:connected={$connection.connected}>
		<span class="dot"></span>
		{$connection.connected ? 'runtime IPC 已连接' : 'runtime IPC 未连接'}
	</div>
</div>

<div class="script-grid">
	<div class="card launcher-card">
		<div class="card-header">
			<span class="card-title">按路径启动</span>
			<button class="btn btn-sm" onclick={openLogs}>查看日志</button>
		</div>
		<div class="card-body">
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
					{startingCustom ? '启动中...' : '启动脚本'}
				</button>
			</div>
			<div class="helper-text">
				Runtime 会按路径加载脚本；已加载脚本会出现在下方列表。内容编辑仍建议使用外部编辑器或远程 Dashboard 的 Scripts 页面。
			</div>
			<div class="quick-list">
				{#each quickScripts as item}
					<button class="quick-card" onclick={() => startPath(item.path)} disabled={!$connection.connected || startingCustom}>
						<strong>{item.name}</strong>
						<span>{item.path}</span>
						<small>{item.hint}</small>
					</button>
				{/each}
			</div>
		</div>
	</div>

	<div class="card stats-card">
		<div class="card-header">
			<span class="card-title">运行概览</span>
		</div>
		<div class="card-body stats">
			<div class="stat">
				<strong>{$scripts.length}</strong>
				<span>已加载脚本</span>
			</div>
			<div class="stat">
				<strong>{$scripts.filter(script => script.is_running).length}</strong>
				<span>运行中</span>
			</div>
			<div class="stat">
				<strong>{$scripts.filter(script => !script.is_running).length}</strong>
				<span>已停止</span>
			</div>
		</div>
	</div>
</div>

<div class="card">
	<div class="card-header">
		<span class="card-title">脚本列表</span>
		<button class="btn btn-sm" onclick={refreshScripts}>刷新</button>
	</div>
	<div class="card-body">
		<div class="script-list">
			{#if $scripts.length === 0}
				<div class="empty-state">
					<div class="empty-state-icon">▣</div>
					<div>{$connection.connected ? '还没有加载脚本，先从上方按路径启动一个脚本' : '连接 runtime IPC 后可获取脚本列表'}</div>
				</div>
			{:else}
				{#each $scripts as script}
					<div class="script-item">
						<div class="script-info">
							<div class="script-name">{script.name}</div>
							<div class="script-meta">
								<span title={script.path}>{script.path}</span>
								<span>{formatSize(script.size)}</span>
								<div class="script-status">
									<div class="script-status-dot" class:running={script.is_running}></div>
									<span>{script.is_running ? '运行中' : '已停止'}</span>
								</div>
							</div>
						</div>
						<div class="script-actions">
							<button
								class="btn {script.is_running ? 'btn-danger' : 'btn-primary'}"
								style="font-size: 12px; padding: 6px 12px;"
								disabled={!$connection.connected || busyId === script.id}
								onclick={() => toggleScript(script.id, script.is_running)}
							>
								{busyId === script.id ? '处理中...' : script.is_running ? '停止' : '启动'}
							</button>
						</div>
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
	.page-title { font-size: 24px; font-weight: 600; color: var(--text-primary); margin-bottom: 8px; }
	.page-subtitle { font-size: 14px; color: var(--text-secondary); }

	.connection-pill {
		display: inline-flex;
		align-items: center;
		gap: 8px;
		padding: 8px 12px;
		border: 1px solid var(--border-color);
		border-radius: 999px;
		background: var(--bg-secondary);
		color: var(--text-secondary);
		font-size: 12px;
	}
	.connection-pill.connected { color: var(--accent-green); border-color: rgba(63, 185, 80, 0.45); }
	.dot { width: 8px; height: 8px; border-radius: 50%; background: var(--accent-red); }
	.connection-pill.connected .dot { background: var(--accent-green); }

	.script-grid {
		display: grid;
		grid-template-columns: minmax(0, 2fr) minmax(240px, 1fr);
		gap: 16px;
		margin-bottom: 16px;
	}

	.card {
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 8px;
		margin-bottom: 16px;
	}

	.card-header {
		padding: 16px;
		border-bottom: 1px solid var(--border-color);
		display: flex;
		align-items: center;
		justify-content: space-between;
	}

	.card-title { font-size: 15px; font-weight: 500; color: var(--text-primary); }
	.card-body { padding: 16px; }

	.path-launcher {
		display: grid;
		grid-template-columns: 1fr auto;
		gap: 8px;
	}

	.path-launcher input {
		padding: 9px 10px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		color: var(--text-primary);
		font-size: 13px;
	}
	.path-launcher input:focus { outline: none; border-color: var(--accent-blue); }

	.helper-text {
		margin-top: 10px;
		font-size: 12px;
		line-height: 1.6;
		color: var(--text-secondary);
	}

	.quick-list {
		display: grid;
		grid-template-columns: repeat(3, minmax(0, 1fr));
		gap: 8px;
		margin-top: 14px;
	}

	.quick-card {
		display: flex;
		flex-direction: column;
		gap: 4px;
		text-align: left;
		padding: 12px;
		border: 1px solid var(--border-color);
		border-radius: 8px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
	}
	.quick-card:hover:not(:disabled) { border-color: var(--accent-blue); }
	.quick-card span, .quick-card small { color: var(--text-secondary); font-size: 11px; overflow: hidden; text-overflow: ellipsis; }
	.quick-card:disabled { opacity: 0.55; cursor: not-allowed; }

	.stats { display: grid; gap: 10px; }
	.stat {
		display: flex;
		align-items: baseline;
		justify-content: space-between;
		padding: 12px;
		background: var(--bg-tertiary);
		border-radius: 8px;
	}
	.stat strong { font-size: 24px; color: var(--accent-blue); }
	.stat span { font-size: 12px; color: var(--text-secondary); }

	.script-list { display: flex; flex-direction: column; gap: 8px; }

	.script-item {
		display: flex;
		align-items: center;
		padding: 12px 16px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		transition: all 0.2s;
	}

	.script-item:hover { border-color: var(--text-secondary); }
	.script-info { flex: 1; }
	.script-name { font-size: 14px; font-weight: 500; color: var(--text-primary); margin-bottom: 4px; }
	.script-meta { font-size: 12px; color: var(--text-secondary); display: flex; gap: 12px; flex-wrap: wrap; }
	.script-status { display: flex; align-items: center; gap: 4px; }
	.script-status-dot { width: 6px; height: 6px; border-radius: 50%; background: var(--text-secondary); }
	.script-status-dot.running { background: var(--accent-green); }
	.script-actions { display: flex; gap: 8px; }

	.btn {
		display: inline-flex;
		align-items: center;
		gap: 6px;
		padding: 8px 16px;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		font-size: 14px;
		cursor: pointer;
		transition: all 0.2s;
	}

	.btn:hover { background: var(--border-color); }
	.btn:disabled { opacity: 0.55; cursor: not-allowed; }
	.btn-sm { padding: 6px 12px; font-size: 12px; }
	.btn-primary { background: var(--accent-green); color: white; border-color: var(--accent-green); }
	.btn-primary:hover { background: #2ea043; }
	.btn-danger { background: var(--accent-red); color: white; border-color: var(--accent-red); }
	.btn-danger:hover { background: #da3633; }

	.empty-state { text-align: center; padding: 60px 20px; color: var(--text-secondary); }
	.empty-state-icon { font-size: 48px; margin-bottom: 16px; opacity: 0.5; }

	@media (max-width: 900px) {
		.script-grid { grid-template-columns: 1fr; }
		.quick-list { grid-template-columns: 1fr; }
		.path-launcher { grid-template-columns: 1fr; }
		.page-header { flex-direction: column; }
	}
</style>
