<script lang="ts">
	import { triggers, type TriggerConfig, type TriggerCondition, type TriggerAction } from '$lib/stores/triggers';
	import { connection } from '$lib/stores/connection';
	import { logs } from '$lib/stores/logs';
	import RegionPicker from '$lib/components/RegionPicker.svelte';
	import ColorPicker from '$lib/components/ColorPicker.svelte';

	let selectedId = $state<string | null>(null);
	let editing = $state<TriggerConfig | null>(null);

	const conditionTypes = [
		{ value: 'color_found', label: '颜色找到' },
		{ value: 'color_lost', label: '颜色消失' },
		{ value: 'image_found', label: '图像匹配' },
		{ value: 'image_lost', label: '图像消失' },
		{ value: 'time_elapsed', label: '定时触发' },
		{ value: 'hotkey_pressed', label: '热键按下' },
	];

	const actionTypes = [
		{ value: 'run_script', label: '运行脚本' },
		{ value: 'click', label: '鼠标点击' },
		{ value: 'key_press', label: '按键' },
		{ value: 'type', label: '输入文本' },
		{ value: 'delay', label: '延时' },
		{ value: 'log', label: '日志' },
	];

	function withEditorDefaults(config: TriggerConfig): TriggerConfig {
		return {
			...config,
			condition: {
				...config.condition,
				tolerance: config.condition.tolerance ?? 10,
				interval: config.condition.interval ?? 1000,
				region: config.condition.region ?? { x: 0, y: 0, width: 0, height: 0 },
			},
			oneShot: config.oneShot ?? false,
			cooldown: config.cooldown ?? 0,
		};
	}

	function selectTrigger(id: string) {
		selectedId = id;
		const t = $triggers.find(t => t.id === id);
		editing = t ? withEditorDefaults(structuredClone(t)) : null;
	}

	async function newTrigger() {
		const fallbackId = Date.now().toString();
		const config: TriggerConfig = withEditorDefaults({
			id: fallbackId,
			name: '新建触发器',
			enabled: true,
			condition: { type: 'color_found', value: '#ff0000', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 10, interval: 1000 },
			actions: [],
		});
		const id = await triggers.add(config) || fallbackId;
		selectTrigger(id);
		logs.add('已创建触发器', 'info');
	}

	async function saveTrigger() {
		if (!editing) return;
		await triggers.update(editing.id, editing);
		logs.add(`已保存触发器: ${editing.name}`, 'success');
	}

	async function deleteTrigger() {
		if (!editing) return;
		const confirmed = confirm(`确定删除触发器 "${editing.name}" 吗？此操作不可撤销。`);
		if (!confirmed) return;
		await triggers.remove(editing.id);
		selectedId = null;
		editing = null;
		logs.add('已删除触发器', 'info');
	}

	function addAction() {
		if (!editing) return;
		editing = {
			...editing,
			actions: [...editing.actions, { type: 'log', value: '' }],
		};
	}

	function removeAction(index: number) {
		if (!editing) return;
		editing = {
			...editing,
			actions: editing.actions.filter((_, i) => i !== index),
		};
	}

	function updateAction(index: number, field: string, value: any) {
		if (!editing) return;
		const actions = [...editing.actions];
		actions[index] = { ...actions[index], [field]: value };
		editing = { ...editing, actions };
	}

	async function toggleTrigger(id: string) {
		await triggers.toggle(id);
	}

	function handleTriggerKeydown(event: KeyboardEvent, id: string) {
		if (event.key === 'Enter' || event.key === ' ') {
			event.preventDefault();
			selectTrigger(id);
		}
	}
</script>

<div class="page-header">
	<h2 class="page-title">触发器</h2>
	<p class="page-subtitle">管理和配置自动化触发器</p>
</div>

