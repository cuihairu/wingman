<script lang="ts">
	import { logs } from '$lib/stores/logs';
</script>

<div class="page-header">
	<h2 class="page-title">系统日志</h2>
	<p class="page-subtitle">查看运行日志和错误信息</p>
</div>

<div class="card">
	<div class="card-header">
		<span class="card-title">实时日志</span>
		<button class="btn" style="font-size: 12px; padding: 4px 8px;" onclick={() => logs.clear()}>清空</button>
	</div>
	<div class="card-body">
		<div class="log-container">
			{#if $logs.length === 0}
				<div class="log-entry"><span class="log-time">[00:00:00]</span> 等待连接...</div>
			{:else}
				{#each $logs as log}
					<div class="log-entry">
						<span class="log-time">{log.time}</span>
						<span class="log-{log.type}">{log.message}</span>
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

	.btn {
		padding: 4px 8px;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		font-size: 12px;
		cursor: pointer;
		transition: all 0.2s;
	}

	.btn:hover { background: var(--border-color); }

	.log-container {
		background: #0d1117;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		height: 400px;
		overflow-y: auto;
		padding: 12px;
		font-family: 'Consolas', 'Monaco', monospace;
		font-size: 12px;
	}

	.log-entry { margin-bottom: 4px; }
	.log-time { color: var(--text-secondary); }
	.log-info { color: var(--accent-blue); }
	.log-success { color: var(--accent-green); }
	.log-warning { color: var(--accent-yellow); }
	.log-error { color: var(--accent-red); }
</style>
