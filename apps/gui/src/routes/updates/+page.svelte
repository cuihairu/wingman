<script lang="ts">
	import { onMount } from 'svelte';
	import { connection } from '$lib/stores/connection';
	import { updates } from '$lib/stores/updates';

	const t = $derived($updates.lastCheckMs ? ($updates.lastCheckMs / 1000) : 0);

	let pillText = $derived.by(() => {
		if (!$connection.connected) return 'runtime IPC 未连接';
		if (!$updates.enabled) return '更新检测未启用';
		if ($updates.downloading) return '正在下载更新';
		if ($updates.downloaded) return '更新已就绪';
		if ($updates.available) return '发现新版本';
		return '已是最新版本';
	});

	let summary = $derived([
		{ label: '当前版本', value: $updates.localVersion || '—', hint: 'local' },
		{ label: '最新版本', value: $updates.candidate.version || '—', hint: 'candidate' },
		{ label: '最近检查', value: t ? timeAgo(t) : '尚未检查', hint: 'last check' },
		{ label: '状态', value: pillText, hint: 'session' },
	]);

	function timeAgo(ts: number): string {
		const diff = Math.max(0, Math.floor(Date.now() / 1000 - ts));
		if (diff < 5) return '刚刚';
		if (diff < 60) return `${diff} 秒前`;
		if (diff < 3600) return `${Math.floor(diff / 60)} 分钟前`;
		return `${Math.floor(diff / 3600)} 小时前`;
	}

	onMount(() => {
		updates.refresh();
	});

	async function handleCheck() {
		await updates.check();
	}

	async function handleDownload() {
		await updates.download();
	}
</script>

