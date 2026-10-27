import { withMermaid } from "vitepress-plugin-mermaid";
import { defineConfig } from 'vitepress'
import { tabsMarkdownPlugin } from 'vitepress-plugin-tabs'

// 站点布局为 fleet 约定：英文是默认 locale（root），中文镜像在 docs/zh/（路由 /zh/）。
// 两侧 nav/sidebar 由同一张 sections 表生成（zh/en 文案成对），避免中英镜像漂移。
// 中文工作文档（DEVELOPMENT / DEVELOPER_TOOLS / game-automation-coverage）只在 zh 侧成组。

type Entry = { zh: string; en: string; link: string }
type Section = { zh: string; en: string; items: Entry[] }

const sections: Section[] = [
  {
    zh: '开始', en: 'Getting Started', items: [
      { zh: '项目简介', en: 'Introduction', link: '/guide/introduction' },
      { zh: '快速开始', en: 'Quick Start', link: '/guide/getting-started' },
      { zh: '脚本开发指南', en: 'Script Development Guide', link: '/guide/script-development' },
      { zh: '架构设计', en: 'Architecture Design', link: '/guide/architecture' },
      { zh: 'VS Code 开发环境', en: 'VS Code Development Environment', link: '/development-environment' },
      { zh: '项目目录结构', en: 'Project Structure', link: '/guide/structure' }
    ]
  },
  {
    zh: '界面使用', en: 'Using the UI', items: [
      { zh: 'Dashboard 远程管理', en: 'Dashboard Remote Management', link: '/guide/dashboard' },
      { zh: 'Runtime 本地 GUI', en: 'Runtime Local GUI', link: '/guide/runtime-gui' }
    ]
  },
  {
    zh: '进阶指南', en: 'Advanced Guides', items: [
      { zh: 'UI Automation', en: 'UI Automation', link: '/guides/uia-guide' },
      { zh: 'YOLO 模型使用', en: 'Using YOLO Models', link: '/guides/yolo-guide' },
      { zh: '配置管理实践', en: 'Configuration Management in Practice', link: '/guides/configuration' },
      { zh: '数据库使用', en: 'Database Usage', link: '/guides/database' },
      { zh: '触发器系统', en: 'Trigger System', link: '/guides/triggers' },
      { zh: 'Android Agent 保活', en: 'Android Agent Keep-Alive', link: '/guides/android-keep-alive' },
      { zh: 'Android Agent 受限设置', en: 'Android Agent Restricted Settings', link: '/guides/android-restricted-settings' },
      { zh: '资产下发', en: 'Asset Sync', link: '/guides/asset-sync' }
    ]
  },
  {
    zh: '高级', en: 'Advanced', items: [
      { zh: '配置系统', en: 'Configuration System', link: '/guide/config' },
      { zh: '调试指南', en: 'Debugging Guide', link: '/guide/debugging' },
      { zh: '通信协议', en: 'Protocols', link: '/protocols' }
    ]
  },
  {
    zh: '参考', en: 'Reference', items: [
      { zh: '数据类型', en: 'Data Types', link: '/api/types' },
      { zh: '概览', en: 'Overview', link: '/api/' },
      { zh: '模块索引', en: 'Module Index', link: '/api/overview' }
    ]
  },
  {
    zh: '核心模块', en: 'Core Modules', items: [
      { zh: 'wingman.screen', en: 'wingman.screen', link: '/api/screen' },
      { zh: 'wingman.input', en: 'wingman.input', link: '/api/input' },
      { zh: 'wingman.window', en: 'wingman.window', link: '/api/window' },
      { zh: 'wingman.process', en: 'wingman.process', link: '/api/process' },
      { zh: 'wingman.system', en: 'wingman.system', link: '/api/system' }
    ]
  },
  {
    zh: 'UI Automation', en: 'UI Automation', items: [
      { zh: '概述', en: 'Overview', link: '/api/uia/' },
      { zh: 'Button 按钮', en: 'Button', link: '/api/uia/button' },
      { zh: 'Edit 编辑框', en: 'Edit', link: '/api/uia/edit' },
      { zh: 'Text 文本', en: 'Text', link: '/api/uia/text' },
      { zh: 'ComboBox 下拉框', en: 'ComboBox', link: '/api/uia/combobox' },
      { zh: 'List 列表', en: 'List', link: '/api/uia/list' },
      { zh: 'CheckBox 复选框', en: 'CheckBox', link: '/api/uia/checkbox' },
      { zh: 'RadioButton 单选按钮', en: 'RadioButton', link: '/api/uia/radiobutton' },
      { zh: 'Tab 标签页', en: 'Tab', link: '/api/uia/tab' },
      { zh: 'Menu 菜单', en: 'Menu', link: '/api/uia/menu' },
      { zh: 'Tree 树形控件', en: 'Tree', link: '/api/uia/tree' },
      { zh: 'Window 窗口', en: 'Window', link: '/api/uia/window' },
      { zh: 'ScrollBar 滚动条', en: 'ScrollBar', link: '/api/uia/scrollbar' },
      { zh: 'ProgressBar 进度条', en: 'ProgressBar', link: '/api/uia/progressbar' },
      { zh: 'Slider 滑块', en: 'Slider', link: '/api/uia/slider' },
      { zh: 'ToolTip 工具提示', en: 'ToolTip', link: '/api/uia/tooltip' }
    ]
  },
  {
    zh: '视觉与 AI', en: 'Vision & AI', items: [
      { zh: 'wingman.vision', en: 'wingman.vision', link: '/api/vision' },
      { zh: 'wingman.ocr', en: 'wingman.ocr', link: '/api/ocr' },
      { zh: 'wingman.ml', en: 'wingman.ml', link: '/api/ml' }
    ]
  },
  {
    zh: '自动化系统', en: 'Automation Systems', items: [
      { zh: 'wingman.event', en: 'wingman.event', link: '/api/event' },
      { zh: 'wingman.fsm', en: 'wingman.fsm', link: '/api/fsm' },
      { zh: 'wingman.task', en: 'wingman.task', link: '/api/task' },
      { zh: 'wingman.notify', en: 'wingman.notify', link: '/api/notify' },
      { zh: 'wingman.smarttrigger', en: 'wingman.smarttrigger', link: '/api/smart-trigger' },
      { zh: 'wingman.bt', en: 'wingman.bt', link: '/api/behavior-tree' },
      { zh: 'wingman.orchestration', en: 'wingman.orchestration', link: '/api/orchestration' }
    ]
  },
  {
    zh: '网络', en: 'Networking', items: [
      { zh: 'wingman.http', en: 'wingman.http', link: '/api/http' },
      { zh: 'wingman.transport', en: 'wingman.transport', link: '/api/transport' }
    ]
  },
  {
    zh: '序列化', en: 'Serialization', items: [
      { zh: 'wingman.json', en: 'wingman.json', link: '/api/json' },
      { zh: 'wingman.ini', en: 'wingman.ini', link: '/api/ini' }
    ]
  },
  {
    zh: '数据存储', en: 'Data Storage', items: [
      { zh: 'wingman.kv', en: 'wingman.kv', link: '/api/kv' },
      { zh: 'wingman.db', en: 'wingman.db', link: '/api/db' }
    ]
  },
  {
    zh: '辅助功能', en: 'Utilities', items: [
      { zh: 'wingman.human', en: 'wingman.human', link: '/api/human' },
      { zh: 'wingman.config', en: 'wingman.config', link: '/api/config' },
      { zh: 'wingman.verification', en: 'wingman.verification', link: '/api/verification' },
      { zh: 'wingman.security', en: 'wingman.security', link: '/api/security' },
      { zh: 'wingman.gameprofile', en: 'wingman.gameprofile', link: '/api/gameprofile' },
      { zh: 'wingman.debugger', en: 'wingman.debugger', link: '/api/debugger' },
      { zh: 'wingman.util', en: 'wingman.util', link: '/api/util' },
      { zh: 'wingman.timer', en: 'wingman.timer', link: '/api/timer' },
      { zh: 'wingman.perf', en: 'wingman.perf', link: '/api/perf' },
      { zh: 'wingman.clipboard', en: 'wingman.clipboard', link: '/api/clipboard' },
      { zh: 'wingman.crypto', en: 'wingman.crypto', link: '/api/crypto' },
      { zh: 'wingman.file', en: 'wingman.file', link: '/api/file' },
      { zh: 'wingman.filewatcher', en: 'wingman.filewatcher', link: '/api/filewatcher' },
      { zh: 'wingman.hotkey', en: 'wingman.hotkey', link: '/api/hotkey' },
      { zh: 'wingman.macro', en: 'wingman.macro', link: '/api/macro' },
      { zh: 'wingman.script', en: 'wingman.script', link: '/api/script' },
      { zh: 'wingman.systemwatch', en: 'wingman.systemwatch', link: '/api/systemwatch' },
      { zh: '核心 API', en: 'Core API', link: '/api/core' },
      { zh: '数据存储', en: 'Data Store', link: '/api/data' },
      { zh: '序列化', en: 'Serialization', link: '/api/serialize' }
    ]
  },
  {
    zh: '编排与协作', en: 'Orchestration & Teamwork', items: [
      { zh: 'wingman.inbox', en: 'wingman.inbox', link: '/api/inbox' },
      { zh: 'wingman.team', en: 'wingman.team', link: '/api/team' },
      { zh: 'wingman.node', en: 'wingman.node', link: '/guide/config' }
    ]
  },
  {
    zh: '示例脚本', en: 'Example Scripts', items: [
      { zh: 'Hello World', en: 'Hello World', link: '/examples/hello-world' },
      { zh: '像素检测', en: 'Pixel Detection', link: '/examples/pixel-detection' },
      { zh: '图像匹配', en: 'Image Matching', link: '/examples/image-matching' },
      { zh: '自动化循环', en: 'Automation Loop', link: '/examples/auto-loop' },
      { zh: '宏录制', en: 'Macro Recording', link: '/examples/macro-record' },
      { zh: 'UI 自动化', en: 'UI Automation', link: '/examples/ui-automation' }
    ]
  }
]

