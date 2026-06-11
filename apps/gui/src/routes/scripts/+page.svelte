<script lang="ts">
	import { scripts } from '$lib/stores/scripts';
	import { connection } from '$lib/stores/connection';
	import { logs } from '$lib/stores/logs';

	function formatSize(bytes: number): string {
		if (bytes < 1024) return bytes + ' B';
		if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
		return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
	}

	async function toggleScript(id: string, isRunning: boolean) {
		if (!$connection.connected) {
			logs.add('未连接到服务器', 'error');
			return;
		}
		const script = $scripts.find(item => item.id === id);
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
		}
	}

	async function refreshScripts() {
		await scripts.load();
		logs.add('脚本列表已刷新', 'info');
	}
</script>

<div class="page-header">
	<h2 class="page-title">脚本管理</h2>
	<p class="page-subtitle">管理和控制自动化脚本</p>
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
					<div class="empty-state-icon">📜</div>
					<div>{$connection.connected ? '没有找到脚本' : '点击刷新获取脚本列表'}</div>
				</div>
			{:else}
				{#each $scripts as script}
					<div class="script-item">
						<div class="script-info">
							<div class="script-name">{script.name}</div>
							<div class="script-meta">
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
								onclick={() => toggleScript(script.id, script.is_running)}
							>
								{script.is_running ? '停止' : '启动'}
							</button>
						</div>
					</div>
				{/each}
			{/if}
		</div>
	</div>
</div>

<style>
	.page-header { margin-bottom: 24px; }
	.page-title { font-size: 24px; font-weight: 600; color: var(--text-primary); margin-bottom: 8px; }
	.page-subtitle { font-size: 14px; color: var(--text-secondary); }

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
	.script-meta { font-size: 12px; color: var(--text-secondary); display: flex; gap: 12px; }
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
	.btn-sm { padding: 6px 12px; font-size: 12px; }
	.btn-primary { background: var(--accent-green); color: white; border-color: var(--accent-green); }
	.btn-primary:hover { background: #2ea043; }
	.btn-danger { background: var(--accent-red); color: white; border-color: var(--accent-red); }
	.btn-danger:hover { background: #da3633; }

	.empty-state { text-align: center; padding: 60px 20px; color: var(--text-secondary); }
	.empty-state-icon { font-size: 48px; margin-bottom: 16px; opacity: 0.5; }
</style>
