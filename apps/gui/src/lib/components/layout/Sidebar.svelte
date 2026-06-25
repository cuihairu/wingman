<script lang="ts">
	import { getCurrentWindow } from '@tauri-apps/api/window';
	import { router } from '$lib/router.svelte';
	import { logs } from '$lib/stores/logs';

	const navItems = [
		{ id: 'dashboard', label: '仪表板', icon: 'dashboard' },
		{ id: 'scripts', label: '脚本管理', icon: 'scripts' },
		{ id: 'screen', label: '屏幕预览', icon: 'screen' },
		{ id: 'logs', label: '日志', icon: 'logs' },
		{ id: 'triggers', label: '触发器', icon: 'triggers' },
		{ id: 'macros', label: '宏录制', icon: 'macros' },
		{ id: 'settings', label: '设置', icon: 'settings' },
	];

	async function minimizeToTray() {
		if (!(window as any).__TAURI_INVOKE__) {
			logs.add('开发模式下无法最小化到托盘', 'info');
			return;
		}
		try {
			await getCurrentWindow().hide();
		} catch (error: any) {
			logs.add(`最小化到托盘失败: ${error}`, 'error');
		}
	}
</script>

<aside class="sidebar">
	<div class="sidebar-header">
		<div class="logo-mark">W</div>
		<div class="brand-text">
			<h1>Wingman</h1>
			<span>Local Control</span>
		</div>
	</div>
	<nav class="nav-items">
		{#each navItems as item}
			<button
				class="nav-item"
				class:active={$router.current === item.id}
				onclick={() => router.navigate(item.id)}
				title={item.label}
				aria-current={$router.current === item.id ? 'page' : undefined}
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
		<button class="nav-item tray-action" onclick={minimizeToTray} title="最小化到托盘">
			<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
				<path d="M5 20h14"></path>
				<path d="M12 4v10"></path>
				<path d="m7 9 5 5 5-5"></path>
			</svg>
			<span>最小化到托盘</span>
		</button>
	</div>
</aside>

<style>
	.sidebar {
		width: 248px;
		flex-shrink: 0;
		background: var(--bg-secondary);
		border-right: 1px solid var(--border-color);
		display: flex;
		flex-direction: column;
		min-height: 100vh;
	}

	.sidebar-header {
		min-height: 64px;
		padding: 14px 16px;
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

	.brand-text {
		min-width: 0;
		display: flex;
		flex-direction: column;
		gap: 2px;
	}

	.brand-text span {
		font-size: 11px;
		color: var(--text-secondary);
		letter-spacing: 0;
	}

	.logo-mark {
		width: 32px;
		height: 32px;
		background: linear-gradient(135deg, var(--accent-green), var(--accent-blue));
		border-radius: 6px;
		display: flex;
		align-items: center;
		justify-content: center;
		color: white;
		font-size: 15px;
		font-weight: 700;
		box-shadow: 0 8px 20px rgba(31, 111, 235, 0.18);
		flex-shrink: 0;
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
		min-height: 40px;
		padding: 9px 12px;
		border-radius: 6px;
		cursor: pointer;
		color: var(--text-secondary);
		font-size: 13px;
		transition: all 0.2s;
		background: transparent;
		width: 100%;
		text-align: left;
		border: 1px solid transparent;
	}

	.nav-item:hover {
		background: var(--bg-tertiary);
		color: var(--text-primary);
	}

	.nav-item.active {
		background: var(--bg-tertiary);
		color: var(--accent-blue);
		border-color: var(--border-color);
		box-shadow: inset 3px 0 0 var(--accent-blue);
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

	.tray-action {
		color: var(--text-secondary);
	}

	@media (max-width: 768px) {
		.sidebar { width: 68px; }
		.sidebar-header {
			justify-content: center;
			padding: 14px 10px;
		}
		.brand-text, .nav-item span { display: none; }
		.nav-item { justify-content: center; }
		.nav-item.active { box-shadow: inset 0 -3px 0 var(--accent-blue); }
	}
</style>
