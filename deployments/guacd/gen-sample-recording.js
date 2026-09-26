#!/usr/bin/env node
/**
 * 生成最小可回放的 Guacamole 会话录像样例（.mjs，写入 recordings/ 卷）。
 *
 * 录像格式 = 裸 Guacamole 协议指令流（服务端→客户端方向，无额外封装），
 * 帧以 `sync,<毫秒时间戳>;` 指令分帧（guacamole-common-js 1.5.0
 * SessionRecording 实测：非 sync 指令累积、sync 收口为一帧，回放位置
 * 以首帧时间戳为原点）。画面指令用 size/rect/cfill 画一块移动色带，
 * 足以肉眼验证播放/拖动进度。
 *
 * 用途：compose 栈的 recordings/ 目录挂给 guacd（/recordings），server
 * 侧 WINGMAN_RECORDING_DIR 指向同一目录经 /api/remote/recordings 列出；
 * 放一份样例让本地栈启动后回放面板即有真实条目可放（此前目录恒空，
 * 回放功能没有可验证数据）。浏览器内回放入口见 dashboard
 * RemoteRecordingPlayer（设计 §16「回放」）。
 *
 * 用法：node gen-sample-recording.js [--frames 12] [--gap 400] [--out recordings/sample-session.mjs]
 */
'use strict';

const fs = require('node:fs');
const path = require('node:path');

function parseArgs(argv) {
  const out = { frames: 12, gap: 400, out: path.join(__dirname, 'recordings', 'sample-session.mjs') };
  for (let i = 0; i < argv.length; i++) {
    const key = argv[i];
    if (key === '--frames') out.frames = parseInt(argv[++i], 10);
    else if (key === '--gap') out.gap = parseInt(argv[++i], 10);
    else if (key === '--out') out.out = path.resolve(argv[++i]);
    else {
      console.error('未知参数：%s（支持 --frames/--gap/--out）', key);
      process.exit(2);
    }
  }
  if (!(out.frames >= 1) || !(out.gap >= 1)) {
    console.error('--frames/--gap 必须为正整数');
    process.exit(2);
  }
  return out;
}

/** 一条 Guacamole 指令：元素按「长度.内容」编码，逗号分隔，分号收尾 */
function instruction(opcode, args) {
  const head = `${opcode.length}.${opcode}`;
  const tail = args.map((a) => `,${String(a).length}.${String(a)}`).join('');
  return `${head}${tail};`;
}

// 1280x720 画面：深色底 + 底部一条随帧移动的色带（色轮循环，肉眼可辨进度）
const WIDTH = 1280;
const HEIGHT = 720;
// 时间戳基线取非零：SessionRecording.isPlaying() 用 !!startVideoTimestamp
// 判真值，首帧 timestamp=0 会让 play() 后仍显示「未在播放」（真实录像的
// sync 时间戳是 guacd 侧 epoch 毫秒，天然非零；位置计算对基线不敏感——
// 回放位置统一减去首帧时间戳）
const TIMESTAMP_BASE = 1000;
const PALETTE = [
  [66, 133, 244],
  [219, 68, 55],
  [244, 180, 0],
  [15, 157, 88],
  [123, 31, 162],
  [0, 172, 193],
];

function frame(k, frames, gap) {
  const parts = [];
  if (k === 0) {
    parts.push(instruction('size', [0, WIDTH, HEIGHT]));
  }
  // 整幅重刷：rect 定路径，cfill 填色（mask 0xC = SRC，覆盖写）
  parts.push(instruction('rect', [0, 0, 0, WIDTH, HEIGHT]));
  parts.push(instruction('cfill', [0xc, 0, 0x1e, 0x24, 0x30, 0xff]));
  // 移动色带
  const bandWidth = 96;
  const x = Math.round((k / (frames - 1)) * (WIDTH - bandWidth - 8)) + 4;
  const [r, g, b] = PALETTE[k % PALETTE.length];
  parts.push(instruction('rect', [0, x, HEIGHT - 80, bandWidth, 60]));
  parts.push(instruction('cfill', [0xc, 0, r, g, b, 0xff]));
  // sync 收口：时间戳 = 基线 + 帧序 * 间隔（绝对毫秒，回放时相对首帧）
  parts.push(instruction('sync', [TIMESTAMP_BASE + k * gap]));
  return parts.join('');
}

function main() {
  const { frames, gap, out } = parseArgs(process.argv.slice(2));
  const body = Array.from({ length: frames }, (_, k) => frame(k, frames, gap)).join('');
  fs.mkdirSync(path.dirname(out), { recursive: true });
  fs.writeFileSync(out, body, 'utf8');
  const duration = ((frames - 1) * gap) / 1000;
  console.log('已生成 %s（%d 帧，时长 %s 秒，%d 字节）', out, frames, duration.toFixed(1), body.length);
}

main();
