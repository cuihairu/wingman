<script lang="ts">
	import { connection } from '$lib/stores/connection';

	async function handleRefresh() {
		if ($connection.connected) {
			await connection.refresh();
		}
	}
</script>

<header class="top-bar">
	<div class="top-bar-left">
		<div class="connection-status">
			<div class="status-dot" class:connected={$connection.connected} class:error={!$connection.connected}></div>
			<span>{$connection.connected ? '已连接' : '未连接'}</span>
		</div>
		<span class="version-info">{$connection.version}</span>
	</div>
	<div class="top-bar-right">
		{#if $connection.connected}
			<button class="btn btn-warning" onclick={() => connection.togglePause()}>
				<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					{#if $connection.paused}
						<polygon points="5 3 19 12 5 21 5 3"></polygon>
					{:else}
						<rect x="6" y="4" width="4" height="16"></rect>
						<rect x="14" y="4" width="4" height="16"></rect>
					{/if}
				</svg>
				{$connection.paused ? '恢复运行' : '暂停全部'}
			</button>
		{/if}
		<button class="btn btn-icon" title="刷新" onclick={handleRefresh}>
			<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
				<polyline points="23 4 23 10 17 10"></polyline>
				<polyline points="1 20 1 14 7 14"></polyline>
				<path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"></path>
			</svg>
		</button>
	</div>
</header>

<style>
	.top-bar {
		height: 56px;
		background: var(--bg-secondary);
		border-bottom: 1px solid var(--border-color);
		display: flex;
		align-items: center;
		justify-content: space-between;
		padding: 0 20px;
	}

	.top-bar-left, .top-bar-right {
		display: flex;
		align-items: center;
		gap: 12px;
	}

	.connection-status {
		display: flex;
		align-items: center;
		gap: 8px;
		font-size: 13px;
		padding: 6px 12px;
		background: var(--bg-tertiary);
		border-radius: 20px;
	}

	.status-dot {
		width: 8px;
		height: 8px;
		border-radius: 50%;
		background: var(--text-secondary);
	}

	.status-dot.connected {
		background: var(--accent-green);
		box-shadow: 0 0 8px rgba(63, 185, 80, 0.5);
	}

	.status-dot.error {
		background: var(--accent-red);
	}

	.version-info {
		font-size: 13px;
		color: var(--text-secondary);
	}

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

	.btn-warning {
		background: var(--accent-yellow);
		color: var(--bg-primary);
		border-color: var(--accent-yellow);
	}

	.btn-icon {
		padding: 8px;
		width: 36px;
		height: 36px;
		display: flex;
		align-items: center;
		justify-content: center;
	}
</style>
