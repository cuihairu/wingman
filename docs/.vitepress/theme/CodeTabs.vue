<script setup lang="ts">
import { useData } from 'vitepress'

// VitePress built-in code groups are styled automatically
// This component is just a placeholder/wrapper if needed
</script>

<template>
  <div class="wingman-code-tabs">
    <slot />
  </div>
</template>

<style scoped>
.wingman-code-tabs {
  margin: 1em 0;
}
</style>
