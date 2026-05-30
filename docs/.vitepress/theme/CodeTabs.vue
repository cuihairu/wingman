<script setup lang="ts">
import { ref, computed } from 'vue'

const props = defineProps<{
  active?: string
}>()

const activeLang = ref<'python' | 'lua'>(props.active || 'python')

function setLang(lang: 'python' | 'lua') {
  activeLang.value = lang
}
</script>

<template>
  <div class="code-tabs">
    <div class="code-tabs-header">
      <button
        :class="['tab-button', { active: activeLang === 'python' }]"
        @click="setLang('python')"
      >
        Python
      </button>
      <button
        :class="['tab-button', { active: activeLang === 'lua' }]"
        @click="setLang('lua')"
      >
        Lua
      </button>
    </div>
    <div class="code-tabs-content">
      <div v-if="activeLang === 'python'" class="tab-content">
        <slot name="python"></slot>
      </div>
      <div v-else-if="activeLang === 'lua'" class="tab-content">
        <slot name="lua"></slot>
      </div>
    </div>
  </div>
</template>

<style scoped>
.code-tabs {
  margin: 1em 0;
  border: 1px solid var(--vp-c-border);
  border-radius: 8px;
  overflow: hidden;
}

.code-tabs-header {
  display: flex;
  border-bottom: 1px solid var(--vp-c-border);
  background: var(--vp-c-bg-soft);
}

.tab-button {
  padding: 8px 16px;
  border: none;
  background: transparent;
  color: var(--vp-c-text-2);
  cursor: pointer;
  transition: all 0.2s;
  font-size: 14px;
  font-weight: 500;
}

.tab-button:hover {
  color: var(--vp-c-text-1);
  background: var(--vp-c-bg-soft);
}

.tab-button.active {
  color: var(--vp-c-brand-1);
  background: var(--vp-c-bg);
  border-bottom: 2px solid var(--vp-c-brand-1);
}

.code-tabs-content {
  margin: 0;
}

.tab-content {
  margin: 0;
}

.tab-content :deep(pre) {
  margin: 0 !important;
  border-radius: 0 !important;
}

.tab-content :deep(code) {
  display: block;
  padding: 16px;
  overflow-x: auto;
}
</style>