const tutorialsSection: Section = {
  zh: '视频教程', en: 'Video Tutorials', items: [
    { zh: '分镜脚本', en: 'Storyboard Script', link: '/tutorials/video-storyboard' },
    { zh: '字幕制作模板', en: 'Subtitle Template', link: '/tutorials/srt-template' }
  ]
}

const zhWorkingDocs: Entry[] = [
  { zh: '开发指南', en: 'Development Guide', link: '/zh/DEVELOPMENT' },
  { zh: '开发者工具', en: 'Developer Tools', link: '/zh/DEVELOPER_TOOLS' },
  { zh: '覆盖度分析', en: 'Coverage Analysis', link: '/zh/game-automation-coverage' }
]

const navFor = (en: boolean) => {
  const p = (link: string) => (en ? link : `/zh${link === '/' ? '' : link}`)
  return [
    { text: en ? 'Guide' : '指南', link: p('/guide/introduction') },
    { text: en ? 'Tutorials' : '教程', link: p('/tutorials/video-storyboard') },
    { text: 'API', link: p('/api/') },
    { text: en ? 'Examples' : '示例', link: p('/examples/') }
  ]
}

const sidebarFor = (en: boolean) => {
  const groups = [tutorialsSection, ...sections].map((s) => ({
    text: en ? s.en : s.zh,
    items: s.items.map((it) => ({ text: en ? it.en : it.zh, link: en ? it.link : `/zh${it.link}` }))
  }))
  if (!en) {
    groups.splice(1, 0, {
      text: '中文工作文档',
      items: zhWorkingDocs.map((it) => ({ text: it.zh, link: it.link }))
    } as Section)
  }
  return { '/': groups }
}

