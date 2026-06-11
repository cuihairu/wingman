<script lang="ts">
	import { router } from '$lib/router.svelte';
	import { connection, type SystemStatus } from '$lib/stores/connection';
	import { logs } from '$lib/stores/logs';
	import { activeProfile, profiles } from '$lib/stores/profiles';
	import { scripts } from '$lib/stores/scripts';
	import { triggers } from '$lib/stores/triggers';

	let status = $state<SystemStatus>({
		server: 'wingman',
		version: '-',
		uptime: 0,
		running_scripts: 0,
		paused: false,
	});
	let refreshing = $state(false);
	let lastUpdated = $state('-');

	let runningScripts = $derived($scripts.filter(script => script.is_running).length);
	let stoppedScripts = $derived(Math.max($scripts.length - runningScripts, 0));
	let enabledTriggers = $derived($triggers.filter(trigger => trigger.enabled).length);
	let triggeredCount = $derived($triggers.filter(trigger => trigger.last_triggered).length);
	let recentLogs = $derived($logs.slice(-7).reverse());
	let activeProfileScripts = $derived($activeProfile?.scripts?.length ?? 0);
	let activeProfileTriggers = $derived($activeProfile?.triggers?.length ?? 0);
	let activeProfileHotkeys = $derived($activeProfile
		? [
			`启动 ${$activeProfile.hotkeys.start.join('+')}`,
			`停止 ${$activeProfile.hotkeys.stop.join('+')}`,
			`暂停 ${$activeProfile.hotkeys.pause.join('+')}`,
			`急停 ${$activeProfile.hotkeys.emergencyStop.join('+')}`,
		]
		: []);

	function formatUptime(seconds: number): string {
		const hrs = Math.floor(seconds / 3600);
		const mins = Math.floor((seconds % 3600) / 60);
		const secs = seconds % 60;
		return `${hrs.toString().padStart(2, '0')}:${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
	}

	function formatSize(bytes: number): string {
		if (bytes < 1024) return `${bytes} B`;
		if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
		return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
	}

	function conditionLabel(type: string): string {
		const labels: Record<string, string> = {
			color_found: '颜色找到',
			color_lost: '颜色消失',
			image_found: '图像匹配',
			image_lost: '图像消失',
			time_elapsed: '定时触发',
			hotkey_pressed: '热键按下',
		};
		return labels[type] || type;
	}

	function statusLabel(): string {
		if (!$connection.connected) return '未连接';
		return $connection.paused || status.paused ? '已暂停' : '运行中';
	}

	async function refreshDashboard(showLog = false) {
		if (refreshing) return;
		refreshing = true;
		try {
			const result = await connection.refresh();
			if (result) {
				status = result;
			}
			await Promise.allSettled([
				scripts.load(),
				triggers.load(),
				profiles.load(),
				profiles.loadActive(),
			]);
			lastUpdated = new Date().toLocaleTimeString();
			if (showLog) logs.add('仪表板已刷新', 'info');
		} catch (error: any) {
			logs.add(`刷新仪表板失败: ${error}`, 'error');
		} finally {
			refreshing = false;
		}
	}

	async function startCurrentProfile() {
		if (!$connection.connected) {
			logs.add('未连接到 runtime', 'error');
			return;
		}
		if (!$activeProfile) {
			logs.add('未选择当前配置', 'warning');
			return;
		}
		try {
			const started = await connection.startActiveProfile();
			logs.add(`已启动当前配置脚本: ${started}`, 'success');
			await refreshDashboard();
		} catch (error: any) {
			logs.add(`启动当前配置失败: ${error}`, 'error');
		}
	}

	async function stopCurrentProfile() {
		if (!$connection.connected) {
			logs.add('未连接到 runtime', 'error');
			return;
		}
		try {
			const stopped = await connection.stopActiveProfile();
			logs.add(`已停止当前配置脚本: ${stopped}`, 'info');
			await refreshDashboard();
		} catch (error: any) {
			logs.add(`停止当前配置失败: ${error}`, 'error');
		}
	}

	async function stopEverything() {
		if (!$connection.connected) {
			logs.add('未连接到 runtime', 'error');
			return;
		}
		try {
			const stopped = await connection.stopAll();
			logs.add(`已停止全部脚本: ${stopped}`, 'warning');
			await refreshDashboard();
		} catch (error: any) {
			logs.add(`停止全部失败: ${error}`, 'error');
		}
	}

	async function togglePause() {
		if (!$connection.connected) {
			logs.add('未连接到 runtime', 'error');
			return;
		}
		try {
			await connection.togglePause();
			await refreshDashboard();
			logs.add($connection.paused ? '全部脚本已暂停' : '全部脚本已恢复', 'info');
		} catch (error: any) {
			logs.add(`切换暂停状态失败: ${error}`, 'error');
		}
	}

	$effect(() => {
		if (!$connection.connected) return;
		refreshDashboard();
		const interval = window.setInterval(() => refreshDashboard(), 3000);
		return () => window.clearInterval(interval);
	});
</script>

<div class="dashboard">
	<section class="page-header">
		<div>
			<h2 class="page-title">仪表板</h2>
			<p class="page-subtitle">本地 runtime 控制台 · 最近刷新 {lastUpdated}</p>
		</div>
		<button class="btn" onclick={() => refreshDashboard(true)} disabled={refreshing}>
			<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
				<polyline points="23 4 23 10 17 10"></polyline>
				<polyline points="1 20 1 14 7 14"></polyline>
				<path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"></path>
			</svg>
			{refreshing ? '刷新中' : '刷新'}
		</button>
	</section>

	<section class="status-grid">
		<div class="metric-panel">
			<span class="metric-label">系统状态</span>
			<strong class:green={$connection.connected && !$connection.paused} class:yellow={$connection.paused}>{statusLabel()}</strong>
			<span class="metric-note">{status.version || $connection.version}</span>
		</div>
		<div class="metric-panel">
			<span class="metric-label">运行脚本</span>
			<strong class="green">{status.running_scripts || runningScripts}</strong>
			<span class="metric-note">已停止 {stoppedScripts}</span>
		</div>
		<div class="metric-panel">
			<span class="metric-label">触发器</span>
			<strong class="blue">{enabledTriggers}/{ $triggers.length }</strong>
			<span class="metric-note">最近命中 {triggeredCount}</span>
		</div>
		<div class="metric-panel">
			<span class="metric-label">运行时间</span>
			<strong>{formatUptime(status.uptime)}</strong>
			<span class="metric-note">server {status.server || 'wingman'}</span>
		</div>
	</section>

	<section class="control-panel">
		<div class="profile-summary">
			<div class="summary-heading">
				<span>当前配置</span>
				<button class="link-btn" onclick={() => router.navigate('settings')}>管理</button>
			</div>
			{#if $activeProfile}
				<h3>{$activeProfile.name}</h3>
				<p>{$activeProfile.description || '未填写描述'}</p>
				<div class="profile-tags">
					<span>{activeProfileScripts} 个脚本</span>
					<span>{activeProfileTriggers} 个触发器</span>
					{#each activeProfileHotkeys as hotkey}
						<span>{hotkey}</span>
					{/each}
				</div>
			{:else}
				<h3>未选择配置</h3>
				<p>在设置页创建或激活一个游戏配置。</p>
			{/if}
		</div>

		<div class="quick-actions">
			<div class="summary-heading">
				<span>快速操作</span>
				<span class="muted">Local IPC</span>
			</div>
			<div class="action-grid">
				<button class="btn btn-primary" onclick={startCurrentProfile} disabled={!$connection.connected}>
					<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<polygon points="5 3 19 12 5 21 5 3"></polygon>
					</svg>
					启动当前配置
				</button>
				<button class="btn btn-secondary" onclick={stopCurrentProfile} disabled={!$connection.connected}>
					<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<rect x="6" y="6" width="12" height="12"></rect>
					</svg>
					停止当前配置
				</button>
				<button class="btn btn-warning" onclick={togglePause} disabled={!$connection.connected}>
					<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						{#if $connection.paused}
							<polygon points="5 3 19 12 5 21 5 3"></polygon>
						{:else}
							<rect x="6" y="4" width="4" height="16"></rect>
							<rect x="14" y="4" width="4" height="16"></rect>
						{/if}
					</svg>
					{$connection.paused ? '恢复全部' : '暂停全部'}
				</button>
				<button class="btn btn-danger" onclick={stopEverything} disabled={!$connection.connected}>
					<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<path d="M18 6 6 18"></path>
						<path d="m6 6 12 12"></path>
					</svg>
					停止全部
				</button>
			</div>
		</div>
	</section>

	<section class="detail-grid">
		<div class="detail-panel">
			<div class="summary-heading">
				<span>脚本</span>
				<button class="link-btn" onclick={() => router.navigate('scripts')}>打开</button>
			</div>
			{#if $scripts.length === 0}
				<div class="empty-row">暂无脚本</div>
			{:else}
				<div class="compact-list">
					{#each $scripts.slice(0, 6) as script}
						<div class="compact-row">
							<div>
								<strong>{script.name}</strong>
								<span>{script.path} · {formatSize(script.size)}</span>
							</div>
							<span class="pill" class:green-pill={script.is_running}>{script.is_running ? '运行中' : '已停止'}</span>
						</div>
					{/each}
				</div>
			{/if}
		</div>

		<div class="detail-panel">
			<div class="summary-heading">
				<span>触发器</span>
				<button class="link-btn" onclick={() => router.navigate('triggers')}>打开</button>
			</div>
			{#if $triggers.length === 0}
				<div class="empty-row">暂无触发器</div>
			{:else}
				<div class="compact-list">
					{#each $triggers.slice(0, 6) as trigger}
						<div class="compact-row">
							<div>
								<strong>{trigger.name}</strong>
								<span>{conditionLabel(trigger.condition.type)} · {trigger.actions.length} 个动作</span>
							</div>
							<span class="pill" class:green-pill={trigger.enabled}>{trigger.enabled ? '启用' : '停用'}</span>
						</div>
					{/each}
				</div>
			{/if}
		</div>

		<div class="detail-panel log-panel">
			<div class="summary-heading">
				<span>最近日志</span>
				<button class="link-btn" onclick={() => router.navigate('logs')}>打开</button>
			</div>
			{#if recentLogs.length === 0}
				<div class="empty-row">暂无日志</div>
			{:else}
				<div class="compact-list">
					{#each recentLogs as log}
						<div class="log-row">
							<span>{log.time}</span>
							<strong class="log-{log.type}">{log.message}</strong>
						</div>
					{/each}
				</div>
			{/if}
		</div>
	</section>
</div>

<style>
	.dashboard {
		display: flex;
		flex-direction: column;
		gap: 20px;
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

	.page-subtitle,
	.muted {
		font-size: 13px;
		color: var(--text-secondary);
	}

	.status-grid,
	.detail-grid {
		display: grid;
		grid-template-columns: repeat(4, minmax(0, 1fr));
		gap: 16px;
	}

	.metric-panel,
	.profile-summary,
	.quick-actions,
	.detail-panel {
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 8px;
		padding: 16px;
	}

	.metric-panel {
		display: flex;
		flex-direction: column;
		gap: 8px;
		min-height: 112px;
	}

	.metric-label {
		font-size: 12px;
		color: var(--text-secondary);
	}

	.metric-panel strong {
		font-size: 28px;
		line-height: 1;
		color: var(--text-primary);
	}

	.metric-panel .metric-note {
		margin-top: auto;
		font-size: 12px;
		color: var(--text-secondary);
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.green { color: var(--accent-green) !important; }
	.blue { color: var(--accent-blue) !important; }
	.yellow { color: var(--accent-yellow) !important; }

	.control-panel {
		display: grid;
		grid-template-columns: minmax(0, 1.2fr) minmax(340px, 0.8fr);
		gap: 16px;
	}

	.summary-heading {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 12px;
		margin-bottom: 14px;
		font-size: 14px;
		font-weight: 600;
		color: var(--text-primary);
	}

	.profile-summary h3 {
		font-size: 18px;
		margin-bottom: 6px;
	}

	.profile-summary p {
		min-height: 20px;
		color: var(--text-secondary);
		font-size: 13px;
		margin-bottom: 14px;
	}

	.profile-tags {
		display: flex;
		flex-wrap: wrap;
		gap: 8px;
	}

	.profile-tags span,
	.pill {
		display: inline-flex;
		align-items: center;
		justify-content: center;
		min-height: 24px;
		padding: 3px 8px;
		border-radius: 6px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		color: var(--text-secondary);
		font-size: 12px;
		white-space: nowrap;
	}

	.green-pill {
		color: var(--accent-green);
		border-color: rgba(63, 185, 80, 0.45);
		background: rgba(63, 185, 80, 0.08);
	}

	.action-grid {
		display: grid;
		grid-template-columns: repeat(2, minmax(0, 1fr));
		gap: 10px;
	}

	.detail-grid {
		grid-template-columns: 1fr 1fr 1fr;
		align-items: start;
	}

	.compact-list {
		display: flex;
		flex-direction: column;
		gap: 8px;
	}

	.compact-row,
	.log-row {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 12px;
		min-height: 42px;
		padding: 10px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
	}

	.compact-row div {
		min-width: 0;
	}

	.compact-row strong,
	.log-row strong {
		display: block;
		font-size: 13px;
		color: var(--text-primary);
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.compact-row span:not(.pill),
	.log-row span {
		display: block;
		margin-top: 3px;
		font-size: 11px;
		color: var(--text-secondary);
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.log-panel {
		max-height: 360px;
		overflow: hidden;
	}

	.log-row {
		align-items: flex-start;
	}

	.log-row strong {
		flex: 1;
		text-align: right;
		white-space: normal;
	}

	.log-info { color: var(--accent-blue) !important; }
	.log-success { color: var(--accent-green) !important; }
	.log-warning { color: var(--accent-yellow) !important; }
	.log-error { color: var(--accent-red) !important; }

	.empty-row {
		display: grid;
		place-items: center;
		min-height: 120px;
		color: var(--text-secondary);
		font-size: 13px;
		background: var(--bg-tertiary);
		border: 1px dashed var(--border-color);
		border-radius: 6px;
	}

	.btn,
	.link-btn {
		display: inline-flex;
		align-items: center;
		justify-content: center;
		gap: 6px;
		min-height: 36px;
		padding: 8px 14px;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		font-size: 13px;
		cursor: pointer;
		transition: all 0.2s;
	}

	.btn:hover,
	.link-btn:hover {
		background: var(--border-color);
	}

	.btn:disabled {
		cursor: not-allowed;
		opacity: 0.45;
	}

	.link-btn {
		min-height: 28px;
		padding: 4px 8px;
		color: var(--accent-blue);
		background: transparent;
	}

	.btn-primary {
		background: var(--accent-green);
		color: white;
		border-color: var(--accent-green);
	}

	.btn-secondary {
		background: var(--accent-blue);
		color: white;
		border-color: var(--accent-blue);
	}

	.btn-warning {
		background: var(--accent-yellow);
		color: var(--bg-primary);
		border-color: var(--accent-yellow);
	}

	.btn-danger {
		background: var(--accent-red);
		color: white;
		border-color: var(--accent-red);
	}

	@media (max-width: 1100px) {
		.status-grid {
			grid-template-columns: repeat(2, minmax(0, 1fr));
		}

		.control-panel,
		.detail-grid {
			grid-template-columns: 1fr;
		}
	}

	@media (max-width: 720px) {
		.page-header,
		.compact-row,
		.log-row {
			align-items: stretch;
			flex-direction: column;
		}

		.status-grid,
		.action-grid {
			grid-template-columns: 1fr;
		}

		.log-row strong {
			text-align: left;
		}
	}
</style>
