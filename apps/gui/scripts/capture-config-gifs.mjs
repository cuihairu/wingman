// capture-config-gifs.mjs — 配置开关 off/on 截图与切换 GIF 流水线
//
// 用途：渲染 apps/gui 设置页的三个布尔配置开关，逐一采集 off/on 两态截图与
// 切换循环 GIF，落 docs/public/config-effects/，供 docs「配置开关效果」双语页嵌入。
// 可重跑兼作回归：重新执行即按固定视口/固定初始态重新生成，设置页布局或开关
// 语义变化会体现在产物 diff 上。
//
// 前置：
//   1. 起 GUI dev server：cd apps/gui && npm run dev（默认 http://localhost:5173）
//   2. playwright-core：cd apps/gui && npm i -D playwright-core
//   3. 本地 chromium：默认取 ~/.cache/ms-playwright/chromium-*/chrome-linux64/chrome，
//      可用 CHROME_PATH 覆盖；需 headless 支持（--no-sandbox）。
//   4. 合成 GIF：ImageMagick `magick` 或 ffmpeg（自动回退）。
//
// 环境变量：GUI_DEV_URL（默认 http://localhost:5173）、OUT_DIR（默认仓库 docs/public/config-effects）、
//          CHROME_PATH、MAGICK_BIN、FFMPEG_BIN、DWELL_MS（每态停留毫秒，默认 1000）。
import { createRequire } from 'node:module';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { execFileSync } from 'node:child_process';

// 解析 playwright-core：脚本目录向上 node_modules 查找；找不到给出安装指引。
let chromium;
try {
  ({ chromium } = createRequire(import.meta.url)('playwright-core'));
} catch (e) {
  console.error('[capture-config-gifs] 未找到 playwright-core，请先执行：cd apps/gui && npm i -D playwright-core');
  process.exit(2);
}

// 开关清单：与 src/routes/settings/+page.svelte 的三个布尔设置一一对应。
const SWITCHES = [
  { key: 'auto-reconnect', label: '自动重连', setting: 'autoReconnect' },
  { key: 'auto-start', label: '启动后自动加载当前配置脚本', setting: 'autoStart' },
  { key: 'minimize-to-tray', label: '启动时最小化到托盘', setting: 'minimizeOnStart' },
];

const BASE = process.env.GUI_DEV_URL || 'http://localhost:5173';
const OUT_DIR = path.resolve(process.env.OUT_DIR || path.join(import.meta.dirname, '..', '..', '..', 'docs', 'public', 'config-effects'));
const DWELL_MS = Number(process.env.DWELL_MS || 1000);

function findChrome() {
  if (process.env.CHROME_PATH) return process.env.CHROME_PATH;
  const cache = path.join(os.homedir(), '.cache', 'ms-playwright');
  const dirs = fs.readdirSync(cache).filter((d) => d.startsWith('chromium-'));
  for (const d of dirs.sort().reverse()) {
    const p = path.join(cache, d, 'chrome-linux64', 'chrome');
    if (fs.existsSync(p)) return p;
  }
  return null;
}

function gifBin() {
  const magick = process.env.MAGICK_BIN || 'magick';
  const ffmpeg = process.env.FFMPEG_BIN || 'ffmpeg';
  try { execFileSync(magick, ['-version'], { stdio: 'ignore' }); return { bin: magick, kind: 'magick' }; } catch {}
  try { execFileSync(ffmpeg, ['-version'], { stdio: 'ignore' }); return { bin: ffmpeg, kind: 'ffmpeg' }; } catch {}
  return null;
}

function makeGif(gif, off, on, bin) {
  const hold = String(Math.round(DWELL_MS / 10)); // ImageMagick delay 单位=1/100s
  const frames = [off, on, off, on]; // off→on 往返循环，突出切换
  if (bin.kind === 'magick') {
    const args = frames.flatMap((f) => ['-delay', hold, f]).concat(['-loop', '0', gif]);
    execFileSync(bin.bin, args, { stdio: 'ignore' });
  } else {
    const inputs = [];
    for (let i = 0; i < frames.length; i++) inputs.push('-loop', '1', '-t', String(DWELL_MS / 1000), frames[i]);
    const fc = frames.map((_, i) => `[${i}]`).join('');
    execFileSync(bin.bin, [
      '-y', ...inputs,
      '-filter_complex', `${fc}concat=n=${frames.length}:v=1:a=0,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse`,
      '-loop', '0', gif,
    ], { stdio: 'ignore' });
  }
}

async function main() {
  const chrome = findChrome();
  if (!chrome) { console.error('[capture-config-gifs] 未找到本地 chromium（~/.cache/ms-playwright）'); process.exit(3); }
  const gif = gifBin();
  if (!gif) { console.error('[capture-config-gifs] 未找到 magick 或 ffmpeg'); process.exit(4); }

  fs.mkdirSync(OUT_DIR, { recursive: true });
  const browser = await chromium.launch({ executablePath: chrome, headless: true, args: ['--no-sandbox'] });
  const page = await browser.newPage({ viewport: { width: 1200, height: 900 } });

  for (const sw of SWITCHES) {
    // 确定性初始态：全部布尔关，重载（Svelte store 从 localStorage 初始化）。
    await page.goto(BASE + '/#/settings', { waitUntil: 'domcontentloaded' });
    await page.evaluate(() => {
      localStorage.setItem('wingman-settings', JSON.stringify({
        ipcEndpoint: 'wingman', orchestratorUrl: 'http://localhost:9527',
        autoReconnect: false, autoStart: false, minimizeOnStart: false,
        logLevel: 'info', theme: 'dark',
      }));
    });
    await page.reload({ waitUntil: 'networkidle' });

    const xpath = `//div[contains(@class,'form-group')][.//span[contains(@class,'form-label')][normalize-space()='${sw.label}']]`;
    const row = page.locator(`xpath=${xpath}`);
    await row.waitFor({ state: 'visible', timeout: 10000 });
    await row.scrollIntoViewIfNeeded();

    const offPng = path.join(OUT_DIR, `${sw.key}-off.png`);
    const onPng = path.join(OUT_DIR, `${sw.key}-on.png`);
    const gifPng = path.join(OUT_DIR, `${sw.key}.gif`);

    await row.screenshot({ path: offPng });

    const cb = row.locator('input[type="checkbox"]');
    const before = await cb.isChecked();
    await cb.click(); // 直接点 checkbox，命中开关本体
    await page.waitForTimeout(400); // Svelte 响应 + 重绘
    const after = await cb.isChecked();
    if (before === after) {
      throw new Error(`${sw.key}: 点击后开关状态未翻转 (before=${before}, after=${after})`);
    }
    await row.screenshot({ path: onPng });

    makeGif(gifPng, offPng, onPng, gif);
    console.log(`[capture-config-gifs] ${sw.key}: off/on/gif -> ${path.basename(offPng)} / ${path.basename(onPng)} / ${path.basename(gifPng)}`);
  }

  await browser.close();
  console.log(`[capture-config-gifs] 完成，输出目录：${OUT_DIR}`);
}

main().catch((e) => { console.error('[capture-config-gifs] 失败：', e); process.exit(1); });
