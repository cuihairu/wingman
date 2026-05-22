<script lang="ts">
	import { connection } from '$lib/stores/connection';
	import { settings } from '$lib/stores/settings';
	import { logs } from '$lib/stores/logs';

	async function handleConnect() {
		try {
			if ($connection.connected) {
				await connection.disconnect();
				logs.add('已断开连接', 'warning');
			} else {
				await connection.connect($settings.wsUrl);
				logs.add('已连接到服务器', 'success');
			}
		} catch (error: any) {
			logs.add('连接失败: ' + error, 'error');
		}
	}
</script>

<div class="page-header">
	<h2 class="page-title">设置</h2>
	<p class="page-subtitle">配置连接和应用程序选项</p>
</div>

<div class="card">
	<div class="card-header">
		<span class="card-title">连接设置</span>
	</div>
	<div class="card-body">
		<div class="form-group">
			<span class="form-label">WebSocket 服务器地址</span>
			<div class="connect-config">
				<input
					type="text"
					id="wsUrlInput"
					class="form-input"
					value={$settings.wsUrl}
					onchange={(e) => settings.update({ wsUrl: (e.target as HTMLInputElement).value })}
				>
				<button class="btn btn-primary" onclick={handleConnect}>
					{$connection.connected ? '断开' : '连接'}
				</button>
			</div>
		</div>
	</div>
</div>

<div class="card">
	<div class="card-header">
		<span class="card-title">应用程序</span>
	</div>
	<div class="card-body">
		<div class="form-group">
			<span class="form-label">启动时最小化到托盘</span>
			<label class="checkbox-label">
				<input
					type="checkbox"
					checked={$settings.minimizeOnStart}
					onchange={() => settings.update({ minimizeOnStart: !$settings.minimizeOnStart })}
				>
				<span>启用</span>
			</label>
		</div>
		<div class="form-group">
			<span class="form-label">自动重连</span>
			<label class="checkbox-label">
				<input
					type="checkbox"
					checked={$settings.autoReconnect}
					onchange={() => settings.update({ autoReconnect: !$settings.autoReconnect })}
				>
				<span>启用</span>
			</label>
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

	.form-group { margin-bottom: 20px; }
	.form-label { display: block; font-size: 13px; color: var(--text-secondary); margin-bottom: 8px; }

	.form-input {
		flex: 1;
		padding: 10px 12px;
		background: var(--bg-primary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		color: var(--text-primary);
		font-size: 14px;
	}

	.form-input:focus { outline: none; border-color: var(--accent-blue); }

	.connect-config { display: flex; gap: 12px; align-items: flex-end; }

	.checkbox-label {
		display: flex;
		align-items: center;
		gap: 8px;
		cursor: pointer;
		font-size: 14px;
	}

	.btn {
		display: inline-flex;
		align-items: center;
		gap: 6px;
		padding: 10px 16px;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		font-size: 14px;
		cursor: pointer;
		transition: all 0.2s;
	}

	.btn:hover { background: var(--border-color); }

	.btn-primary {
		background: var(--accent-green);
		color: white;
		border-color: var(--accent-green);
	}

	.btn-primary:hover { background: #2ea043; }
</style>
