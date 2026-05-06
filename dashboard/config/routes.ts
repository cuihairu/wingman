/**
 * @name umi 的路由配置
 * @description Wingman 游戏自动化控制引擎 - 路由配置
 * @doc https://umijs.org/docs/guides/routes
 */
export default [
  // 用户登录
  {
    path: '/user',
    layout: false,
    routes: [
      {
        name: 'login',
        path: '/user/login',
        component: './User/Login',
      },
    ],
  },
  // 主页
  {
    path: '/',
    component: './Welcome',
  },
  // Agent 管理
  {
    path: '/agents',
    name: 'Agents',
    icon: 'api',
    component: './Agents',
  },
  // 工作流管理
  {
    path: '/workflows',
    name: 'Workflows',
    icon: 'block',
    component: './Workflows',
  },
  // 用户中心
  {
    path: '/account',
    name: 'Account',
    icon: 'user',
    routes: [
      {
        path: '/account',
        redirect: '/account/center',
      },
      {
        path: '/account/center',
        name: 'Profile',
        component: './Profile',
      },
    ],
  },
  // 设置（可选）
  {
    path: '/settings',
    name: 'Settings',
    icon: 'setting',
    component: './Admin',
  },
  // 错误页面
  {
    path: '/403',
    layout: false,
    component: './403',
  },
  {
    path: '/404',
    layout: false,
    component: './404',
  },
  {
    path: '*',
    redirect: '/404',
  },
];
