<script lang="ts">
	import ScreenPickerModal from './ScreenPickerModal.svelte';

	let {
		value = $bindable('#ff0000'),
		tolerance = $bindable(10),
	} = $props<{ value: string; tolerance: number }>();

	let showPicker = $state(false);

	let isValid = $derived(/^#[0-9a-fA-F]{6}$/.test(value));

	function handleInput(e: Event) {
		value = (e.target as HTMLInputElement).value;
	}

	function handlePick(color: string) {
		value = color;
		showPicker = false;
	}
</script>

<div class="color-picker">
	<div class="color-preview" style="background-color: {isValid ? value : '#000'}"></div>
	<input
		type="text"
		class="color-input"
		class:invalid={!isValid}
		{value}
		oninput={handleInput}
		placeholder="#ff0000"
	/>
	<button class="pick-btn" title="从屏幕截图上吸取颜色" onclick={() => showPicker = true}>
		<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
			<path d="M11 7l6 6"></path>
			<path d="M20.71 5.63l-2.34-2.34a1 1 0 0 0-1.41 0l-3.1 3.1 3.75 3.75 3.1-3.1a1 1 0 0 0 0-1.41z"></path>
			<path d="M4 20l1-4L14.06 6.94l3.75 3.75L8.75 19.75 4 20z"></path>
		</svg>
		取色
	</button>
	<label class="tolerance-label">
		<span>容差</span>
		<input
			type="range"
			min="0"
			max="100"
			value={tolerance}
			oninput={(e) => tolerance = parseInt((e.target as HTMLInputElement).value) || 0}
		/>
		<span class="tolerance-value">{tolerance}</span>
	</label>
</div>

{#if showPicker}
	<ScreenPickerModal
		mode="color"
		onconfirm={(result) => handlePick(result.color || '#ff0000')}
		onclose={() => showPicker = false}
	/>
{/if}

<style>
	.color-picker { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }
	.color-preview {
		width: 28px; height: 28px; border-radius: 4px;
		border: 1px solid var(--border-color); flex-shrink: 0;
	}
	.color-input {
		width: 90px; padding: 4px 6px;
		background: var(--bg-tertiary); border: 1px solid var(--border-color);
		border-radius: 4px; color: var(--text-primary); font-size: 12px;
		font-family: monospace;
	}
	.color-input.invalid { border-color: var(--accent-red); }
	.pick-btn {
		display: inline-flex; align-items: center; gap: 4px;
		padding: 4px 8px; font-size: 12px;
		border: 1px solid var(--border-color); border-radius: 4px;
		background: var(--bg-tertiary); color: var(--text-secondary);
		cursor: pointer;
	}
	.pick-btn:hover { background: var(--border-color); color: var(--text-primary); }
	.tolerance-label {
		display: flex; align-items: center; gap: 6px;
		font-size: 11px; color: var(--text-secondary);
	}
	.tolerance-label input[type="range"] { width: 80px; }
	.tolerance-value { min-width: 24px; text-align: right; font-size: 12px; }
</style>
