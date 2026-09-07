<script lang="ts">
	import ScreenPickerModal, { type PickerResult } from './ScreenPickerModal.svelte';
	import type { ScreenRegion } from '$lib/stores/screen';

	let {
		value = $bindable({ x: 0, y: 0, width: 0, height: 0 }),
	} = $props<{ value: ScreenRegion }>();

	let showPicker = $state(false);

	function updateField(field: keyof ScreenRegion, val: string) {
		value = { ...value, [field]: parseInt(val) || 0 };
	}

	function handlePick(result: PickerResult) {
		if (result.region) {
			value = { ...result.region };
		}
		showPicker = false;
	}
</script>

<div class="region-picker">
	<div class="region-fields">
		<label>
			<span>X</span>
			<input type="number" value={value.x} oninput={(e) => updateField('x', (e.target as HTMLInputElement).value)} />
		</label>
		<label>
			<span>Y</span>
			<input type="number" value={value.y} oninput={(e) => updateField('y', (e.target as HTMLInputElement).value)} />
		</label>
		<label>
			<span>W</span>
			<input type="number" value={value.width} oninput={(e) => updateField('width', (e.target as HTMLInputElement).value)} />
		</label>
		<label>
			<span>H</span>
			<input type="number" value={value.height} oninput={(e) => updateField('height', (e.target as HTMLInputElement).value)} />
		</label>
	</div>
	<button class="btn-sm" title="从屏幕截图上拖拽选择区域" onclick={() => showPicker = true}>
		<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
			<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path>
			<circle cx="12" cy="12" r="3"></circle>
		</svg>
		拾取
	</button>
</div>

{#if showPicker}
	<ScreenPickerModal
		mode="region"
		onconfirm={handlePick}
		onclose={() => showPicker = false}
	/>
{/if}

<style>
	.region-picker { display: flex; gap: 8px; align-items: center; }
	.region-fields { display: flex; gap: 4px; }
	.region-fields label {
		display: flex; flex-direction: column; gap: 2px;
		font-size: 11px; color: var(--text-secondary);
	}
	.region-fields input {
		width: 60px; padding: 4px 6px;
		background: var(--bg-tertiary); border: 1px solid var(--border-color);
		border-radius: 4px; color: var(--text-primary); font-size: 12px;
	}
	.btn-sm {
		padding: 4px 8px; font-size: 12px;
		display: inline-flex; align-items: center; gap: 4px;
		border: 1px solid var(--border-color); border-radius: 4px;
		background: var(--bg-tertiary); color: var(--text-secondary);
		cursor: pointer;
	}
	.btn-sm:hover { background: var(--border-color); color: var(--text-primary); }
</style>
