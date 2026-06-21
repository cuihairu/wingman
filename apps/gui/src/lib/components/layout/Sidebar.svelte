<script lang="ts">
	import { router } from '$lib/router.svelte';

	const navItems = [
		{ id: 'dashboard', label: '仪表板', icon: 'dashboard' },
		{ id: 'scripts', label: '脚本管理', icon: 'scripts' },
		{ id: 'screen', label: '屏幕预览', icon: 'screen' },
		{ id: 'logs', label: '日志', icon: 'logs' },
		{ id: 'triggers', label: '触发器', icon: 'triggers' },
		{ id: 'macros', label: '宏录制', icon: 'macros' },
		{ id: 'settings', label: '设置', icon: 'settings' },
	];
</script>

<aside class="sidebar">
	<div class="sidebar-header">
		<div class="logo">🎮</div>
		<h1>Wingman</h1>
	</div>
	<nav class="nav-items">
		{#each navItems as item}
			<button
				class="nav-item"
				class:active={$router.current === item.id}
				onclick={() => router.navigate(item.id)}
			>
				{#if item.icon === 'dashboard'}
					<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<rect x="3" y="3" width="7" height="7"></rect>
						<rect x="14" y="3" width="7" height="7"></rect>
						<rect x="14" y="14" width="7" height="7"></rect>
						<rect x="3" y="14" width="7" height="7"></rect>
					</svg>
				{:else if item.icon === 'scripts'}
					<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path>
						<polyline points="14 2 14 8 20 8"></polyline>
						<line x1="16" y1="13" x2="8" y2="13"></line>
						<line x1="16" y1="17" x2="8" y2="17"></line>
					</svg>
				{:else if item.icon === 'screen'}
					<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<rect x="3" y="4" width="18" height="12" rx="2"></rect>
						<line x1="8" y1="20" x2="16" y2="20"></line>
						<line x1="12" y1="16" x2="12" y2="20"></line>
						<path d="M8 9h8"></path>
						<path d="M8 12h5"></path>
					</svg>
				{:else if item.icon === 'logs'}
					<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path>
						<polyline points="14 2 14 8 20 8"></polyline>
						<line x1="16" y1="13" x2="8" y2="13"></line>
						<line x1="16" y1="17" x2="8" y2="17"></line>
					</svg>
			{:else if item.icon === 'triggers'}
				<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"></polygon>
				</svg>
			{:else if item.icon === 'macros'}
				<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<circle cx="12" cy="12" r="3"></circle>
					<circle cx="12" cy="12" r="9"></circle>
					<circle cx="12" cy="6" r="1"></circle>
					<circle cx="18" cy="12" r="1"></circle>
					<circle cx="12" cy="18" r="1"></circle>
					<circle cx="6" cy="12" r="1"></circle>
				</svg>
				{:else if item.icon === 'settings'}
					<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<circle cx="12" cy="12" r="3"></circle>
						<path d="M12 1v6m0 6v6"></path>
						<path d="m4.93 4.93 4.24 4.24m5.66 5.66 4.24 4.24"></path>
						<path d="M1 12h6m6 0h6"></path>
						<path d="m4.93 19.07 4.24-4.24m5.66-5.66 4.24-4.24"></path>
					</svg>
				{/if}
				<span>{item.label}</span>
			</button>
		{/each}
	</nav>
	<div class="sidebar-footer">
		<button class="nav-item">
			<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
				<line x1="5" y1="12" x2="19" y2="12"></line>
			</svg>
			<span>最小化到托盘</span>
		</button>
	</div>
</aside>

<style>
	.sidebar {
		width: 240px;
		background: var(--bg-secondary);
		border-right: 1px solid var(--border-color);
		display: flex;
		flex-direction: column;
	}

	.sidebar-header {
		padding: 16px;
		border-bottom: 1px solid var(--border-color);
		display: flex;
		align-items: center;
		gap: 10px;
	}

	.sidebar-header h1 {
		font-size: 16px;
		font-weight: 600;
		color: var(--text-primary);
	}

	.logo {
		width: 28px;
		height: 28px;
		background: linear-gradient(135deg, var(--accent-green), var(--accent-blue));
		border-radius: 6px;
		display: flex;
		align-items: center;
		justify-content: center;
		font-size: 14px;
	}

	.nav-items {
		flex: 1;
		padding: 8px;
		display: flex;
		flex-direction: column;
		gap: 2px;
	}

	.nav-item {
		display: flex;
		align-items: center;
		gap: 10px;
		padding: 10px 12px;
		border-radius: 6px;
		cursor: pointer;
		color: var(--text-secondary);
		font-size: 14px;
		transition: all 0.2s;
		background: transparent;
		width: 100%;
		text-align: left;
	}

	.nav-item:hover {
		background: var(--bg-tertiary);
		color: var(--text-primary);
	}

	.nav-item.active {
		background: var(--bg-tertiary);
		color: var(--accent-blue);
	}

	.icon {
		width: 18px;
		height: 18px;
		opacity: 0.8;
		flex-shrink: 0;
	}

	.sidebar-footer {
		padding: 12px;
		border-top: 1px solid var(--border-color);
	}

	@media (max-width: 768px) {
		.sidebar { width: 60px; }
		.sidebar-header h1, .nav-item span { display: none; }
		.nav-item { justify-content: center; }
	}
</style>
