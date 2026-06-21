<script lang="ts">
	import Sidebar from '$lib/components/layout/Sidebar.svelte';
	import TopBar from '$lib/components/layout/TopBar.svelte';
	import { router } from '$lib/router.svelte';
	import { connection } from '$lib/stores/connection';
	import { settings } from '$lib/stores/settings';
	import { scripts } from '$lib/stores/scripts';
	import { triggers } from '$lib/stores/triggers';
	import { profiles, activeProfile } from '$lib/stores/profiles';
	import { logs } from '$lib/stores/logs';
	import { screen } from '$lib/stores/screen';
	import { createEventPoller } from '$lib/stores/events';

	import DashboardPage from './routes/dashboard/+page.svelte';
	import ScriptsPage from './routes/scripts/+page.svelte';
	import ScreenPage from './routes/screen/+page.svelte';
	import LogsPage from './routes/logs/+page.svelte';
	import SettingsPage from './routes/settings/+page.svelte';
	import TriggersPage from './routes/triggers/+page.svelte';

	router.init();

	const invoke = (window as any).__TAURI_INVOKE__;
	let reconnecting = false;

	// runtime → GUI 事件轮询（日志/触发器/截图）
	const eventPoller = createEventPoller();

	async function loadAllData() {
		await Promise.allSettled([
			scripts.load(),
			triggers.load(),
			profiles.load(),
			profiles.loadActive(),
		]);
	}

	async function autoStartProfileScripts() {
		if (!$settings.autoStart || !$activeProfile) return;
		const startupScripts = $activeProfile.scripts.filter(script => script.autoStart);
		for (const script of startupScripts) {
			try {
				await scripts.start(script.name, script.path);
				logs.add(`已自动启动脚本: ${script.name}`, 'success');
			} catch (error: any) {
				logs.add(`自动启动脚本失败 ${script.name}: ${error}`, 'error');
			}
		}
	}

	async function reconnectOnce() {
		if (reconnecting || !invoke || !$settings.autoReconnect) return;
		reconnecting = true;
		try {
			await connection.connect($settings.ipcEndpoint);
			await loadAllData();
			await autoStartProfileScripts();
			logs.add('已自动重连到本地 runtime IPC', 'success');
		} catch {
			// 等待下一轮心跳。
		} finally {
			reconnecting = false;
		}
	}

	$effect(() => {
		if (!invoke) {
			// 开发模式：加载模拟数据
			connection.setConnected(true);
			connection.setVersion('wingman 0.1.0 (dev)');
			scripts.loadDevData();
			triggers.loadDevData();
			profiles.loadDevData();
			screen.loadDevData();
			logs.add('开发模式已启动', 'info');
		} else {
			// 生产模式：自动连接
			(async () => {
				try {
					await connection.connect($settings.ipcEndpoint);
					logs.add('已自动连接到本地 runtime IPC', 'success');
					await loadAllData();
					await autoStartProfileScripts();
				} catch {
					logs.add('自动连接失败，请在设置中手动连接', 'warning');
				}
			})();
		}
	});

	$effect(() => {
		if (!invoke) return;
		const interval = window.setInterval(async () => {
			if ($connection.connected) {
				const status = await connection.refresh();
				if (!status) {
					logs.add('runtime IPC 连接已断开', 'warning');
				}
				return;
			}
			await reconnectOnce();
		}, 5000);
		return () => window.clearInterval(interval);
	});

	// 仅在已连接时轮询 runtime 事件；断开时停止以避免无效请求
	$effect(() => {
		if (!invoke) return;
		if ($connection.connected) {
			eventPoller.start();
			return () => eventPoller.stop();
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
			{:else if $router.current === 'screen'}
				<ScreenPage />
			{:else if $router.current === 'logs'}
				<LogsPage />
			{:else if $router.current === 'settings'}
				<SettingsPage />
			{:else if $router.current === 'triggers'}
				<TriggersPage />
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
