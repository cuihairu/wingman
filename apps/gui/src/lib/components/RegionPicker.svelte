<script lang="ts">
	interface Region {
		x: number;
		y: number;
		width: number;
		height: number;
	}

	let { value = $bindable({ x: 0, y: 0, width: 0, height: 0 }) } = $props<{ value: Region }>();

	function updateField(field: keyof Region, val: string) {
		value = { ...value, [field]: parseInt(val) || 0 };
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
	<button class="btn btn-sm" title="从屏幕拾取区域（开发中）">
		<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
			<circle cx="12" cy="12" r="10"></circle>
			<line x1="12" y1="8" x2="12" y2="16"></line>
			<line x1="8" y1="12" x2="16" y2="12"></line>
		</svg>
		拾取
	</button>
</div>

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
