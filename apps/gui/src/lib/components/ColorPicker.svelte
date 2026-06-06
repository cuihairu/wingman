<script lang="ts">
	let { value = $bindable('#ff0000'), tolerance = $bindable(10) } = $props<{
		value: string;
		tolerance: number;
	}>();

	let isValid = $derived(/^#[0-9a-fA-F]{6}$/.test(value));

	function handleInput(e: Event) {
		value = (e.target as HTMLInputElement).value;
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

<style>
	.color-picker { display: flex; gap: 8px; align-items: center; }
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
	.tolerance-label {
		display: flex; align-items: center; gap: 6px;
		font-size: 11px; color: var(--text-secondary);
	}
	.tolerance-label input[type="range"] { width: 80px; }
	.tolerance-value { min-width: 24px; text-align: right; font-size: 12px; }
</style>
