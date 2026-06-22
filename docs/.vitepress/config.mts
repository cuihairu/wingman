import { defineConfig } from 'vitepress'
import { tabsMarkdownPlugin } from 'vitepress-plugin-tabs'

export default defineConfig({
  title: 'Wingman',
  description: '游戏自动化可编程控制引擎',
  base: '/wingman/',
  ignoreDeadLinks: true,

  themeConfig: {
    nav: [
      { text: '指南', link: '/guide/introduction' },
      { text: 'API', link: '/api/' },
      { text: '示例', link: '/examples/' }
    ],

    sidebar: {
      '/guide/': [
        {
          text: '开始',
          items: [
            { text: '项目简介', link: '/guide/introduction' },
            { text: '快速开始', link: '/guide/getting-started' },
            { text: '脚本开发指南', link: '/guide/script-development' },
            { text: '架构设计', link: '/guide/architecture' },
            { text: 'VS Code 开发环境', link: '/development-environment' }
          ]
        },
        {
          text: '进阶指南',
          items: [
            { text: 'UI Automation', link: '/guides/uia-guide' },
            { text: 'YOLO 模型使用', link: '/guides/yolo-guide' }
          ]
        },
        {
          text: '高级',
          items: [
            { text: '配置系统', link: '/guide/config' },
            { text: '调试指南', link: '/guide/debugging' },
            { text: '通信协议', link: '/protocols' }
          ]
        }
      ],
      '/api/': [
        {
          text: '参考',
          items: [
            { text: '数据类型', link: '/api/types' },
            { text: '概览', link: '/api/' }
          ]
        },
        {
          text: '核心模块',
          items: [
            { text: 'wingman.screen', link: '/api/screen' },
            { text: 'wingman.input', link: '/api/input' },
            { text: 'wingman.window', link: '/api/window' },
            { text: 'wingman.process', link: '/api/process' },
            { text: 'wingman.system', link: '/api/system' }
          ]
        },
        {
          text: 'UI Automation',
          items: [
            { text: '概述', link: '/api/uia/' },
            { text: 'Button 按钮', link: '/api/uia/button' },
            { text: 'Edit 编辑框', link: '/api/uia/edit' },
            { text: 'Text 文本', link: '/api/uia/text' },
            { text: 'ComboBox 下拉框', link: '/api/uia/combobox' },
            { text: 'List 列表', link: '/api/uia/list' },
            { text: 'CheckBox 复选框', link: '/api/uia/checkbox' },
            { text: 'RadioButton 单选按钮', link: '/api/uia/radiobutton' },
            { text: 'Tab 标签页', link: '/api/uia/tab' },
            { text: 'Menu 菜单', link: '/api/uia/menu' },
            { text: 'Tree 树形控件', link: '/api/uia/tree' },
            { text: 'Window 窗口', link: '/api/uia/window' },
            { text: 'ScrollBar 滚动条', link: '/api/uia/scrollbar' },
            { text: 'ProgressBar 进度条', link: '/api/uia/progressbar' },
            { text: 'Slider 滑块', link: '/api/uia/slider' },
            { text: 'ToolTip 工具提示', link: '/api/uia/tooltip' }
          ]
        },
        {
          text: '视觉与 AI',
          items: [
            { text: 'wingman.vision', link: '/api/vision' },
            { text: 'wingman.ocr', link: '/api/ocr' },
            { text: 'wingman.ml', link: '/api/ml' }
          ]
        },
        {
          text: '自动化系统',
          items: [
            { text: 'wingman.event', link: '/api/event' },
            { text: 'wingman.fsm', link: '/api/fsm' },
            { text: 'wingman.task', link: '/api/task' },
            { text: 'wingman.notify', link: '/api/notify' },
            { text: 'wingman.smart-trigger', link: '/api/smart-trigger' },
            { text: 'wingman.behavior-tree', link: '/api/behavior-tree' },
            { text: 'wingman.orchestration', link: '/api/orchestration' }
          ]
        },
        {
          text: '网络',
          items: [
            { text: 'wingman.http', link: '/api/http' },
            { text: 'wingman.transport', link: '/api/transport' }
          ]
        },
        {
          text: '序列化',
          items: [
            { text: 'wingman.json', link: '/api/json' },
            { text: 'wingman.ini', link: '/api/ini' }
          ]
        },
        {
          text: '数据存储',
          items: [
            { text: 'wingman.kv', link: '/api/kv' },
            { text: 'wingman.db', link: '/api/db' }
          ]
        },
        {
          text: '辅助功能',
          items: [
            { text: 'wingman.human', link: '/api/human' },
            { text: 'wingman.config', link: '/api/config' },
            { text: 'wingman.verification', link: '/api/verification' },
            { text: 'wingman.security', link: '/api/security' },
            { text: 'wingman.gameprofile', link: '/api/gameprofile' },
            { text: 'wingman.debugger', link: '/api/debugger' },
            { text: 'wingman.util', link: '/api/util' },
            { text: 'wingman.perf', link: '/api/perf' },
            { text: '数据类型', link: '/api/types' }
          ]
        },
        {
          text: '编排与协作',
          items: [
            { text: 'wingman.inbox', link: '/api/inbox' },
            { text: 'wingman.team', link: '/api/team' },
            { text: 'wingman.node', link: '/api/orchestration' }
          ]
        }
      ],
      '/examples/': [
        {
          text: '示例脚本',
          items: [
            { text: 'Hello World', link: '/examples/hello-world' },
            { text: '像素检测', link: '/examples/pixel-detection' },
            { text: '图像匹配', link: '/examples/image-matching' },
            { text: '自动化循环', link: '/examples/auto-loop' },
            { text: '宏录制', link: '/examples/macro-record' },
            { text: 'UI 自动化', link: '/examples/ui-automation' }
          ]
        }
      ]
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/cuihairu/wingman' }
    ],

    search: {
      provider: 'local'
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
})
