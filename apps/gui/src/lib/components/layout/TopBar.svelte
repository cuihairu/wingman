<script lang="ts">
	import { connection } from '$lib/stores/connection';
	import { profiles, activeProfile } from '$lib/stores/profiles';
	import { router } from '$lib/router.svelte';
	import { logs } from '$lib/stores/logs';
	import { theme, toggleTheme } from '$lib/stores/theme';

	let profileOpen = $state(false);

	async function handleRefresh() {
		if ($connection.connected) {
			await connection.refresh();
		}
	}

	function selectProfile(id: string) {
		profiles.setActive(id);
		profileOpen = false;
	}

	function goToSettings() {
		router.navigate('settings');
		profileOpen = false;
	}

	async function handleEmergencyStop() {
		try {
			const stopped = await connection.stopAll();
			logs.add(`已急停脚本: ${stopped}`, 'warning');
		} catch (error: any) {
			logs.add(`急停失败: ${error}`, 'error');
		}
	}

	async function handleStartProfile() {
		try {
			const started = await connection.startActiveProfile();
			logs.add(`已启动当前配置脚本: ${started}`, 'success');
		} catch (error: any) {
			logs.add(`启动当前配置失败: ${error}`, 'error');
		}
	}

	async function handleStopProfile() {
		try {
			const stopped = await connection.stopActiveProfile();
			logs.add(`已停止当前配置脚本: ${stopped}`, 'info');
		} catch (error: any) {
			logs.add(`停止当前配置失败: ${error}`, 'error');
		}
	}
</script>

