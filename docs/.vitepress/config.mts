import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'Wingman',
  description: '游戏自动化可编程控制引擎',
  base: '/wingman/',

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
            { text: '远程控制', link: '/guide/remote' },
            { text: '调试指南', link: '/guide/debugging' },
            { text: '性能优化', link: '/guide/performance' }
          ]
        }
      ],
      '/api/': [
        {
          text: 'API 参考',
          items: [
            { text: 'wingman.screen', link: '/api/screen' },
            { text: 'wingman.input', link: '/api/input' },
            { text: 'wingman.window', link: '/api/window' },
            { text: 'wingman.process', link: '/api/process' },
            { text: 'wingman.human', link: '/api/human' },
            { text: 'wingman.macro', link: '/api/macro' },
            { text: 'wingman.trigger', link: '/api/trigger' },
            { text: 'wingman.util', link: '/api/util' },
            { text: 'wingman.remote', link: '/api/remote' },
            { text: 'wingman.debug', link: '/api/debug' }
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
            { text: '宏录制', link: '/examples/macro-record' }
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
