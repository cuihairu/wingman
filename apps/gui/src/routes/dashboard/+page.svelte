<script lang="ts">
	import { connection } from '$lib/stores/connection';
	import { scripts } from '$lib/stores/scripts';

	let runningCount = $state(0);
	let uptimeDisplay = $state('00:00:00');
	let systemStatus = $state('-');

	function formatUptime(seconds: number): string {
		const hrs = Math.floor(seconds / 3600);
		const mins = Math.floor((seconds % 3600) / 60);
		const secs = seconds % 60;
		return `${hrs.toString().padStart(2, '0')}:${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
	}

	async function updateStatus() {
		const invoke = (window as any).__TAURI_INVOKE__;
		if (!invoke || !$connection.connected) return;
		try {
			const status = await invoke('get_system_status');
			runningCount = status.running_scripts;
			uptimeDisplay = formatUptime(status.uptime);
			systemStatus = status.paused ? '已暂停' : '运行中';
		} catch { /* ignore */ }
	}

	$effect(() => {
		if ($connection.connected) {
			updateStatus();
			const interval = setInterval(updateStatus, 3000);
			return () => clearInterval(interval);
		}
	});
</script>

<div class="page-header">
	<h2 class="page-title">仪表板</h2>
	<p class="page-subtitle">系统运行状态概览</p>
</div>

<div class="status-grid">
	<div class="status-card">
		<div class="status-card-label">运行脚本</div>
		<div class="status-card-value green">{runningCount}</div>
	</div>
	<div class="status-card">
		<div class="status-card-label">运行时间</div>
		<div class="status-card-value blue">{uptimeDisplay}</div>
	</div>
	<div class="status-card">
		<div class="status-card-label">系统状态</div>
		<div class="status-card-value">{systemStatus}</div>
	</div>
	<div class="status-card">
		<div class="status-card-label">内存使用</div>
		<div class="status-card-value yellow">-</div>
	</div>
</div>

<div class="card">
	<div class="card-header">
		<span class="card-title">快速操作</span>
	</div>
	<div class="card-body">
		<div style="display: flex; gap: 12px;">
			<button class="btn btn-primary">
				<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<polygon points="5 3 19 12 5 21 5 3"></polygon>
				</svg>
				启动全部
			</button>
			<button class="btn btn-danger">
				<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<rect x="6" y="4" width="4" height="16"></rect>
					<rect x="14" y="4" width="4" height="16"></rect>
				</svg>
				停止全部
			</button>
		</div>
	</div>
</div>

<style>
	.page-header { margin-bottom: 24px; }
	.page-title { font-size: 24px; font-weight: 600; color: var(--text-primary); margin-bottom: 8px; }
	.page-subtitle { font-size: 14px; color: var(--text-secondary); }

	.status-grid {
		display: grid;
		grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
		gap: 16px;
		margin-bottom: 24px;
	}

	.status-card {
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 8px;
		padding: 16px;
	}

	.status-card-label { font-size: 12px; color: var(--text-secondary); margin-bottom: 8px; }
	.status-card-value { font-size: 24px; font-weight: 600; color: var(--text-primary); }
	.status-card-value.green { color: var(--accent-green); }
	.status-card-value.blue { color: var(--accent-blue); }
	.status-card-value.yellow { color: var(--accent-yellow); }

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

	.btn-primary { background: var(--accent-green); color: white; border-color: var(--accent-green); }
	.btn-primary:hover { background: #2ea043; }

	.btn-danger { background: var(--accent-red); color: white; border-color: var(--accent-red); }
	.btn-danger:hover { background: #da3633; }
</style>
