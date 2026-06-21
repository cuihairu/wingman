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
		<span class="version-info">{$connection.version}</span>
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
			<button class="btn btn-success" onclick={handleStartProfile}>
				<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<polygon points="5 3 19 12 5 21 5 3"></polygon>
				</svg>
				启动当前配置
			</button>
			<button class="btn btn-secondary" onclick={handleStopProfile}>
				<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<rect x="6" y="6" width="12" height="12"></rect>
				</svg>
				停止当前配置
			</button>
			<button class="btn btn-danger" onclick={handleEmergencyStop}>
				<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<rect x="6" y="6" width="12" height="12"></rect>
				</svg>
				急停
			</button>
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

	.btn-success {
		background: var(--accent-green);
		color: white;
		border-color: var(--accent-green);
	}

	.btn-success:hover {
		background: #2ea043;
	}

	.btn-secondary {
		background: var(--accent-blue);
		color: white;
		border-color: var(--accent-blue);
	}

	.btn-secondary:hover {
		background: #1f6feb;
	}

	.btn-danger {
		background: var(--accent-red);
		color: white;
		border-color: var(--accent-red);
	}

	.btn-danger:hover {
		background: #da3633;
	}

	.btn-icon {
		padding: 8px;
		width: 36px;
		height: 36px;
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
		padding: 6px 12px;
		background: var(--bg-tertiary);
		border: 1px solid var(--border-color);
		border-radius: 6px;
		color: var(--text-primary);
		font-size: 13px;
		cursor: pointer;
		transition: all 0.2s;
	}

	.profile-btn:hover {
		background: var(--border-color);
	}

	.profile-dropdown {
		position: absolute;
		top: calc(100% + 4px);
		left: 0;
		min-width: 200px;
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 8px;
		box-shadow: 0 8px 24px rgba(0, 0, 0, 0.3);
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
</style>
