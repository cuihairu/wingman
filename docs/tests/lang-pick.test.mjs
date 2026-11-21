// lang.ts（首访语言自动选择）行为验证：esbuild 把 TS 编成 CJS 后，
// 桩掉 window/localStorage/navigator 逐个用例断言跳转与 localStorage 记账。
// 覆盖：zh 浏览器首访 root→跳 /zh/；en 浏览器留 root；手动选择优先（双向）；
// 旧值 index.en.html 清理后回落自动探测；root 以外路径不跳转；点击 zh/英文
// 链接记账（站内链接 /wingman/zh/… 与 md 相对 zh/ 两种形态）。
import assert from 'node:assert/strict'
import { build } from 'esbuild'

const out = await build({
  entryPoints: ['.vitepress/theme/lang.ts'],
  bundle: true,
  format: 'cjs',
  platform: 'node',
  write: false
})
const { text: code } = out.outputFiles[0]
// esbuild 的 CJS 命名导出只写 `0 && (module.exports = {...})` 注释（不赋值），
// 测试侧显式把具名函数挂到 exports 上。
const bundle = code + '\nexports.setupLangPick = setupLangPick'

function makeEnv({ pathname, picked = null, lang = 'en-US', langs = null } = {}) {
  const calls = { replace: [], keys: [], removed: [] }
  const storage = {
    _v: picked,
    // 逗号运算符：push 返回值是数组长度，直接 || 会把 picked 值吃掉
    getItem: (k) => (k === 'wingman-lang-auto-picked' ? ((calls.keys.push(k), storage._v)) : null),
    setItem: (k, v) => {
      calls.keys.push(k)
      storage._v = v
    },
    removeItem: (k) => {
      calls.removed.push(k)
      storage._v = null
    }
  }
  const env = {
    window: {
      location: { pathname, replace: (u) => calls.replace.push(u) },
      addEventListener: (t, fn) => {
        env._click = fn
      }
    },
    localStorage: storage,
    navigator: langs ? { languages: langs } : { language: lang }
  }
  return { calls, env }
}

function run(env) {
  const mod = { exports: {} }
  const fn = new Function('exports', 'require', 'module', 'window', 'localStorage', 'navigator', 'document', 'console', bundle)
  fn(mod.exports, () => {}, mod, env.window, env.localStorage, env.navigator, undefined, console)
  assert.equal(typeof mod.exports.setupLangPick, 'function')
  mod.exports.setupLangPick()
  if (process.env.LANG_DEBUG)
    console.log(`run#${++runs} p=${env.window?.location?.pathname} picked=${env.localStorage._v}`)
}
let runs = 0

// 1. zh 浏览器首访 root（/wingman/）→ 跳 /wingman/zh/
{
  const { env, calls } = makeEnv({ pathname: '/wingman/', langs: ['zh-CN', 'en'] })
  run(env)
  assert.deepEqual(calls.replace, ['/wingman/zh/'])
  assert.equal(env.localStorage._v, null, 'auto-pick 不写 localStorage')
}

// 2. zh 浏览器首访 root 无尾斜杠（/wingman）→ 同样跳转
{
  const { env, calls } = makeEnv({ pathname: '/wingman', lang: 'zh-TW' })
  run(env)
  assert.deepEqual(calls.replace, ['/wingman/zh/'])
}

// 3. en 浏览器首访 root → 不跳
{
  const { env, calls } = makeEnv({ pathname: '/wingman/', lang: 'en-US' })
  run(env)
  assert.deepEqual(calls.replace, [])
}

// 4. 手动选择 zh 后回 root → 跳 /zh/（且不再自动探测 en）
{
  const { env, calls } = makeEnv({ pathname: '/wingman/', picked: '/wingman/zh/', lang: 'en-US' })
  run(env)
  assert.deepEqual(calls.replace, ['/wingman/zh/'])
}

// 5. 手动选择英文后回 root → 不跳
{
  const { env, calls } = makeEnv({ pathname: '/wingman/', picked: '/wingman/', lang: 'zh-CN' })
  run(env)
  assert.deepEqual(calls.replace, [])
}

// 6. 旧值（index.en.html 时代）→ 清掉并按浏览器语言自动探测
{
  const { env, calls } = makeEnv({ pathname: '/wingman/', picked: '/wingman/index.en.html', lang: 'zh-CN' })
  run(env)
  assert.ok(calls.removed.includes('wingman-lang-auto-picked'), 'stale 值应被清除')
  assert.deepEqual(calls.replace, ['/wingman/zh/'])
}

// 7. 非 root 路径不跳转（如 /wingman/zh/ 已落地、或子页）
{
  for (const p of ['/wingman/zh/', '/wingman/zh/guide/getting-started', '/wingman/guide/getting-started']) {
    const { env, calls } = makeEnv({ pathname: p, lang: 'zh-CN' })
    run(env)
    assert.deepEqual(calls.replace, [], `pathname=${p} 不应跳转`)
  }
}

// 8. 点击记账：站内 /wingman/zh/… 与 md 相对 zh/ 两种形态 → KEY=/wingman/zh/
{
  const { env } = makeEnv({ pathname: '/wingman/zh/', picked: '/wingman/', lang: 'en-US' })
  run(env)
  const click = env._click
  for (const href of ['/wingman/zh/guide/getting-started', 'zh/index.md']) {
    env.localStorage._v = '/wingman/'
    click({ target: { closest: () => ({ getAttribute: (k) => (k === 'href' ? href : null) }) } })
    assert.equal(env.localStorage._v, '/wingman/zh/', `href=${href} 应记 zh`)
  }
}

// 9. 点击英文链接 → KEY=/wingman/（含尾斜杠与去尾斜杠两种写法）
{
  const { env } = makeEnv({ pathname: '/wingman/zh/', picked: '/wingman/zh/', lang: 'zh-CN' })
  run(env)
  const click = env._click
  for (const href of ['/wingman/', '/wingman']) {
    env.localStorage._v = '/wingman/zh/'
    click({ target: { closest: () => ({ getAttribute: (k) => (k === 'href' ? href : null) }) } })
    assert.equal(env.localStorage._v, '/wingman/', `href=${href} 应记英文`)
  }
}

// 10. SSR / 无 window 时静默返回
{
  const { calls } = makeEnv({ pathname: '/wingman/' })
  assert.doesNotThrow(() => run({ window: undefined, localStorage: calls, navigator: {} }), 'no window must not throw')
}

console.log('lang-pick: 10 cases OK')
