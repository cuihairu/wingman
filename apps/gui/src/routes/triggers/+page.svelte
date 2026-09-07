<script lang="ts">
	import {
		triggers,
		type TriggerConfig,
		type TriggerConditionType,
		type TriggerAction,
	} from '$lib/stores/triggers';
	import { logs } from '$lib/stores/logs';
	import RegionPicker from '$lib/components/RegionPicker.svelte';
	import ColorPicker from '$lib/components/ColorPicker.svelte';
	import ScreenPickerModal, { type PickerResult } from '$lib/components/ScreenPickerModal.svelte';

	let selectedId = $state<string | null>(null);
	let editing = $state<TriggerConfig | null>(null);

	/// 区域拾取 / 点击坐标拾取 / 像素坐标拾取的模态来源
	let regionPickOpen = $state(false);
	let clickPickIndex = $state<number | null>(null);
	let pixelPickOpen = $state(false);

	/// 条件类型按语义分组（optgroup 展示）
	const conditionGroups = [
		{
			label: '视觉检测',
			items: [
				{ value: 'color_found', label: '颜色找到' },
				{ value: 'color_lost', label: '颜色消失' },
				{ value: 'image_found', label: '图像匹配' },
				{ value: 'image_lost', label: '图像消失' },
				{ value: 'pixel_changed', label: '像素变化' },
			],
		},
		{
			label: '窗口 / 进程',
			items: [
				{ value: 'window_opened', label: '窗口打开' },
				{ value: 'window_closed', label: '窗口关闭' },
				{ value: 'process_started', label: '进程启动' },
				{ value: 'process_stopped', label: '进程退出' },
			],
		},
		{
			label: '时间 / 输入',
			items: [
				{ value: 'time_elapsed', label: '定时触发' },
				{ value: 'hotkey_pressed', label: '热键按下' },
			],
		},
	];

	const conditionLabels: Record<string, string> = Object.fromEntries(
		conditionGroups.flatMap(g => g.items.map(i => [i.value, i.label]))
	);

	const actionTypes = [
		{ value: 'run_script', label: '运行脚本' },
		{ value: 'stop_script', label: '停止脚本' },
		{ value: 'pause_script', label: '暂停脚本' },
		{ value: 'click', label: '鼠标点击' },
		{ value: 'key_press', label: '按键' },
		{ value: 'type', label: '输入文本' },
		{ value: 'delay', label: '延时' },
		{ value: 'show_message', label: '显示消息' },
		{ value: 'play_audio', label: '播放音频' },
		{ value: 'log', label: '日志' },
	];

	const actionLabels: Record<string, string> = Object.fromEntries(
		actionTypes.map(a => [a.value, a.label])
	);

	/// 各条件类型的 value 语义
	function conditionValueLabel(type: TriggerConditionType): string | null {
		switch (type) {
			case 'color_found':
			case 'color_lost':
				return null; // 由 ColorPicker 接管
			case 'image_found':
			case 'image_lost':
				return '模板图片路径';
			case 'pixel_changed':
				return '监测像素坐标 (x,y)';
			case 'window_opened':
			case 'window_closed':
				return '窗口标题（模糊匹配）';
			case 'process_started':
			case 'process_stopped':
				return '进程名 (如 game.exe)';
			case 'hotkey_pressed':
				return '热键（单键，如 F9 / 1 / A）';
			default:
				return null;
		}
	}

	/// 条件是否需要搜索区域
	function conditionUsesRegion(type: TriggerConditionType): boolean {
		return type === 'color_found' || type === 'color_lost'
			|| type === 'image_found' || type === 'image_lost';
	}

	/// 条件是否需要容差
	function conditionUsesTolerance(type: TriggerConditionType): boolean {
		return conditionUsesRegion(type) || type === 'pixel_changed';
	}

	/// 动作 value 字段占位提示
	function actionValuePlaceholder(type: string): string {
		switch (type) {
			case 'run_script': return 'scripts/farm.lua';
			case 'stop_script':
			case 'pause_script': return '脚本名或路径';
			case 'key_press': return 'F1 / 1 / ctrl+s';
			case 'type': return '要输入的文本';
			case 'show_message': return '消息内容';
			case 'play_audio': return 'audio/alert.wav';
			case 'log': return '日志内容';
			default: return '参数';
		}
	}

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

	function moveAction(index: number, delta: number) {
		if (!editing) return;
		const target = index + delta;
		if (target < 0 || target >= editing.actions.length) return;
		const actions = [...editing.actions];
		[actions[index], actions[target]] = [actions[target], actions[index]];
		editing = { ...editing, actions };
	}

	function updateAction(index: number, field: string, value: any) {
		if (!editing) return;
		const actions = [...editing.actions];
		actions[index] = { ...actions[index], [field]: value };
		editing = { ...editing, actions };
	}

	/// 切换动作类型时重置无关字段，保留可复用字段
	function changeActionType(index: number, type: TriggerAction['type']) {
		if (!editing) return;
		const actions = [...editing.actions];
		const prev = actions[index];
		actions[index] = {
			type,
			value: prev?.type === 'click' || type === 'click' ? prev?.value : prev?.value ?? '',
			x: type === 'click' ? (prev?.x ?? 0) : undefined,
			y: type === 'click' ? (prev?.y ?? 0) : undefined,
			delay: type === 'delay' ? (prev?.delay ?? 500) : undefined,
		};
		editing = { ...editing, actions };
	}

	function parseInt0(v: string): number {
		return parseInt(v) || 0;
	}

	function parsePixelPoint(value: string): { x: number; y: number } {
		const [x, y] = value.split(',').map(v => parseInt(v.trim()));
		return { x: Number.isFinite(x) ? x : 0, y: Number.isFinite(y) ? y : 0 };
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

	function formatFiredTime(ts?: number): string {
		if (!ts) return '';
		const d = new Date(ts);
		if (Number.isNaN(d.getTime())) return '';
		return d.toTimeString().split(' ')[0];
	}

	// ---- 拾取回调 ----

	function handleRegionPick(result: PickerResult) {
		if (editing && result.region) {
			editing = {
				...editing,
				condition: { ...editing.condition, region: result.region },
			};
		}
		regionPickOpen = false;
	}

	function handleClickPick(result: { position?: { x: number; y: number } }) {
		if (editing && clickPickIndex !== null && result.position) {
			updateAction(clickPickIndex, 'x', result.position.x);
			updateAction(clickPickIndex, 'y', result.position.y);
		}
		clickPickIndex = null;
	}

	function handlePixelPick(result: { position?: { x: number; y: number } }) {
		if (editing && result.position) {
			editing = {
				...editing,
				condition: {
					...editing.condition,
					value: `${result.position.x},${result.position.y}`,
				},
			};
		}
		pixelPickOpen = false;
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
					{#each $triggers as trigger (trigger.id)}
						<div
							class="trigger-item"
							class:active={selectedId === trigger.id}
							role="button"
							tabindex="0"
							onclick={() => selectTrigger(trigger.id)}
							onkeydown={(event) => handleTriggerKeydown(event, trigger.id)}
						>
							<div class="trigger-info">
								<div class="trigger-name-row">
									<span
										class="fired-dot"
										class:lit={!!trigger.last_triggered}
										title={trigger.last_triggered_at
											? `最近命中 ${formatFiredTime(trigger.last_triggered_at)}`
											: '尚未命中'}
									></span>
									<div class="trigger-name">{trigger.name}</div>
								</div>
								<div class="trigger-type">{conditionLabels[trigger.condition.type] || trigger.condition.type}</div>
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
			<span class="card-title">{editing ? `编辑触发器${editing.name ? `: ${editing.name}` : ''}` : '选择一个触发器'}</span>
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
								{#each conditionGroups as group}
									<optgroup label={group.label}>
										{#each group.items as ct}
											<option value={ct.value}>{ct.label}</option>
										{/each}
									</optgroup>
								{/each}
							</select>
						</div>

						{#if editing.condition.type === 'color_found' || editing.condition.type === 'color_lost'}
							<div class="form-group">
								<span class="field-label">目标颜色（可从屏幕取色）</span>
								<ColorPicker bind:value={editing.condition.value} bind:tolerance={editing.condition.tolerance} />
							</div>
						{:else if conditionValueLabel(editing.condition.type)}
							<div class="form-group">
								<span class="field-label">{conditionValueLabel(editing.condition.type)}</span>
								<div class="value-with-pick">
									<input
										type="text"
										bind:value={editing.condition.value}
										placeholder={editing.condition.type === 'pixel_changed' ? '100,200' : ''}
									/>
									{#if editing.condition.type === 'pixel_changed'}
										<button class="pick-btn" title="从屏幕拾取像素坐标" onclick={() => pixelPickOpen = true}>
											<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
												<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path>
												<circle cx="12" cy="12" r="3"></circle>
											</svg>
											拾取
										</button>
									{/if}
								</div>
							</div>
						{/if}

						{#if conditionUsesRegion(editing.condition.type)}
							<div class="form-group">
								<span class="field-label">搜索区域（可在截图上框选）</span>
								<RegionPicker bind:value={editing.condition.region} />
								{#if editing.condition.region.width === 0 && editing.condition.region.height === 0}
									<span class="field-hint">区域为 0 时搜索整个屏幕</span>
								{/if}
							</div>
						{/if}

						{#if conditionUsesTolerance(editing.condition.type)}
							<div class="form-group">
								<span class="field-label">容差</span>
								<input type="number" min="0" max="255" bind:value={editing.condition.tolerance} />
							</div>
						{/if}

						<div class="form-group">
							<label for="triggerCheckIntervalInput">检查间隔 (ms)</label>
							<input id="triggerCheckIntervalInput" type="number" min="50" bind:value={editing.condition.interval} placeholder="1000" />
						</div>
					</div>

					<!-- 动作列表 -->
					<div class="form-section">
						<h4>触发动作（按顺序执行）</h4>
						{#if editing.actions.length === 0}
							<div class="field-hint">尚无动作，点击下方按钮添加</div>
						{/if}
						{#each editing.actions as action, i}
							<div class="action-card">
								<div class="action-head">
									<span class="action-index">{i + 1}</span>
									<select value={action.type} onchange={(e) => changeActionType(i, (e.target as HTMLSelectElement).value as TriggerAction['type'])}>
										{#each actionTypes as at}
											<option value={at.value}>{at.label}</option>
										{/each}
									</select>
									<div class="action-ops">
										<button class="op-btn" onclick={() => moveAction(i, -1)} disabled={i === 0} title="上移">↑</button>
										<button class="op-btn" onclick={() => moveAction(i, 1)} disabled={i === editing.actions.length - 1} title="下移">↓</button>
										<button class="op-btn danger" onclick={() => removeAction(i)} title="删除动作">✕</button>
									</div>
								</div>
								<div class="action-body">
									{#if action.type === 'click'}
										<div class="action-fields">
											<label>
												<span>X</span>
												<input type="number" value={action.x || 0} oninput={(e) => updateAction(i, 'x', parseInt0((e.target as HTMLInputElement).value))} />
											</label>
											<label>
												<span>Y</span>
												<input type="number" value={action.y || 0} oninput={(e) => updateAction(i, 'y', parseInt0((e.target as HTMLInputElement).value))} />
											</label>
											<button class="pick-btn" title="从屏幕拾取点击坐标" onclick={() => clickPickIndex = i}>
												<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
													<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path>
													<circle cx="12" cy="12" r="3"></circle>
												</svg>
												拾取坐标
											</button>
										</div>
									{:else if action.type === 'delay'}
										<div class="action-fields">
											<label>
												<span>延时 (ms)</span>
												<input type="number" min="0" step="50" value={action.delay || 0} oninput={(e) => updateAction(i, 'delay', parseInt0((e.target as HTMLInputElement).value))} />
											</label>
										</div>
									{:else}
										<div class="action-fields">
											<label class="grow">
												<span>{actionLabels[action.type] || '参数'}</span>
												<input
													type="text"
													value={action.value || ''}
													oninput={(e) => updateAction(i, 'value', (e.target as HTMLInputElement).value)}
													placeholder={actionValuePlaceholder(action.type)}
												/>
											</label>
										</div>
									{/if}
								</div>
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
							<input id="triggerCooldownInput" type="number" min="0" bind:value={editing.cooldown} placeholder="0" />
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

{#if regionPickOpen}
	<ScreenPickerModal mode="region" onconfirm={handleRegionPick} onclose={() => regionPickOpen = false} />
{/if}

{#if clickPickIndex !== null}
	<ScreenPickerModal mode="color" onconfirm={handleClickPick} onclose={() => clickPickIndex = null} />
{/if}

{#if pixelPickOpen}
	<ScreenPickerModal mode="color" onconfirm={handlePixelPick} onclose={() => pixelPickOpen = false} />
{/if}

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
	.card-title { font-size: 15px; font-weight: 500; color: var(--text-primary); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
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
	.trigger-name-row { display: flex; align-items: center; gap: 6px; min-width: 0; }
	.trigger-name { font-size: 13px; font-weight: 500; color: var(--text-primary); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
	.trigger-type { font-size: 11px; color: var(--text-secondary); margin-top: 2px; }

	.fired-dot {
		width: 7px; height: 7px; border-radius: 50%;
		background: var(--border-color); flex-shrink: 0;
	}
	.fired-dot.lit {
		background: var(--accent-green);
		box-shadow: 0 0 6px rgba(63, 185, 80, 0.6);
	}

	.toggle-btn {
		padding: 2px 8px; border-radius: 4px; font-size: 11px;
		background: var(--bg-tertiary); border: 1px solid var(--border-color);
		color: var(--text-secondary); cursor: pointer; flex-shrink: 0;
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
	.field-hint { font-size: 11px; color: var(--text-secondary); }

	.value-with-pick { display: flex; gap: 6px; align-items: center; }
	.value-with-pick input { flex: 1; padding: 6px 8px; background: var(--bg-tertiary); border: 1px solid var(--border-color); border-radius: 4px; color: var(--text-primary); font-size: 13px; }
	.value-with-pick input:focus { outline: none; border-color: var(--accent-blue); }

	.pick-btn {
		display: inline-flex; align-items: center; gap: 4px;
		padding: 5px 8px; font-size: 12px; flex-shrink: 0;
		border: 1px solid var(--border-color); border-radius: 4px;
		background: var(--bg-tertiary); color: var(--text-secondary);
		cursor: pointer; white-space: nowrap;
	}
	.pick-btn:hover { background: var(--border-color); color: var(--text-primary); }

	.action-card {
		border: 1px solid var(--border-color);
		border-radius: 6px;
		padding: 8px 10px;
		margin-bottom: 8px;
		background: var(--bg-tertiary);
	}
	.action-head { display: flex; align-items: center; gap: 8px; }
	.action-index {
		font-size: 11px; color: var(--text-secondary);
		width: 18px; height: 18px; border-radius: 50%;
		border: 1px solid var(--border-color);
		display: flex; align-items: center; justify-content: center;
		flex-shrink: 0;
	}
	.action-head select {
		flex: 1; padding: 4px 6px; font-size: 12px;
		background: var(--bg-secondary); border: 1px solid var(--border-color);
		border-radius: 4px; color: var(--text-primary);
	}
	.action-ops { display: flex; gap: 2px; flex-shrink: 0; }
	.op-btn {
		padding: 2px 6px; font-size: 12px; line-height: 1.4;
		background: transparent; border: none; border-radius: 4px;
		color: var(--text-secondary); cursor: pointer;
	}
	.op-btn:hover:not(:disabled) { background: var(--border-color); color: var(--text-primary); }
	.op-btn:disabled { opacity: 0.35; cursor: not-allowed; }
	.op-btn.danger:hover { color: var(--accent-red); }

	.action-body { margin-top: 8px; }
	.action-fields { display: flex; gap: 8px; align-items: flex-end; flex-wrap: wrap; }
	.action-fields label {
		display: flex; flex-direction: column; gap: 2px;
		font-size: 11px; color: var(--text-secondary);
	}
	.action-fields label.grow { flex: 1; min-width: 160px; }
	.action-fields input {
		padding: 5px 7px; background: var(--bg-secondary);
		border: 1px solid var(--border-color); border-radius: 4px;
		color: var(--text-primary); font-size: 12px;
		width: 90px;
	}
	.action-fields label.grow input { width: 100%; }
	.action-fields input:focus { outline: none; border-color: var(--accent-blue); }

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
