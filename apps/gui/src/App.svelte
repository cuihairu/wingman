<script lang="ts">
	import Sidebar from '$lib/components/layout/Sidebar.svelte';
	import TopBar from '$lib/components/layout/TopBar.svelte';
	import { router } from '$lib/router.svelte';
	import { connection } from '$lib/stores/connection';
	import { logs } from '$lib/stores/logs';

	import DashboardPage from './routes/dashboard/+page.svelte';
	import ScriptsPage from './routes/scripts/+page.svelte';
	import LogsPage from './routes/logs/+page.svelte';
	import SettingsPage from './routes/settings/+page.svelte';
	import TriggersPage from './routes/triggers/+page.svelte';
	import EditorPage from './routes/editor/+page.svelte';

	// 初始化路由
	router.init();

	// 开发模式模拟数据
	const invoke = (window as any).__TAURI_INVOKE__;

	$effect(() => {
		if (!invoke) {
			connection.setConnected(true);
			connection.setVersion('wingman 0.1.0 (dev)');
			logs.add('开发模式已启动', 'info');
		}
	});
</script>

<div class="app-container">
	<Sidebar />

	<main class="main-content">
		{#if $connection.paused}
			<div class="pause-banner">
				<span>⏸️ 所有功能已暂停</span>
				<button class="btn btn-primary" onclick={() => connection.togglePause()}>恢复运行</button>
			</div>
		{/if}

		<TopBar />

		<div class="content-area">
			{#if $router.current === 'dashboard'}
				<DashboardPage />
			{:else if $router.current === 'scripts'}
				<ScriptsPage />
			{:else if $router.current === 'logs'}
				<LogsPage />
			{:else if $router.current === 'settings'}
				<SettingsPage />
			{:else if $router.current === 'triggers'}
				<TriggersPage />
			{:else if $router.current === 'editor'}
				<EditorPage />
			{/if}
		</div>
	</main>
</div>

<style>
	.app-container {
		display: flex;
		height: 100vh;
	}

	.main-content {
		flex: 1;
		display: flex;
		flex-direction: column;
		overflow: hidden;
	}

	.pause-banner {
		background: var(--accent-yellow);
		color: var(--bg-primary);
		padding: 12px 20px;
		display: flex;
		align-items: center;
		justify-content: space-between;
	}

	.content-area {
		flex: 1;
		overflow-y: auto;
		padding: 24px;
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

	.btn-primary {
		background: var(--accent-green);
		color: white;
		border-color: var(--accent-green);
	}

	.btn-primary:hover {
		background: #2ea043;
	}
</style>
