import component from './zh-CN/component';
import globalHeader from './zh-CN/globalHeader';
import menu from './zh-CN/menu';
import pages from './zh-CN/pages';
import pwa from './zh-CN/pwa';
import settingDrawer from './zh-CN/settingDrawer';
import settings from './zh-CN/settings';

export default {
  'navBar.lang': '语言',
  'layout.user.link.help': '帮助',
  'layout.user.link.privacy': '隐私',
  'layout.user.link.terms': '条款',
  'app.preview.down.block': '下载此页面到本地项目',
  'app.welcome.link.fetch-blocks': '获取全部区块',
  'app.welcome.link.block-list': '基于 block 开发，快速构建标准页面',
  ...pages,
  ...globalHeader,
  ...menu,
  ...settingDrawer,
  ...settings,
  ...pwa,
  ...component,
  'pages.layouts.userLayout.title': 'Wingman 编排、自动化与审计控制台',
  'pages.login.accountLogin.errorMessage': '错误的用户名或密码',
  'pages.login.username.placeholder': '用户名',
  'pages.login.password.placeholder': '密码',
  'profile.stats.games': '作用域',
  'profile.games.title': '作用域访问',
  'profile.games.empty': '当前账户没有可显示的作用域信息',
  'profile.games.envs': '执行环境',
  'profile.games.permissions': '授权动作',
  'profile.permissions.fallback.scripts.manage.name': '脚本编辑权限',
  'profile.permissions.fallback.scripts.manage.description': '允许创建、编辑和保存自动化脚本',
  'profile.permissions.fallback.scripts.run.name': '脚本运行权限',
  'profile.permissions.fallback.scripts.run.description': '允许运行和停止自动化脚本',
  'profile.permissions.fallback.workflows.manage.name': '工作流管理权限',
  'profile.permissions.fallback.workflows.manage.description': '允许创建、更新和取消工作流执行',
  'profile.permissions.fallback.agents.manage.name': '代理管理权限',
  'profile.permissions.fallback.agents.manage.description': '允许查看代理详情并控制代理生命周期',
  'profile.permissions.fallback.settings.manage.name': '系统设置权限',
  'profile.permissions.fallback.settings.manage.description': '允许更新系统设置和运行开关',
};