const rootThemeConfig = {
    nav: navFor(true),

    sidebar: sidebarFor(true),

    socialLinks: [
      { icon: 'github', link: 'https://github.com/cuihairu/wingman' }
    ],

    search: {
      provider: 'local'
    }
  };

const zhThemeConfig = {
    nav: [
      ...navFor(false),
      { text: '开发指南', link: '/zh/DEVELOPMENT' },
      { text: '覆盖度分析', link: '/zh/game-automation-coverage' }
    ],

    sidebar: sidebarFor(false),

    socialLinks: [
      { icon: 'github', link: 'https://github.com/cuihairu/wingman' }
    ],

    search: {
      provider: 'local'
    }
  };

export default withMermaid(defineConfig({
  title: 'Wingman',
  description: 'Programmable Control Engine for Game Automation',
  base: '/wingman/',
  ignoreDeadLinks: true,

  // 内部开发台账（逐批追加、翻译即过期）只作底稿，不进站点路由
  srcExclude: [
    'review/development-todo.md',
  ],

  locales: {
    root: {
      label: 'English',
      lang: 'en',
      themeConfig: rootThemeConfig
    },
    zh: {
      label: '简体中文',
      lang: 'zh-CN',
      themeConfig: zhThemeConfig
    }
  },

  markdown: {
    ignoreDeadLinks: true,
    config: (md) => {
      md.use(tabsMarkdownPlugin)
    },
    codeTransformers: [
      {
        postTransform: (code, node) => {
          if (node.props.lang === 'lua') {
            return code.replace(/:::shighlight\n/g, '')
          }
        }
      }
    ]
  }
}));
