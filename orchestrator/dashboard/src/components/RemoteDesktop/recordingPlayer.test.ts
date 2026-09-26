/**
 * 录像回放封装测试（设计 §16「回放」）。
 *
 * 与仓库其他 Guacamole 测试不同：这里不 mock guacamole-common-js，用
 * 真实 SessionRecording 解析 deployments/guacd/recordings/sample-session.mjs
 * （gen-sample-recording.js 产物；录像 = 裸指令流、sync 分帧的格式契约
 * mock 不出来，必须真解析）。jsdom 没有 canvas 2d 实现（getContext 返回
 * null，Display/Layer 构造即崩），打一个 no-op 2d context 桩——只服务
 * size/rect/cfill/sync 这条指令路径，不做像素断言（像素正确性是浏览器
 * canvas 的事，不是本封装的）。
 */
import fs from 'node:fs';
import path from 'node:path';
import Guacamole from 'guacamole-common-js';
import { BlobRecordingTunnel, createRecordingPlayer, formatRecordingTime } from './recordingPlayer';
import type { RecordingPlayerEvents } from './recordingPlayer';

/** 样例录像：仓库根 deployments/guacd/recordings/（12 帧 × 400ms，1280x720） */
const SAMPLE_PATH = path.resolve(
  __dirname,
  '../../../../../deployments/guacd/recordings/sample-session.mjs',
);

/** no-op 2d context 桩：方法全部吞掉，属性可写（fillStyle 等） */
function makeContextStub(canvas: HTMLCanvasElement): CanvasRenderingContext2D {
  // Proxy trap 的 prop 是 string|symbol，目标索引用 PropertyKey 才装得下
  const target: Record<PropertyKey, unknown> = { canvas };
  return new Proxy(target, {
    get(t, prop) {
      if (prop in t) {
        return t[prop];
      }
      if (prop === 'measureText') {
        return () => ({ width: 0 });
      }
      if (
        prop === 'createLinearGradient' ||
        prop === 'createRadialGradient' ||
        prop === 'createPattern'
      ) {
        return () => ({ addColorStop: () => undefined });
      }
      if (prop === 'getImageData') {
        return () => ({ data: new Uint8ClampedArray(4) });
      }
      return () => undefined;
    },
    set(t, prop, value) {
      t[prop] = value;
      return true;
    },
  }) as unknown as CanvasRenderingContext2D;
}

beforeAll(() => {
  const original = HTMLCanvasElement.prototype.getContext;
  HTMLCanvasElement.prototype.getContext = function contextStub(this: HTMLCanvasElement) {
    return makeContextStub(this);
  } as unknown as typeof HTMLCanvasElement.prototype.getContext;
  (HTMLCanvasElement.prototype.getContext as unknown as { __original: unknown }).__original =
    original;
  // exportState 走 toDataURL 序列化画面，jsdom 未实现会刷 console 噪音
  const originalToDataURL = HTMLCanvasElement.prototype.toDataURL;
  HTMLCanvasElement.prototype.toDataURL = () => 'data:,';
  (HTMLCanvasElement.prototype as unknown as { __originalToDataURL: unknown }).__originalToDataURL =
    originalToDataURL;
});

afterAll(() => {
  HTMLCanvasElement.prototype.getContext = (
    HTMLCanvasElement.prototype.getContext as unknown as {
      __original: () => unknown;
    }
  ).__original as typeof HTMLCanvasElement.prototype.getContext;
  HTMLCanvasElement.prototype.toDataURL = (
    HTMLCanvasElement.prototype as unknown as {
      __originalToDataURL: () => string;
    }
  ).__originalToDataURL;
});

function sampleBlob(): Blob {
  return new Blob([fs.readFileSync(SAMPLE_PATH, 'utf8')], { type: 'text/plain' });
}

/** 真实定时器轮询（FileReader/解析回调走宏任务，不能同步等） */
async function until(cond: () => boolean, timeoutMs = 3000): Promise<void> {
  const start = Date.now();
  while (!cond()) {
    if (Date.now() - start > timeoutMs) {
      throw new Error('等待条件超时');
    }
    await new Promise((r) => {
      setTimeout(r, 10);
    });
  }
}

/**
 * fake 定时器弃用说明：jsdom 的 FileReader 在 jest 假时钟下不完成
 * （帧重放走 FileReader 异步读），播放状态机测试改用真实定时器——
 * 帧距 400ms 是真实等待，用 until 条件收敛，不做整段时长假设。
 */
interface PlayerHarness {
  player: ReturnType<typeof createRecordingPlayer>;
  onReady: jest.Mock;
  onError: jest.Mock;
  onPosition: jest.Mock;
  onPlayingChange: jest.Mock;
}

function makePlayer(blob: Blob, events: RecordingPlayerEvents = {}): PlayerHarness {
  const onReady = jest.fn();
  const onError = jest.fn();
  const onPosition = jest.fn();
  const onPlayingChange = jest.fn();
  const player = createRecordingPlayer(blob, {
    onReady,
    onError,
    onPosition,
    onPlayingChange,
    ...events,
  });
  return { player, onReady, onError, onPosition, onPlayingChange };
}

