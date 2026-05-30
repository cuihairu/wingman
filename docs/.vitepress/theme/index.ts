import type { Theme } from 'vitepress'
import DefaultTheme from 'vitepress/theme'
import CodeTabs from './CodeTabs.vue'

export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component('CodeTabs', CodeTabs)
  }
} satisfies Theme
