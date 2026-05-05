import { defineConfig } from 'umi';

export default defineConfig({
  nodeModulesTransform: {
    type: 'none',
  },
  routes: [
    { path: '/login', component: '@/pages/login', layout: false },
    {
      path: '/',
      component: '@/layouts/index',
      routes: [
        { path: '/', component: '@/pages/index' },
        { path: '/scripts', component: '@/pages/scripts' },
        { path: '/windows', component: '@/pages/windows' },
        { path: '/settings', component: '@/pages/settings' },
      ],
    },
  ],
  fastRefresh: {},
  dva: {},
  antd: {},
  locale: {
    default: 'zh-CN',
    antd: true,
    title: false,
    baseNavigator: true,
  },
  targets: {
    ie: 11,
  },
  // 代理配置，连接到 Wingman 服务器
  proxy: {
    '/api': {
      target: 'http://localhost:8080',
      changeOrigin: true,
    },
  },
});