<div class="updates-page">
	<section class="page-header">
		<div>
			<h2 class="page-title">更新</h2>
			<p class="page-subtitle">检查 runtime 新版本，下载并校验升级制品</p>
		</div>
		<div class="header-state">
			<div class="status-pill" class:available={$updates.available} class:offline={!$connection.connected}>
				<span class="status-dot"></span>
				{pillText}
			</div>
		</div>
	</section>

	<section class="summary-grid">
		{#each summary as item}
			<div class="metric-card">
				<span>{item.label}</span>
				<strong class:sm={item.label === '状态'}>{item.value}</strong>
				<small>{item.hint}</small>
			</div>
		{/each}
	</section>

	{#if !$connection.connected}
		<div class="notice-card">
			<strong>更新功能当前不可用</strong>
			<span>该页依赖 runtime IPC。连接建立后才能检查并下载更新。</span>
		</div>
	{:else if !$updates.enabled}
		<div class="notice-card">
			<strong>更新检测未启用</strong>
			<span>runtime 的 [update] 段未配置或已关闭。请确认 enabled 与 base_url 均已设置。</span>
		</div>
	{:else}
		<section class="status-card card">
			<div class="panel-heading">
				<div>
					<span class="eyebrow">Auto Update</span>
					<h3>更新状态</h3>
				</div>
				<span class="inline-count">{$updates.candidate.version || '—'}</span>
			</div>
			<div class="release-banner" class:active={$updates.available || $updates.downloaded}>
				<div class="banner-dot"></div>
				<div>
					{#if $updates.downloaded}
						<strong>更新已下载完成</strong>
						<span>{$updates.downloadPath}</span>
					{:else if $updates.available}
						<strong>发现新版本 {$updates.candidate.version}</strong>
						<span>{$updates.candidate.fileName || $updates.candidate.url || '待下载'}</span>
					{:else}
						<strong>已是最新版本</strong>
						<span>当前运行 {$updates.localVersion || '（未知）'}，点击「检查更新」确认服务端是否存在新版本。</span>
					{/if}
				</div>
			</div>
			{#if $updates.lastError}
				<div class="error-line">{$updates.lastError}</div>
			{/if}
			<div class="action-row">
				<button class="btn" onclick={handleCheck}>
					<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<path d="M21 3v6h-6"></path>
						<path d="M3 12a9 9 0 0 1 15-6.7L21 8"></path>
						<path d="M3 21v-6h6"></path>
						<path d="M21 12a9 9 0 0 1-15 6.7L3 16"></path>
					</svg>
					检查更新
				</button>
				{#if $updates.available || $updates.downloading || $updates.downloaded}
					<button class="btn btn-primary" onclick={handleDownload} disabled={$updates.downloading}>
						<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
							<path d="M12 5v14"></path>
							<polyline points="19 12 12 19 5 12"></polyline>
						</svg>
						{$updates.downloading ? '下载中…' : $updates.downloaded ? '重新下载' : '下载更新'}
					</button>
				{/if}
			</div>
		</section>

		<section class="hint-grid">
			<div class="hint-card">
				<strong>下载与校验</strong>
				<span>制品下载到本地目录后先做 SHA-256 校验，校验通过才落盘。全程不自动替换运行中的进程。</span>
			</div>
			<div class="hint-card">
				<strong>安装说明</strong>
				<span>下载完成后将手动执行安装。静默自替换需按各平台原生机制在真机桌面会话中单独验收。</span>
			</div>
		</section>
	{/if}
</div>

<style>
	.updates-page {
		display: flex;
		flex-direction: column;
		gap: 18px;
	}

	.page-header {
		display: flex;
		align-items: flex-start;
		justify-content: space-between;
		gap: 16px;
	}

	.page-title {
		margin-bottom: 8px;
		color: var(--text-primary);
		font-size: 24px;
		font-weight: 600;
	}

	.page-subtitle {
		color: var(--text-secondary);
		font-size: 13px;
	}

	.status-pill {
		display: inline-flex;
		align-items: center;
		gap: 8px;
		min-height: 34px;
		padding: 7px 11px;
		border: 1px solid rgba(88, 166, 255, 0.32);
		border-radius: 999px;
		background: rgba(88, 166, 255, 0.08);
		color: var(--accent-blue);
		font-size: 12px;
		white-space: nowrap;
	}

	.status-pill.available {
		border-color: rgba(63, 185, 80, 0.36);
		background: rgba(63, 185, 80, 0.1);
		color: var(--accent-green);
	}

	.status-pill.offline {
		border-color: var(--border-color);
		background: var(--bg-secondary);
		color: var(--text-secondary);
	}

	.status-dot,
	.banner-dot {
		width: 8px;
		height: 8px;
		border-radius: 50%;
		background: currentColor;
		flex-shrink: 0;
	}

	.summary-grid {
		display: grid;
		grid-template-columns: repeat(4, minmax(0, 1fr));
		gap: 12px;
	}

	.metric-card,
	.card,
	.notice-card,
	.hint-card {
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 8px;
	}

	.metric-card {
		display: flex;
		flex-direction: column;
		gap: 8px;
		min-height: 104px;
		padding: 14px;
	}

	.metric-card span,
	.metric-card small {
		color: var(--text-secondary);
		font-size: 12px;
	}

	.metric-card strong {
		overflow: hidden;
		color: var(--text-primary);
		font-size: 28px;
		line-height: 1.2;
		text-overflow: ellipsis;
		white-space: nowrap;
		font-variant-numeric: tabular-nums;
	}

	.metric-card strong.sm {
		font-size: 18px;
	}

	.notice-card {
		display: flex;
		flex-direction: column;
		gap: 6px;
		padding: 16px;
		color: var(--text-secondary);
		font-size: 13px;
	}

	.notice-card strong {
		color: var(--text-primary);
		font-size: 14px;
	}

	.card {
		padding: 16px;
	}

	.panel-heading {
		display: flex;
		align-items: flex-start;
		justify-content: space-between;
		gap: 12px;
		margin-bottom: 14px;
	}

	.eyebrow,
	.inline-count {
		color: var(--text-secondary);
		font-size: 11px;
		text-transform: uppercase;
	}

	.panel-heading h3 {
		margin-top: 3px;
		color: var(--text-primary);
		font-size: 16px;
		font-weight: 600;
	}

	.release-banner {
		display: grid;
		grid-template-columns: auto minmax(0, 1fr);
		gap: 12px;
		align-items: flex-start;
		padding: 14px;
		border: 1px solid var(--border-color);
		border-radius: 8px;
		background: var(--bg-tertiary);
	}

	.release-banner.active {
		border-color: rgba(63, 185, 80, 0.34);
		background: rgba(63, 185, 80, 0.08);
	}

	.release-banner strong {
		display: block;
		margin-bottom: 4px;
		color: var(--text-primary);
		font-size: 14px;
	}

	.release-banner span,
	.hint-card span {
		color: var(--text-secondary);
		font-size: 12px;
		line-height: 1.6;
	}

	.error-line {
		margin-top: 12px;
		padding: 10px 12px;
		border: 1px solid rgba(248, 81, 73, 0.32);
		border-radius: 6px;
		background: rgba(248, 81, 73, 0.08);
		color: var(--accent-red);
		font-size: 12px;
	}

	.action-row {
		display: flex;
		flex-wrap: wrap;
		gap: 8px;
		margin-top: 14px;
	}

	.hint-grid {
		display: grid;
		grid-template-columns: repeat(2, minmax(0, 1fr));
		gap: 12px;
	}

	.hint-card {
		display: flex;
		flex-direction: column;
		gap: 6px;
		padding: 14px;
	}

	.hint-card strong {
		color: var(--text-primary);
		font-size: 13px;
	}

	.btn {
		display: inline-flex;
		align-items: center;
		justify-content: center;
		gap: 6px;
		min-height: 36px;
		padding: 8px 12px;
		border: 1px solid var(--border-color);
		border-radius: 6px;
		background: var(--bg-tertiary);
		color: var(--text-primary);
		font-size: 13px;
		transition: all 0.18s;
		white-space: nowrap;
	}

	.btn:hover {
		background: var(--surface-hover);
	}

	.btn:disabled {
		opacity: 0.5;
		cursor: not-allowed;
	}

	.btn-primary {
		background: var(--accent-blue);
		border-color: var(--accent-blue);
		color: white;
	}

	.btn-primary:hover {
		background: var(--accent-blue-hover);
	}

	@media (max-width: 1080px) {
		.summary-grid {
			grid-template-columns: repeat(2, minmax(0, 1fr));
		}
	}

	@media (max-width: 720px) {
		.page-header {
			flex-direction: column;
		}

		.hint-grid {
			grid-template-columns: 1fr;
		}
	}

	@media (max-width: 520px) {
		.summary-grid {
			grid-template-columns: 1fr;
		}

		.action-row {
			flex-direction: column;
		}

		.action-row .btn {
			width: 100%;
		}
	}
</style>