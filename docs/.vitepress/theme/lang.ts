// 首访语言自动选择，全站单点（fleet 布局：root=en 英文首页，中文镜像 /zh/）。
// 优先级：localStorage 手动选择 > navigator.language > 保持当前 locale。
// 放在主题层而不是页内 <script setup>：跳转判定只有 root 页用得上，但点击
// 记账（在 /zh/ 页点 English 回英文）两侧都需要——拆两份页内脚本必漂移，
// 2026-10-18 那版 zh 页脚本「挂而不生效」正是这样过期的。
const KEY = 'wingman-lang-auto-picked'
const BASE = '/wingman/' // 与 config.mts 的 base 同值；客户端代码 import 不到 mts，按旧脚本口径硬编码
const ROOT_PATH = BASE
const ZH_PATH = BASE + 'zh/'

export function setupLangPick(): void {
  if (typeof window === 'undefined') return
  const p = window.location.pathname
  if (p === ROOT_PATH || p === ROOT_PATH.slice(0, -1)) {
    const picked = localStorage.getItem(KEY)
    if (picked === ZH_PATH || picked === ROOT_PATH) {
      // 手动选择优先：只在落点与选择不一致时跳
      if (picked !== p) window.location.replace(picked)
    } else {
      // 修复前的旧值（如已不存在的 index.en.html）直接清掉，走自动探测
      if (picked !== null) localStorage.removeItem(KEY)
      const langs = (navigator.languages?.length ? navigator.languages : [navigator.language]).map((l) =>
        l.toLowerCase()
      )
      if (langs.some((l) => l.startsWith('zh'))) window.location.replace(ZH_PATH)
    }
  }
  window.addEventListener(
    'click',
    (e) => {
      const a = (e.target as HTMLElement)?.closest?.('a') as HTMLAnchorElement | null
      const href = a?.getAttribute('href') || ''
      // 两种形态都认：构建后站内链接是 /wingman/zh/…，md 源里是相对 zh/…
      if (href.startsWith(ZH_PATH) || href.startsWith('zh/')) localStorage.setItem(KEY, ZH_PATH)
      else if (href === ROOT_PATH || href === ROOT_PATH.slice(0, -1)) localStorage.setItem(KEY, ROOT_PATH)
    },
    true
  )
}
