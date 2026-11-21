// md → Vue SFC 插值泄漏守卫：任何 .md 中**未被保护**的 `{{` 会漏进编译后的
// Vue template，被当作插值表达式求值。fenced code block 与 :::v-pre 容器受保护
// （vitepress 对 fence 自动加 v-pre；v-pre 容器显式关编译）；inline code span
// （`...`）**不算**受保护——markdown-it 把它渲染成 <code>，而 <code> 不是 <pre>，
// Vue 编译器照样插值（2026-11-21 用 @vue/compiler-dom 实测：<pre> 与 <code> 内
// 的 {{ }} 都被编译成 _toDisplayString(...)）。
// 2026-11-15 的审计页截断正是这类泄漏：`${{ github.sha }}` 被插值 → SSR 抛
// TypeError（github undefined）→ 页面截断，而 vitepress 仍 exit 0，CI 未察觉。
import { readFileSync, readdirSync, statSync } from 'node:fs'
import { join, relative } from 'node:path'
import { fileURLToPath } from 'node:url'
import assert from 'node:assert/strict'

const DOCS = fileURLToPath(new URL('..', import.meta.url))

// 返回未受保护文本中裸 `{{` 的命中列表（{line, snippet}）。
function findRawMoustache(text) {
  const lines = text.split('\n')
  const hits = []
  let fence = null // null | { f: '```' | '~~~', start }
  let vpre = null // null | 起始行号
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i]
    const fenceMatch = line.match(/^\s*(```|~~~)/)
    if (fenceMatch) {
      if (!fence) fence = { f: fenceMatch[1], start: i + 1 }
      else if (fence.f === fenceMatch[1]) fence = null // 仅同字符算关闭
      continue // 开/闭行与 fence 内整段都受保护
    }
    if (fence) continue
    const vpreMatch = line.match(/^\s*:::\s*(v-pre)?\s*$/)
    if (vpreMatch) {
      if (vpreMatch[1] === 'v-pre') vpre = i + 1
      else if (vpre) vpre = null // 裸 ::: 关闭容器
      continue
    }
    if (vpre) continue // 容器内容整行受保护
    const idx = line.indexOf('{{')
    if (idx >= 0) hits.push({ line: i + 1, snippet: line.trim().slice(0, 120) })
  }
  return hits
}

// —— 负向对照：扫描器必须抓到裸 moustache、放过 fence/v-pre 内的 ——
{
  const bad = findRawMoustache('| a | 返回: {{x,y}} |\n')
  assert.equal(bad.length, 1, 'raw table-cell moustache 必须被抓到')
  const bad2 = findRawMoustache('该值由同一 `${{ github.sha }}` 派生\n')
  assert.equal(bad2.length, 1, '内联 code span 中的 moustache 也必须被抓到（code 非 pre）')
  const good = findRawMoustache('```lua\nlocal p = {{x,y}}\n```\n:::v-pre\n{{ raw }}\n:::\n')
  assert.equal(good.length, 0, 'fence 与 v-pre 容器内 moustache 必须放行')
  // 跨行 fence + 不同 fence 字符不误关
  const mixed = findRawMoustache('```\n~~~\n{{ in }} \n```\n{{ out }}\n')
  assert.equal(mixed.length, 1, '~~~ 不关闭 ```；容器外裸 moustache 被抓')
  assert.equal(mixed[0].line, 5, '命中行号正确')
}

// —— 全量扫描 docs/**/*.md（排除 node_modules/dist）——
const SKIP = new Set(['node_modules', '.vitepress', 'dist', '.git'])
const mds = []
function walk(dir) {
  for (const e of readdirSync(dir)) {
    if (SKIP.has(e)) continue
    const p = join(dir, e)
    if (statSync(p).isDirectory()) walk(p)
    else if (e.endsWith('.md')) mds.push(p)
  }
}
walk(DOCS)
mds.sort()

let total = 0
const allHits = []
for (const f of mds) {
  const hits = findRawMoustache(readFileSync(f, 'utf8'))
  if (hits.length) {
    allHits.push(...hits.map((h) => ({ file: relative(DOCS, f), ...h })))
    total += hits.length
  }
}

if (total > 0) {
  for (const h of allHits) console.error(`${h.file}:${h.line}: ${h.snippet}`)
  console.error(`\nmd-vue-interpolation: ${total} raw {{ found in ${allHits.length} line(s) — 必须修`)
  process.exit(1)
}

console.log(`md-vue-interpolation: ${mds.length} .md scanned, 0 raw {{ outside fences/v-pre`)