describe('样例录像解析（真实 SessionRecording）', () => {
  it('解析出 12 帧 / 4400ms 时长 / 1280x720 画面，全程无错误', async () => {
    const { player, onReady, onError, onPosition } = makePlayer(sampleBlob());
    await until(() => onReady.mock.calls.length > 0);
    expect(onReady).toHaveBeenCalledWith(4400);
    expect(onError).not.toHaveBeenCalled();
    expect(player.getDuration()).toBe(4400);
    // 注意：解析期只建帧表不驱动画面（size 指令要等首轮重放经 Client
    // 才落显示任务队列），displaySize 断言放在 seek 测试里
    // onprogress（封装不透出）之外的 onPosition 在解析期不触发——位置只
    // 由播放/seek 推进
    expect(onPosition).not.toHaveBeenCalled();
    expect(player.getPosition()).toBe(0);
    expect(player.isPlaying()).toBe(false);
    // 画面元素已就绪（Display 的 canvas 容器 div）
    expect(player.element.tagName).toBe('DIV');
    expect(player.element.querySelectorAll('canvas').length).toBeGreaterThan(0);
  });

  it('解析期 onprogress 每个 sync 触发一次（12 帧）', async () => {
    // onprogress 挂在 SessionRecording 上（每 sync 一次），封装事件里
    // 没有它——直接在底层实例上验证帧收口行为（经隧道适配器喂源）
    const recording = new Guacamole.SessionRecording(new BlobRecordingTunnel(sampleBlob()));
    const onprogress = jest.fn();
    recording.onprogress = onprogress;
    recording.connect();
    await until(() => recording.getDuration() === 4400);
    expect(onprogress).toHaveBeenCalledTimes(12);
    expect(onprogress).toHaveBeenLastCalledWith(4400, expect.any(Number));
  });

  it('1.5.0 Blob 直连分支是坏的（上游缺 recordingBlob 赋值）：构造即抛', () => {
    // 锁住绕行理由：谁要是删了隧道适配器改回 Blob 直连，这条用例会红
    expect(() => {
      const recording = new Guacamole.SessionRecording(sampleBlob());
      recording.abort();
    }).toThrow();
  });
});

describe('非法录像', () => {
  it('非指令流 → onError 且不 ready，位置/时长保持 0', async () => {
    const { player, onReady, onError } = makePlayer(new Blob(['x.abc;']));
    await until(() => onError.mock.calls.length > 0);
    expect(onReady).not.toHaveBeenCalled();
    expect(onError).toHaveBeenCalledWith('Non-numeric character in element length.');
    expect(player.getDuration()).toBe(0);
    expect(player.getPosition()).toBe(0);
  });
});

describe('播放状态机（真实时钟）', () => {
  /** 解析完成的播放器 */
  async function makeReadyPlayer(): Promise<PlayerHarness> {
    const h = makePlayer(sampleBlob());
    await until(() => h.onReady.mock.calls.length > 0);
    return h;
  }

  it('play 推进位置（400ms 帧距），pause 停住，播到结尾自动暂停', async () => {
    const { player, onPlayingChange } = await makeReadyPlayer();
    player.play();
    expect(player.isPlaying()).toBe(true);
    expect(onPlayingChange).toHaveBeenCalledWith(true);

    // 泵到第 2 帧（>400ms）
    await until(() => player.getPosition() >= 400);
    const at = player.getPosition();
    player.pause();
    expect(player.isPlaying()).toBe(false);
    expect(onPlayingChange).toHaveBeenLastCalledWith(false);

    // 暂停后时钟流逝位置不变（帧调度已停）
    await new Promise((r) => {
      setTimeout(r, 700);
    });
    expect(player.getPosition()).toBe(at);

    // 继续播放直到结尾：自动暂停（最后一帧 4400）
    player.play();
    await until(() => !player.isPlaying(), 10_000);
    expect(player.getPosition()).toBe(4400);
    expect(onPlayingChange).toHaveBeenLastCalledWith(false);
    // 结尾后再 play 无帧可放，不复活
    player.play();
    expect(player.isPlaying()).toBe(false);
  }, 15_000);

  it('seek 落到最近帧；越界收敛到 [0, 时长]；播放中 seek 后继续播', async () => {
    const { player } = await makeReadyPlayer();
    player.seek(2800);
    await until(() => player.getPosition() === 2800);
    expect(player.isPlaying()).toBe(false);
    // 首轮重放把 size 指令送进显示任务队列，分辨率此刻才落定
    await until(() => player.displaySize().width > 0);
    expect(player.displaySize()).toEqual({ width: 1280, height: 720 });

    // 越界收敛：> 时长 → 时长；< 0 → 0
    player.seek(999_999);
    await until(() => player.getPosition() === 4400);
    player.seek(-5);
    await until(() => player.getPosition() === 0);

    // 播放中 seek：播放态恢复、位置越过 seek 目标继续推进
    // （恢复后首帧零延迟推进，精确中间值是毫秒级瞬态，不逐值断言）
    player.play();
    await until(() => player.isPlaying());
    player.seek(1200);
    await until(() => player.getPosition() >= 1200);
    await until(() => player.getPosition() > 1200, 10_000);
    player.dispose();
  }, 15_000);

  it('dispose 停止播放并终止后续解析', async () => {
    const { player } = await makeReadyPlayer();
    player.play();
    await until(() => player.isPlaying());
    player.dispose();
    expect(player.isPlaying()).toBe(false);
    const at = player.getPosition();
    await new Promise((r) => {
      setTimeout(r, 700);
    });
    expect(player.getPosition()).toBe(at);
  }, 15_000);
});

describe('formatRecordingTime', () => {
  it('毫秒 → mm:ss，非有限/负值按 0 处理', () => {
    expect(formatRecordingTime(0)).toBe('00:00');
    expect(formatRecordingTime(4400)).toBe('00:04');
    expect(formatRecordingTime(61_000)).toBe('01:01');
    expect(formatRecordingTime(3_600_000)).toBe('60:00');
    expect(formatRecordingTime(-1)).toBe('00:00');
    expect(formatRecordingTime(Number.NaN)).toBe('00:00');
  });
});
