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
      { text: '示例', link: '/examples/' },
      { text: 'GitHub', link: 'https://github.com/cuihairu/wingman' }
    ],

    sidebar: {
      '/guide/': [
        {
          text: '开始',
          items: [
            { text: '项目简介', link: '/guide/introduction' },
            { text: '快速开始', link: '/guide/getting-started' },
            { text: '架构设计', link: '/guide/architecture' }
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
          text: '功能',
          items: [
            { text: '屏幕操作', link: '/guide/screen' },
            { text: '输入模拟', link: '/guide/input' },
            { text: '窗口管理', link: '/guide/window' },
            { text: '进程管理', link: '/guide/process' },
            { text: '宏录制', link: '/guide/macro' },
            { text: '触发器', link: '/guide/trigger' },
            { text: '人性化模拟', link: '/guide/human' }
          ]
        },
        {
          text: '高级',
          items: [
            { text: '配置系统', link: '/guide/config' },
            { text: '远程控制', link: '/guide/remote' },
            { text: '调试指南', link: '/guide/debugging' },
            { text: '性能优化', link: '/guide/performance' }
          ]
        }
      ],
      '/api/': [
        {
          text: '核心模块',
          items: [
            { text: 'wingman.screen', link: '/api/screen' },
            { text: 'wingman.input', link: '/api/input' },
            { text: 'wingman.window', link: '/api/window' },
            { text: 'wingman.process', link: '/api/process' },
            { text: 'wingman.uia', link: '/api/uia' }
          ]
        },
        {
          text: '视觉与 AI',
          items: [
            { text: 'wingman.vision', link: '/api/vision' },
            { text: 'wingman.ocr', link: '/api/ocr' }
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
            { text: 'wingman.behavior-tree', link: '/api/behavior-tree' }
          ]
        },
        {
          text: '网络与数据',
          items: [
            { text: 'wingman.http', link: '/api/http' },
            { text: 'wingman.json', link: '/api/json' },
            { text: 'wingman.kv', link: '/api/kv' }
          ]
        },
        {
          text: '辅助功能',
          items: [
            { text: 'wingman.human', link: '/api/human' },
            { text: 'wingman.config', link: '/api/config' },
            { text: 'wingman.verification', link: '/api/verification' },
            { text: 'wingman.gameprofile', link: '/api/gameprofile' },
            { text: 'wingman.debugger', link: '/api/debugger' },
            { text: 'wingman.util', link: '/api/util' },
            { text: 'wingman.tray', link: '/api/tray' }
          ]
        },
        {
          text: '高级功能',
          items: [
            { text: 'wingman.remote', link: '/api/remote' },
            { text: 'wingman.team', link: '/api/team' }
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
