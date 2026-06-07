<script lang="ts">
	import { connection } from '$lib/stores/connection';
	import { settings } from '$lib/stores/settings';
	import { logs } from '$lib/stores/logs';
	import { profiles, activeProfile, type GameProfile } from '$lib/stores/profiles';

	let editingProfile = $state<GameProfile | null>(null);
	let showCreateDialog = $state(false);
	let newProfileName = $state('');

	async function handleConnect() {
		try {
			if ($connection.connected) {
				await connection.disconnect();
				logs.add('已断开连接', 'warning');
			} else {
				await connection.connect($settings.ipcEndpoint);
				logs.add('已连接到本地 runtime IPC', 'success');
			}
		} catch (error: any) {
			logs.add('连接失败: ' + error, 'error');
		}
	}

	function selectProfile(id: string) {
		const p = $profiles.find(p => p.id === id);
		editingProfile = p ? structuredClone(p) : null;
	}

	async function createProfile() {
		if (!newProfileName.trim()) return;
		const id = await profiles.create(newProfileName.trim());
		if (id) {
			logs.add(`已创建配置: ${newProfileName.trim()}`, 'success');
			newProfileName = '';
			showCreateDialog = false;
			selectProfile(id);
		}
	}

	async function saveProfile() {
		if (!editingProfile) return;
		await profiles.update(editingProfile);
		logs.add(`已保存配置: ${editingProfile.name}`, 'success');
	}

	async function deleteProfile(id: string) {
		const p = $profiles.find(p => p.id === id);
		if (!p) return;
		await profiles.remove(id);
		if (editingProfile?.id === id) editingProfile = null;
		logs.add(`已删除配置: ${p.name}`, 'info');
	}

	async function exportProfile(id: string) {
		const json = await profiles.exportToJson(id);
		if (json) {
			await navigator.clipboard.writeText(json);
			logs.add('配置已复制到剪贴板', 'success');
		}
	}

	async function importProfile() {
		const json = await navigator.clipboard.readText();
		if (!json) return;
		const ok = await profiles.importFromJson(json);
		logs.add(ok ? '导入成功' : '导入失败', ok ? 'success' : 'error');
	}

	function updateProfileField(field: keyof GameProfile, value: any) {
		if (!editingProfile) return;
		editingProfile = { ...editingProfile, [field]: value };
	}

	function updateWindowField(field: string, value: any) {
		if (!editingProfile) return;
		editingProfile = {
			...editingProfile,
			window: { ...editingProfile.window, [field]: value },
		};
	}

	function updateHotkeyField(field: 'start' | 'stop' | 'pause' | 'emergencyStop', value: string) {
		if (!editingProfile) return;
		editingProfile = {
			...editingProfile,
			hotkeys: {
				...editingProfile.hotkeys,
				[field]: value
					.split(',')
					.map(v => v.trim().toUpperCase())
					.filter(Boolean),
			},
		};
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
			<span class="form-label">本地 IPC 端点</span>
			<div class="connect-config">
				<input
					type="text"
					id="ipcEndpointInput"
					class="form-input"
					value={$settings.ipcEndpoint}
					onchange={(e) => settings.update({ ipcEndpoint: (e.target as HTMLInputElement).value })}
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

<!-- Profile 管理 -->
<div class="profiles-layout">
	<div class="card">
		<div class="card-header">
			<span class="card-title">游戏配置</span>
			<div class="card-actions">
				<button class="btn btn-sm" onclick={importProfile}>导入</button>
				<button class="btn btn-sm btn-primary" onclick={() => showCreateDialog = true}>新建配置</button>
			</div>
		</div>
		<div class="card-body">
			{#if showCreateDialog}
				<div class="create-dialog">
					<input
						type="text"
						class="form-input"
						placeholder="配置名称"
						bind:value={newProfileName}
						onkeydown={(e) => e.key === 'Enter' && createProfile()}
					/>
					<div class="dialog-actions">
						<button class="btn btn-sm" onclick={() => { showCreateDialog = false; newProfileName = ''; }}>取消</button>
						<button class="btn btn-sm btn-primary" onclick={createProfile}>创建</button>
					</div>
				</div>
			{/if}

			{#if $profiles.length === 0}
				<div class="empty-state">暂无配置，点击"新建配置"创建</div>
			{:else}
				<div class="profile-list">
					{#each $profiles as profile}
						<div class="profile-card" class:active={$activeProfile?.id === profile.id}>
							<button class="profile-card-info" onclick={() => selectProfile(profile.id)}>
								<div class="profile-card-name">{profile.name}</div>
								<div class="profile-card-desc">{profile.description || profile.id}</div>
							</button>
							<div class="profile-card-actions">
								{#if $activeProfile?.id !== profile.id}
									<button class="btn-icon-sm" title="激活" onclick={() => profiles.setActive(profile.id)}>
										<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="5 3 19 12 5 21 5 3"></polygon></svg>
									</button>
								{:else}
									<span class="active-badge">当前</span>
								{/if}
								<button class="btn-icon-sm" title="导出" onclick={() => exportProfile(profile.id)}>
									<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path><polyline points="7 10 12 15 17 10"></polyline><line x1="12" y1="15" x2="12" y2="3"></line></svg>
								</button>
								<button class="btn-icon-sm" title="删除" onclick={() => deleteProfile(profile.id)}>
									<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path></svg>
								</button>
							</div>
						</div>
					{/each}
				</div>
			{/if}
		</div>
	</div>

	<!-- 编辑面板 -->
	<div class="card">
		<div class="card-header">
			<span class="card-title">{editingProfile ? `编辑: ${editingProfile.name}` : '选择一个配置'}</span>
		</div>
		<div class="card-body">
			{#if !editingProfile}
				<div class="empty-state">从左侧选择一个配置进行编辑</div>
			{:else}
				<div class="editor-form">
					<div class="form-group">
						<span class="form-label">名称</span>
						<input
							type="text"
							class="form-input"
							value={editingProfile.name}
							oninput={(e) => updateProfileField('name', (e.target as HTMLInputElement).value)}
						/>
					</div>
					<div class="form-group">
						<span class="form-label">描述</span>
						<input
							type="text"
							class="form-input"
							value={editingProfile.description}
							oninput={(e) => updateProfileField('description', (e.target as HTMLInputElement).value)}
						/>
					</div>

					<div class="form-section">
						<h4>窗口绑定</h4>
						<div class="form-group">
							<span class="form-label">窗口标题</span>
							<input
								type="text"
								class="form-input"
								value={editingProfile.window.title}
								oninput={(e) => updateWindowField('title', (e.target as HTMLInputElement).value)}
								placeholder="窗口标题（模糊匹配）"
							/>
						</div>
						<div class="form-group">
							<span class="form-label">进程名</span>
							<input
								type="text"
								class="form-input"
								value={editingProfile.window.processName}
								oninput={(e) => updateWindowField('processName', (e.target as HTMLInputElement).value)}
								placeholder="game.exe"
							/>
						</div>
						<div class="form-row">
							<label class="checkbox-label">
								<input
									type="checkbox"
									checked={editingProfile.window.exactMatch}
									onchange={() => updateWindowField('exactMatch', !editingProfile!.window.exactMatch)}
								/>
								<span>精确匹配</span>
							</label>
							<label class="checkbox-label">
								<input
									type="checkbox"
									checked={editingProfile.window.fullscreen}
									onchange={() => updateWindowField('fullscreen', !editingProfile!.window.fullscreen)}
								/>
								<span>全屏模式</span>
							</label>
						</div>
					</div>

					<div class="form-section">
						<h4>颜色配置 ({editingProfile.colors.length})</h4>
						{#if editingProfile.colors.length === 0}
							<div class="empty-hint">暂无颜色配置</div>
						{:else}
							{#each editingProfile.colors as color, i}
								<div class="inline-item">
									<span class="color-dot" style="background: rgb({color.r},{color.g},{color.b})"></span>
									<span>{color.name}</span>
									<span class="item-detail">容差 {color.tolerance}</span>
								</div>
							{/each}
						{/if}
					</div>

					<div class="form-section">
						<h4>图像配置 ({editingProfile.images.length})</h4>
						{#if editingProfile.images.length === 0}
							<div class="empty-hint">暂无图像配置</div>
						{:else}
							{#each editingProfile.images as image}
								<div class="inline-item">
									<span>{image.name}</span>
									<span class="item-detail">{image.path}</span>
								</div>
							{/each}
						{/if}
					</div>

					<div class="form-section">
						<h4>触发器 ({editingProfile.triggers.length})</h4>
						{#if editingProfile.triggers.length === 0}
							<div class="empty-hint">暂无触发器</div>
						{:else}
							{#each editingProfile.triggers as trigger}
								<div class="inline-item">
									<span>{trigger.name}</span>
									<span class="item-detail">{trigger.type} &middot; {trigger.enabled ? '启用' : '禁用'}</span>
								</div>
							{/each}
						{/if}
					</div>

					<div class="form-section">
						<h4>脚本 ({editingProfile.scripts.length})</h4>
						{#if editingProfile.scripts.length === 0}
							<div class="empty-hint">暂无脚本</div>
						{:else}
							{#each editingProfile.scripts as script}
								<div class="inline-item">
									<span>{script.name}</span>
									<span class="item-detail">{script.path}</span>
								</div>
							{/each}
						{/if}
					</div>

					<div class="form-section">
						<h4>热键</h4>
						<div class="form-group">
							<span class="form-label">启动</span>
							<input
								type="text"
								class="form-input"
								value={editingProfile.hotkeys.start.join(', ')}
								oninput={(e) => updateHotkeyField('start', (e.target as HTMLInputElement).value)}
								placeholder="F5"
							/>
						</div>
						<div class="form-group">
							<span class="form-label">停止</span>
							<input
								type="text"
								class="form-input"
								value={editingProfile.hotkeys.stop.join(', ')}
								oninput={(e) => updateHotkeyField('stop', (e.target as HTMLInputElement).value)}
								placeholder="F6"
							/>
						</div>
						<div class="form-group">
							<span class="form-label">暂停/恢复</span>
							<input
								type="text"
								class="form-input"
								value={editingProfile.hotkeys.pause.join(', ')}
								oninput={(e) => updateHotkeyField('pause', (e.target as HTMLInputElement).value)}
								placeholder="F7"
							/>
						</div>
						<div class="form-group">
							<span class="form-label">急停</span>
							<input
								type="text"
								class="form-input"
								value={editingProfile.hotkeys.emergencyStop.join(', ')}
								oninput={(e) => updateHotkeyField('emergencyStop', (e.target as HTMLInputElement).value)}
								placeholder="F12"
							/>
						</div>
						<div class="empty-hint">使用逗号分隔多个快捷键，例如 `CTRL+SHIFT+P, F7`。</div>
						<div class="empty-hint">当前语义：启动=启动当前配置脚本，停止=停止当前配置脚本，暂停=暂停/恢复全部已加载脚本，急停=立即停止全部已加载脚本。</div>
					</div>

					<div class="form-actions">
						<button class="btn btn-primary" onclick={saveProfile}>保存</button>
						<button class="btn" onclick={() => profiles.setActive(editingProfile!.id)}>设为当前</button>
					</div>
				</div>
			{/if}
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
	.card-actions { display: flex; gap: 8px; }

	.form-group { margin-bottom: 20px; }
	.form-label { display: block; font-size: 13px; color: var(--text-secondary); margin-bottom: 8px; }

	.form-input {
		width: 100%;
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

	.form-row { display: flex; gap: 20px; margin-bottom: 12px; }
	.form-section { border-top: 1px solid var(--border-color); padding-top: 16px; margin-bottom: 16px; }
	.form-section h4 { font-size: 13px; font-weight: 600; color: var(--text-primary); margin-bottom: 12px; }

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
	.btn-sm { padding: 6px 12px; font-size: 12px; }

	.profiles-layout {
		display: grid;
		grid-template-columns: 320px 1fr;
		gap: 16px;
	}

	.create-dialog {
		padding: 12px;
		background: var(--bg-tertiary);
		border-radius: 6px;
		margin-bottom: 12px;
	}
	.dialog-actions { display: flex; gap: 8px; margin-top: 8px; justify-content: flex-end; }

	.profile-list { display: flex; flex-direction: column; gap: 4px; }
	.profile-card {
		display: flex;
		align-items: center;
		justify-content: space-between;
		padding: 10px 12px;
		border-radius: 6px;
		border: 1px solid transparent;
		transition: all 0.15s;
	}
	.profile-card:hover { background: var(--bg-tertiary); }
	.profile-card.active { background: var(--bg-tertiary); border-color: var(--accent-blue); }
	.profile-card-info {
		cursor: pointer; flex: 1; min-width: 0;
		background: transparent; border: none; padding: 0; text-align: left;
	}
	.profile-card-name { font-size: 13px; font-weight: 500; color: var(--text-primary); }
	.profile-card-desc { font-size: 11px; color: var(--text-secondary); margin-top: 2px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }

	.profile-card-actions { display: flex; gap: 4px; align-items: center; }
	.btn-icon-sm {
		padding: 4px; background: transparent; border: none;
		color: var(--text-secondary); cursor: pointer; border-radius: 4px;
		display: flex; align-items: center;
	}
	.btn-icon-sm:hover { background: var(--border-color); color: var(--text-primary); }
	.active-badge { font-size: 11px; color: var(--accent-green); font-weight: 500; padding: 0 4px; }

	.editor-form { display: flex; flex-direction: column; gap: 16px; }
	.form-actions { display: flex; gap: 8px; padding-top: 16px; border-top: 1px solid var(--border-color); }

	.empty-state { text-align: center; padding: 40px 20px; color: var(--text-secondary); font-size: 13px; }
	.empty-hint { font-size: 12px; color: var(--text-secondary); padding: 8px 0; }

	.inline-item {
		display: flex; align-items: center; gap: 8px;
		padding: 6px 0; font-size: 13px; color: var(--text-primary);
	}
	.item-detail { font-size: 11px; color: var(--text-secondary); margin-left: auto; }
	.color-dot { width: 12px; height: 12px; border-radius: 2px; border: 1px solid var(--border-color); flex-shrink: 0; }
</style>