<header class="top-bar">
	<div class="top-bar-left">
		<div class="connection-status">
			<div class="status-dot" class:connected={$connection.connected} class:error={!$connection.connected}></div>
			<span>{$connection.connected ? '已连接' : '未连接'}</span>
		</div>
		<span class="version-info" title={$connection.version}>{$connection.version}</span>
		{#if $connection.remote}
			<span class="remote-status remote-{$connection.remote.state}" title={$connection.remote.message}>
				{$connection.remote.state === 'connected' ? '远程在线' : $connection.remote.state === 'reconnecting' ? '远程重连' : $connection.remote.state === 'connecting' ? '远程连接中' : $connection.remote.state === 'error' ? '远程异常' : '远程离线'}
			</span>
		{/if}
		<div class="profile-selector">
			<button class="profile-btn" onclick={() => profileOpen = !profileOpen}>
				<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"></path>
					<circle cx="12" cy="7" r="4"></circle>
				</svg>
				<span>{$activeProfile?.name || '选择配置'}</span>
				<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<polyline points="6 9 12 15 18 9"></polyline>
				</svg>
			</button>
			{#if profileOpen}
				<div class="profile-dropdown">
					{#if $profiles.length === 0}
						<div class="profile-empty">暂无配置</div>
					{:else}
						{#each $profiles as profile}
							<button
								class="profile-item"
								class:active={profile.id === $activeProfile?.id}
								onclick={() => selectProfile(profile.id)}
							>
								<span class="profile-name">{profile.name}</span>
								<span class="profile-desc">{profile.description || profile.id}</span>
							</button>
						{/each}
						<div class="profile-divider"></div>
					{/if}
					<button class="profile-item profile-manage" onclick={goToSettings}>
						<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
							<circle cx="12" cy="12" r="3"></circle>
							<path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"></path>
						</svg>
						管理配置...
					</button>
				</div>
			{/if}
		</div>
	</div>
	<div class="top-bar-right">
		{#if $connection.connected}
			<button class="btn btn-success top-action profile-control" onclick={handleStartProfile} title="启动当前配置">
				<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<polygon points="5 3 19 12 5 21 5 3"></polygon>
				</svg>
				<span class="action-text">启动配置</span>
			</button>
			<button class="btn btn-secondary top-action profile-control" onclick={handleStopProfile} title="停止当前配置">
				<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<rect x="6" y="6" width="12" height="12"></rect>
				</svg>
				<span class="action-text">停止配置</span>
			</button>
			<button class="btn btn-danger top-action emergency-control" onclick={handleEmergencyStop} title="急停">
				<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<rect x="6" y="6" width="12" height="12"></rect>
				</svg>
				<span class="action-text">急停</span>
			</button>
			<button class="btn btn-warning top-action pause-control" onclick={() => connection.togglePause()} title={$connection.paused ? '恢复运行' : '暂停全部'}>
				<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					{#if $connection.paused}
						<polygon points="5 3 19 12 5 21 5 3"></polygon>
					{:else}
						<rect x="6" y="4" width="4" height="16"></rect>
						<rect x="14" y="4" width="4" height="16"></rect>
					{/if}
				</svg>
				<span class="action-text">{$connection.paused ? '恢复运行' : '暂停全部'}</span>
			</button>
		{/if}
		<button class="btn btn-icon" title={$theme === 'dark' ? '切换到亮色' : '切换到暗色'} onclick={toggleTheme}>
			{#if $theme === 'dark'}
				<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<circle cx="12" cy="12" r="5"></circle>
					<line x1="12" y1="1" x2="12" y2="3"></line>
					<line x1="12" y1="21" x2="12" y2="23"></line>
					<line x1="4.22" y1="4.22" x2="5.64" y2="5.64"></line>
					<line x1="18.36" y1="18.36" x2="19.78" y2="19.78"></line>
					<line x1="1" y1="12" x2="3" y2="12"></line>
					<line x1="21" y1="12" x2="23" y2="12"></line>
					<line x1="4.22" y1="19.78" x2="5.64" y2="18.36"></line>
					<line x1="18.36" y1="5.64" x2="19.78" y2="4.22"></line>
				</svg>
			{:else}
				<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"></path>
				</svg>
			{/if}
		</button>
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
		min-height: 60px;
		background: var(--bg-secondary);
		border-bottom: 1px solid var(--border-color);
		display: flex;
		align-items: center;
		justify-content: space-between;
		padding: 0 20px;
		gap: 16px;
		flex-shrink: 0;
	}

	.top-bar-left, .top-bar-right {
		display: flex;
		align-items: center;
		gap: 8px;
		min-width: 0;
	}

	.top-bar-left {
		flex: 1 1 auto;
	}

	.top-bar-right {
		flex: 0 0 auto;
	}

	.connection-status {
		display: flex;
		align-items: center;
		gap: 8px;
		font-size: 13px;
		min-height: 32px;
		padding: 6px 10px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 999px;
		white-space: nowrap;
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
		min-width: 0;
		max-width: 220px;
		font-size: 13px;
		color: var(--text-secondary);
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.remote-status {
		display: inline-flex;
		align-items: center;
		min-height: 28px;
		padding: 4px 8px;
		border: 1px solid var(--border-color);
		border-radius: 999px;
		background: var(--bg-tertiary);
		color: var(--text-secondary);
		font-size: 12px;
		white-space: nowrap;
	}

	.remote-connected {
		color: var(--accent-green);
		border-color: rgba(63, 185, 80, 0.45);
		background: rgba(63, 185, 80, 0.08);
	}

	.remote-connecting,
	.remote-reconnecting {
		color: var(--accent-yellow);
		border-color: rgba(210, 153, 34, 0.42);
		background: rgba(210, 153, 34, 0.08);
	}

	.remote-error {
		color: var(--accent-red);
		border-color: rgba(248, 81, 73, 0.42);
		background: rgba(248, 81, 73, 0.08);
	}

	.btn {
		display: inline-flex;
		align-items: center;
		justify-content: center;
		gap: 6px;
		min-height: 34px;
		padding: 7px 10px;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		font-size: 13px;
		cursor: pointer;
		transition: all 0.2s;
		white-space: nowrap;
	}

	.btn:hover { background: var(--surface-hover); }

	.btn-warning {
		background: var(--accent-yellow);
		color: var(--bg-primary);
		border-color: var(--accent-yellow);
	}

	.btn-success {
		background: var(--accent-green);
		color: white;
		border-color: var(--accent-green);
	}

	.btn-success:hover {
		background: var(--accent-green-hover);
	}

	.btn-secondary {
		background: var(--accent-blue);
		color: white;
		border-color: var(--accent-blue);
	}

	.btn-secondary:hover {
		background: var(--accent-blue-hover);
	}

	.btn-danger {
		background: var(--accent-red);
		color: white;
		border-color: var(--accent-red);
	}

	.btn-danger:hover {
		background: var(--accent-red-hover);
	}

	.btn-icon {
		padding: 8px;
		width: 34px;
		height: 34px;
		display: flex;
		align-items: center;
		justify-content: center;
	}

	.profile-selector {
		position: relative;
	}

	.profile-btn {
		display: flex;
		align-items: center;
		gap: 6px;
		min-height: 32px;
		max-width: 220px;
		padding: 6px 10px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		color: var(--text-primary);
		font-size: 13px;
		cursor: pointer;
		transition: all 0.2s;
	}

	.profile-btn span {
		min-width: 0;
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.profile-btn:hover {
		background: var(--surface-hover);
	}

	.profile-dropdown {
		position: absolute;
		top: calc(100% + 4px);
		left: 0;
		width: min(320px, calc(100vw - 32px));
		min-width: 220px;
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 8px;
		box-shadow: 0 12px 30px var(--shadow-color);
		z-index: 100;
		padding: 4px;
	}

	.profile-item {
		display: flex;
		flex-direction: column;
		gap: 2px;
		width: 100%;
		padding: 8px 12px;
		background: transparent;
		border: none;
		border-radius: 4px;
		cursor: pointer;
		text-align: left;
		color: var(--text-primary);
		transition: background 0.15s;
	}

	.profile-item:hover {
		background: var(--bg-tertiary);
	}

	.profile-item.active {
		background: var(--bg-tertiary);
		border-left: 2px solid var(--accent-blue);
	}

	.profile-name {
		font-size: 13px;
		font-weight: 500;
	}

	.profile-desc {
		font-size: 11px;
		color: var(--text-secondary);
	}

	.profile-divider {
		height: 1px;
		background: var(--border-color);
		margin: 4px 0;
	}

	.profile-manage {
		color: var(--text-secondary);
		flex-direction: row;
		align-items: center;
		gap: 6px;
		font-size: 12px;
	}

	.profile-manage:hover {
		color: var(--text-primary);
	}

	.profile-empty {
		padding: 10px 12px;
		color: var(--text-secondary);
		font-size: 12px;
	}

	@media (max-width: 1240px) {
		.action-text {
			display: none;
		}

		.top-action {
			width: 34px;
			padding: 7px;
		}
	}

	@media (max-width: 980px) {
		.version-info,
		.remote-status {
			display: none;
		}
	}

	@media (max-width: 720px) {
		.top-bar {
			padding: 0 12px;
		}

		.top-bar-left {
			gap: 6px;
		}

		.connection-status span {
			display: none;
		}

		.connection-status {
			padding: 6px 8px;
		}

		.profile-btn {
			max-width: 140px;
		}
	}

	@media (max-width: 620px) {
		.profile-control,
		.emergency-control,
		.pause-control {
			display: none;
		}
	}

	@media (max-width: 420px) {
		.top-bar {
			gap: 8px;
			padding: 0 8px;
		}

		.profile-btn {
			max-width: 122px;
		}

		.btn-icon {
			width: 32px;
			height: 32px;
			padding: 7px;
		}
	}
</style>
