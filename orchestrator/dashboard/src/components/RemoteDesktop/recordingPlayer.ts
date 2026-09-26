/**
 * 会话录像回放封装（设计 §16「回放」，公共件）。
 *
 * SessionRecording 是 guacamole-common-js 自带能力（零新增依赖）：构造
 * 即解析录像源——录像格式是裸 Guacamole 指令流，非 sync 指令累积、
 * `sync,<毫秒时间戳>;` 收口为一帧，回放位置以首帧时间戳为原点。这里包
 * 一层最小状态机（加载/错误/时长/播放态/位置），把回调式 API 收敛成
 * 可直接渲染的面板依赖；React 组件只做视图。
 *
 * 上游缺陷（1.5.0 dist 实测）：SessionRecording 的 Blob 直连分支把从未
 * 赋值的 recordingBlob 交给解析器（缺 `recordingBlob = source` 一行），
 * 构造即抛 TypeError；npm 无更新版本可升级。绕开方式：BlobRecording-
 * Tunnel 把 Blob 转成隧道源——connect() 后用官方 Parser 解析录像文本、
 * 逐条喂给 SessionRecording 接管的 oninstruction、读完发 CLOSED。这是
 * 官方播放器「边下边播」走的隧道分支，行为有保障；seek/play 所需的
 * recordingBlob 由 SessionRecording 自己从指令流重建，不依赖缺陷分支。
 *
 * 权限与作用域：回放是纯浏览器本地行为（blob 解析 + canvas 绘制），
 * 不经网关、无注入面；录像取回的权限与审计在 recordings API
 * （desktop:view，desktop.recording_download）。
 */
import Guacamole from 'guacamole-common-js';
import type { RecordingTunnelSource } from 'guacamole-common-js';

/**
 * 只读回放隧道：把录像 Blob 喂给 SessionRecording（duck-type 满足
 * RecordingTunnelSource；生命周期由 SessionRecording 接管——构造时设置
 * oninstruction/onerror/onstatechange，abort 时调 disconnect）。导出供
 * 测试与后续二方复用（绕上游缺陷的官方入口）。
 */
export class BlobRecordingTunnel implements RecordingTunnelSource {
  oninstruction?: (opcode: string, args: string[]) => void;

  onerror?: (status: { message: string }) => void;

  onstatechange?: (state: number) => void;

  private closed = false;

  constructor(private readonly blob: Blob) {}

  /** 读全文（FileReader：jsdom 的 Blob 没有 text()，浏览器端两者皆可） */
  private readText(): Promise<string> {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => resolve(String(reader.result));
      reader.onerror = () => reject(reader.error ?? new Error('录像读取失败'));
      reader.readAsText(this.blob);
    });
  }

  connect(): void {
    this.readText()
      .then((text) => {
        if (this.closed) {
          return;
        }
        const parser = new Guacamole.Parser();
        parser.oninstruction = (opcode, args) => this.oninstruction?.(opcode, args);
        try {
          parser.receive(text);
        } catch (e) {
          // 解析错误经隧道错误通道上报（SessionRecording 记 errorEncountered，
          // 之后的 CLOSED 不再视为加载完成）
          this.onerror?.({ message: e instanceof Error ? e.message : String(e) });
          return;
        }
        this.onstatechange?.(Guacamole.Tunnel.State.CLOSED);
      })
      .catch((e: unknown) => {
        if (!this.closed) {
          this.onerror?.({ message: e instanceof Error ? e.message : '录像读取失败' });
        }
      });
  }

  sendMessage(): void {
    // 回放隧道只进不出
  }

  disconnect(): void {
    this.closed = true;
  }
}

export interface RecordingPlayerEvents {
  /** 解析完成：时长就绪，可挂载画面并 play/seek */
  onReady?: (durationMs: number) => void;
  /** 解析失败（非法指令流等），回放不可用 */
  onError?: (message: string) => void;
  /** 位置变化（播放逐帧推进与 seek 都走这里，毫秒） */
  onPosition?: (positionMs: number) => void;
  /** 播放/暂停切换（含播到结尾的自动暂停） */
  onPlayingChange?: (playing: boolean) => void;
  /**
   * 画面分辨率确定/变化（size 指令经显示任务队列落定后触发；
   * onReady 时分辨率可能尚未落定，自适应缩放以本事件为准）
   */
  onDisplayResize?: (width: number, height: number) => void;
}

export interface RecordingPlayer {
  /** 画面元素（canvas 容器 div），由调用方挂到 DOM */
  element: HTMLElement;
  play(): void;
  pause(): void;
  toggle(): void;
  /** 跳到指定位置（毫秒，越界收敛到 [0, 时长]） */
  seek(positionMs: number): void;
  /** 按录像画面分辨率等比缩放显示（1 = 原始分辨率；只影响显示尺寸） */
  scale(factor: number): void;
  /** 录像画面分辨率（size 指令决定；解析完成前为 0） */
  displaySize(): { width: number; height: number };
  isPlaying(): boolean;
  getPosition(): number;
  getDuration(): number;
  /** 停止播放并终止后续解析；调用后实例不再使用 */
  dispose(): void;
}

export function createRecordingPlayer(
  blob: Blob,
  events: RecordingPlayerEvents = {},
): RecordingPlayer {
  // Blob 走隧道适配器（Blob 直连分支 1.5.0 有缺陷，见头注）
  const recording = new Guacamole.SessionRecording(new BlobRecordingTunnel(blob));
  recording.onload = () => events.onReady?.(recording.getDuration());
  recording.onerror = (message) => events.onError?.(message || '录像解析失败');
  recording.onseek = (position) => events.onPosition?.(position);
  recording.onplay = () => events.onPlayingChange?.(true);
  recording.onpause = () => events.onPlayingChange?.(false);
  const display = recording.getDisplay();
  display.onresize = (width, height) => events.onDisplayResize?.(width, height);
  // 隧道源不自动喂数据：connect() 触发适配器解析并装载（官方播放器同款时序）
  recording.connect();
  return {
    element: display.getElement(),
    play: () => recording.play(),
    pause: () => recording.pause(),
    toggle: () => (recording.isPlaying() ? recording.pause() : recording.play()),
    seek: (positionMs) => {
      const duration = recording.getDuration();
      recording.seek(Math.max(0, Math.min(positionMs, duration)));
    },
    scale: (factor) => display.scale(factor),
    displaySize: () => ({ width: display.getWidth(), height: display.getHeight() }),
    isPlaying: () => recording.isPlaying(),
    getPosition: () => recording.getPosition(),
    getDuration: () => recording.getDuration(),
    dispose: () => {
      recording.pause();
      recording.abort();
    },
  };
}

/** 毫秒 → mm:ss（录像进度展示口径） */
export function formatRecordingTime(ms: number): string {
  const total = Number.isFinite(ms) && ms > 0 ? Math.floor(ms / 1000) : 0;
  const m = Math.floor(total / 60);
  const s = total % 60;
  return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
}
