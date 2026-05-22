<script lang="ts">
	// 触发器页面 - 占位，Phase 6 实现
</script>

<div class="page-header">
	<h2 class="page-title">触发器</h2>
	<p class="page-subtitle">管理和配置自动化触发器</p>
</div>

<div class="card">
	<div class="card-header">
		<span class="card-title">触发器列表</span>
		<button class="btn btn-primary">添加触发器</button>
	</div>
	<div class="card-body">
		<div class="empty-state">
			<div class="empty-state-icon">⚡</div>
			<div>暂无触发器，点击"添加触发器"创建</div>
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

	.btn:hover { background: var(--border-color); }

	.btn-primary {
		background: var(--accent-green);
		color: white;
		border-color: var(--accent-green);
	}

	.btn-primary:hover { background: #2ea043; }

	.empty-state { text-align: center; padding: 60px 20px; color: var(--text-secondary); }
	.empty-state-icon { font-size: 48px; margin-bottom: 16px; opacity: 0.5; }
</style>