<div class="triggers-layout">
	<!-- 左侧：触发器列表 -->
	<div class="triggers-list card">
		<div class="card-header">
			<span class="card-title">触发器列表</span>
			<button class="btn btn-primary" onclick={newTrigger}>添加触发器</button>
		</div>
		<div class="card-body">
			{#if $triggers.length === 0}
				<div class="empty-state">
					<div class="empty-state-icon">⚡</div>
					<div>暂无触发器，点击"添加触发器"创建</div>
				</div>
			{:else}
				<div class="trigger-items">
					{#each $triggers as trigger}
						<div
							class="trigger-item"
							class:active={selectedId === trigger.id}
							role="button"
							tabindex="0"
							onclick={() => selectTrigger(trigger.id)}
							onkeydown={(event) => handleTriggerKeydown(event, trigger.id)}
						>
							<div class="trigger-info">
								<div class="trigger-name">{trigger.name}</div>
								<div class="trigger-type">{conditionTypes.find(c => c.value === trigger.condition.type)?.label || trigger.condition.type}</div>
							</div>
							<button
								class="toggle-btn"
								class:enabled={trigger.enabled}
								onclick={(event) => { event.stopPropagation(); toggleTrigger(trigger.id); }}
								title={trigger.enabled ? '点击禁用' : '点击启用'}
							>
								{trigger.enabled ? '开' : '关'}
							</button>
						</div>
					{/each}
				</div>
			{/if}
		</div>
	</div>

	<!-- 右侧：编辑面板 -->
	<div class="editor-panel card">
		<div class="card-header">
			<span class="card-title">{editing ? '编辑触发器' : '选择一个触发器'}</span>
		</div>
		<div class="card-body">
			{#if !editing}
				<div class="empty-state">
					<div class="empty-state-icon">📝</div>
					<div>从左侧选择一个触发器进行编辑</div>
				</div>
			{:else}
				<div class="editor-form">
					<!-- 基本信息 -->
					<div class="form-group">
						<label for="triggerNameInput">名称</label>
						<input id="triggerNameInput" type="text" bind:value={editing.name} placeholder="触发器名称" />
					</div>

					<!-- 条件配置 -->
					<div class="form-section">
						<h4>触发条件</h4>
						<div class="form-group">
							<label for="triggerConditionTypeSelect">条件类型</label>
							<select id="triggerConditionTypeSelect" bind:value={editing.condition.type}>
								{#each conditionTypes as ct}
									<option value={ct.value}>{ct.label}</option>
								{/each}
							</select>
						</div>

						{#if editing.condition.type === 'color_found' || editing.condition.type === 'color_lost'}
							<div class="form-group">
								<span class="field-label">目标颜色</span>
								<ColorPicker bind:value={editing.condition.value} bind:tolerance={editing.condition.tolerance} />
							</div>
						{:else if editing.condition.type === 'image_found' || editing.condition.type === 'image_lost'}
							<div class="form-group">
								<label for="triggerImagePathInput">模板图片路径</label>
								<input id="triggerImagePathInput" type="text" bind:value={editing.condition.value} placeholder="images/template.png" />
							</div>
						{:else if editing.condition.type === 'time_elapsed'}
							<div class="form-group">
								<label for="triggerTimeElapsedInput">间隔 (ms)</label>
								<input id="triggerTimeElapsedInput" type="number" bind:value={editing.condition.interval} placeholder="1000" />
							</div>
						{:else if editing.condition.type === 'hotkey_pressed'}
							<div class="form-group">
								<label for="triggerHotkeyInput">热键</label>
								<input id="triggerHotkeyInput" type="text" bind:value={editing.condition.value} placeholder="F9" />
							</div>
						{/if}

						{#if editing.condition.type !== 'time_elapsed' && editing.condition.type !== 'hotkey_pressed'}
							<div class="form-group">
								<span class="field-label">搜索区域</span>
								<RegionPicker bind:value={editing.condition.region} />
							</div>
						{/if}

						<div class="form-group">
							<label for="triggerCheckIntervalInput">检查间隔 (ms)</label>
							<input id="triggerCheckIntervalInput" type="number" bind:value={editing.condition.interval} placeholder="1000" />
						</div>
					</div>

					<!-- 动作列表 -->
					<div class="form-section">
						<h4>触发动作</h4>
						{#each editing.actions as action, i}
							<div class="action-row">
								<select value={action.type} onchange={(e) => updateAction(i, 'type', (e.target as HTMLSelectElement).value)}>
									{#each actionTypes as at}
										<option value={at.value}>{at.label}</option>
									{/each}
								</select>
								<input
									type="text"
									value={action.value || ''}
									oninput={(e) => updateAction(i, 'value', (e.target as HTMLInputElement).value)}
									placeholder="参数"
								/>
								{#if action.type === 'click'}
									<input type="number" value={action.x || 0} oninput={(e) => updateAction(i, 'x', parseInt((e.target as HTMLInputElement).value) || 0)} placeholder="X" style="width:60px" />
									<input type="number" value={action.y || 0} oninput={(e) => updateAction(i, 'y', parseInt((e.target as HTMLInputElement).value) || 0)} placeholder="Y" style="width:60px" />
								{/if}
								<button class="btn-icon" onclick={() => removeAction(i)} title="删除动作">
									<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
										<line x1="18" y1="6" x2="6" y2="18"></line>
										<line x1="6" y1="6" x2="18" y2="18"></line>
									</svg>
								</button>
							</div>
						{/each}
						<button class="btn btn-sm" onclick={addAction}>+ 添加动作</button>
					</div>

					<!-- 高级选项 -->
					<div class="form-section">
						<h4>高级选项</h4>
						<label class="checkbox-label">
							<input type="checkbox" bind:checked={editing.oneShot} />
							<span>仅触发一次</span>
						</label>
						<div class="form-group">
							<label for="triggerCooldownInput">冷却时间 (ms)</label>
							<input id="triggerCooldownInput" type="number" bind:value={editing.cooldown} placeholder="0" />
						</div>
					</div>

					<!-- 操作按钮 -->
					<div class="form-actions">
						<button class="btn btn-primary" onclick={saveTrigger}>保存</button>
						<button class="btn btn-danger" onclick={deleteTrigger}>删除</button>
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

	.triggers-layout {
		display: grid;
		grid-template-columns: 280px 1fr;
		gap: 16px;
		min-height: calc(100vh - 200px);
	}

	.card {
		background: var(--bg-secondary);
		border: 1px solid var(--border-color);
		border-radius: 8px;
	}
	.card-header {
		padding: 16px;
		border-bottom: 1px solid var(--border-color);
		display: flex; align-items: center; justify-content: space-between;
	}
	.card-title { font-size: 15px; font-weight: 500; color: var(--text-primary); }
	.card-body { padding: 16px; overflow-y: auto; max-height: calc(100vh - 280px); }

	.trigger-items { display: flex; flex-direction: column; gap: 4px; }
	.trigger-item {
		display: flex; align-items: center; justify-content: space-between;
		padding: 10px 12px; border-radius: 6px;
		background: transparent; border: 1px solid transparent;
		cursor: pointer; text-align: left; width: 100%;
		transition: all 0.15s;
	}
	.trigger-item:hover { background: var(--bg-tertiary); }
	.trigger-item.active { background: var(--bg-tertiary); border-color: var(--accent-blue); }
	.trigger-name { font-size: 13px; font-weight: 500; color: var(--text-primary); }
	.trigger-type { font-size: 11px; color: var(--text-secondary); margin-top: 2px; }

	.toggle-btn {
		padding: 2px 8px; border-radius: 4px; font-size: 11px;
		background: var(--bg-tertiary); border: 1px solid var(--border-color);
		color: var(--text-secondary); cursor: pointer;
	}
	.toggle-btn.enabled { background: var(--accent-green); color: white; border-color: var(--accent-green); }

	.editor-form { display: flex; flex-direction: column; gap: 16px; }
	.form-section { border-top: 1px solid var(--border-color); padding-top: 16px; }
	.form-section h4 { font-size: 13px; font-weight: 600; color: var(--text-primary); margin-bottom: 12px; }
	.form-group { display: flex; flex-direction: column; gap: 4px; margin-bottom: 12px; }
	.form-group label, .form-group .field-label { font-size: 12px; color: var(--text-secondary); }
	.form-group input, .form-group select {
		padding: 6px 8px; background: var(--bg-tertiary);
		border: 1px solid var(--border-color); border-radius: 4px;
		color: var(--text-primary); font-size: 13px;
	}
	.form-group input:focus, .form-group select:focus {
		outline: none; border-color: var(--accent-blue);
	}

	.action-row {
		display: flex; gap: 6px; align-items: center; margin-bottom: 8px;
	}
	.action-row select { min-width: 100px; }
	.action-row input { flex: 1; }

	.btn-icon {
		padding: 4px; background: transparent; border: none;
		color: var(--text-secondary); cursor: pointer; border-radius: 4px;
	}
	.btn-icon:hover { background: var(--bg-tertiary); color: var(--accent-red); }

	.checkbox-label {
		display: flex; align-items: center; gap: 8px;
		font-size: 13px; color: var(--text-primary); cursor: pointer;
		margin-bottom: 12px;
	}

	.form-actions { display: flex; gap: 8px; padding-top: 16px; border-top: 1px solid var(--border-color); }

	.btn {
		display: inline-flex; align-items: center; gap: 6px;
		padding: 8px 16px; border: 1px solid var(--border-color);
		border-radius: 6px; background: var(--bg-tertiary);
		color: var(--text-primary); font-size: 14px; cursor: pointer;
		transition: all 0.2s;
	}
	.btn:hover { background: var(--border-color); }
	.btn-primary { background: var(--accent-green); color: white; border-color: var(--accent-green); }
	.btn-primary:hover { background: #2ea043; }
	.btn-danger { background: var(--accent-red); color: white; border-color: var(--accent-red); }
	.btn-danger:hover { background: #da3633; }
	.btn-sm { padding: 4px 10px; font-size: 12px; }

	.empty-state { text-align: center; padding: 40px 20px; color: var(--text-secondary); }
	.empty-state-icon { font-size: 36px; margin-bottom: 12px; opacity: 0.5; }
</style>
