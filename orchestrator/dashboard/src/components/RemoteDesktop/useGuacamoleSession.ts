/**
 * 连接生命周期 hook（公共件，设计 §7 第 3 条第 1 件）。
 *
 * 职责边界：建连 / 断连状态机 + guacamole client 生命周期 + 剪贴板与文件流
 * 收发编排。**不含任何 UI**——显示挂载、工具栏、错误展示都在上层组件，
 * 这样 cockpit 可以在任意容器（抽屉/全屏页/侧边栏）里复用同一套连接语义。
 *
 * 状态机（票据一次性，断线重连必须重新申请，故无自动重连）：
 *
 *   idle ──activate()──> connecting ──client.connect()──> connected
 *                            │                                │
 *                            └──── 票据/握手失败 ──> error <───┘
 *                                                    （onerror → error）
 *
 * 关键不变式：
 *  - effect 依赖变化/卸载时必定 disconnect 并清空 stage（不留幽灵隧道）；
 *  - 异步建连途中组件卸载（cancelled）时不再 setState，也不建 client；
 *  - 监看模式（readOnly）不挂 Keyboard/Mouse 输入面。
 */
import { useCallback, useEffect, useRef, useState } from 'react';
import Guacamole from 'guacamole-common-js';
import type { Client, Keyboard } from 'guacamole-common-js';
import { guacDecodeBase64, guacDecodeBase64ToBytes, guacEncodeBase64 } from './base64';
import { type RemotePhaseName, type RemoteSessionParams, type TicketClient } from './types';

export interface UseGuacamoleSessionOptions {
  /** true 时建连；false 时断开（受控开关，对应弹窗 open） */
  active: boolean;
  /** 连接参数（变更即重建会话——票据一次性，无法复用） */
  params: RemoteSessionParams;
  /** 票据客户端（注入，便于第二方对接自己的 API base） */
  ticketClient: TicketClient;
  /** 远端剪贴板内容更新（仅 text/*，已逐块 ack） */
  onClipboard?: (text: string) => void;
  /** 远端文件下发完成（blob 已聚合，调用方负责触发浏览器下载） */
  onFile?: (file: { filename: string; blob: Blob }) => void;
  /** 提示回调（上传完成/失败、剪贴板 ack 失败等瞬时反馈） */
  onNotify?: (kind: 'success' | 'error', text: string) => void;
  /** 画布挂载点 ref（display 元素注入这里；为空时跳过挂载） */
  stageRef: React.RefObject<HTMLDivElement>;
}

export interface GuacamoleSessionState {
  phase: RemotePhaseName;
  error: string;
  /** 最近一次收到的远端剪贴板文本 */
  clipboard: string;
  /** 当前 client（发送通道用；未建连时 undefined） */
  client?: Client;
  /** 发送文本到远端剪贴板（text/plain 单流；未建连或空文本为 no-op） */
  sendClipboard: (text: string) => void;
  /** 上传文件（逐个 createFileStream + BlobWriter；未建连为 no-op） */
  uploadFiles: (files: FileList | null) => void;
}

/**
 * useGuacamoleSession 返回连接状态机与发送通道。UI 层只读 phase/error/
 * clipboard 并调用 sendClipboard/uploadFiles。
 */
export function useGuacamoleSession(options: UseGuacamoleSessionOptions): GuacamoleSessionState {
  const { active, params, ticketClient, onClipboard, onFile, onNotify, stageRef } = options;
  const {
    agentId,
    protocol,
    port,
    username,
    password,
    domain,
    readOnly = false,
    record = false,
    width = 1280,
    height = 800,
  } = params;

  const clientRef = useRef<Client>();
  const [phase, setPhase] = useState<RemotePhaseName>('idle');
  const [error, setError] = useState('');
  const [clipboard, setClipboard] = useState('');

  // 回调进 ref：消费方通常传内联箭头函数，放进 effect 依赖会导致每渲染
  // 重建会话（票据一次性，重建即浪费一次申请 + 打断像素面）。
  const notifyRef = useRef(onNotify);
  notifyRef.current = onNotify;
  const clipboardRef = useRef(onClipboard);
  clipboardRef.current = onClipboard;
  const fileRef = useRef(onFile);
  fileRef.current = onFile;

  useEffect(() => {
    if (!active) {
      return undefined;
    }
    let cancelled = false;
    let client: Client | undefined;
    let keyboard: Keyboard | undefined;
    const cleanups: Array<() => void> = [];

    setPhase('connecting');
    setError('');
    setClipboard('');

    (async () => {
      try {
        const { ticket } = await ticketClient.issueTicket({
          agentId,
          protocol,
          port,
          username,
          password,
          domain,
          readOnly,
          record,
          width,
          height,
        });
        if (cancelled) {
          return;
        }

        const tunnel = new Guacamole.WebSocketTunnel(ticketClient.tunnelURL(ticket));
        client = new Guacamole.Client(tunnel);
        clientRef.current = client;
        client.onerror = () => {
          if (!cancelled) {
            setPhase('error');
            setError('桌面会话异常断开，请重新打开');
          }
        };

        // 剪贴板接收（设计 §14）：只收 text/*；每个 blob 都要 ack，否则远端停发
        client.onclipboard = (stream, mimetype) => {
          if (!mimetype.startsWith('text/')) {
            stream.sendAck('unsupported', Guacamole.Status.Code.UNSUPPORTED);
            return;
          }
          let text = '';
          stream.onblob = (data) => {
            text += guacDecodeBase64(data);
            stream.sendAck('ok', Guacamole.Status.Code.SUCCESS);
          };
          stream.onend = () => {
            if (!cancelled && text) {
              setClipboard(text);
              clipboardRef.current?.(text);
            }
          };
        };

        // 文件下载（设计 §15）：onfile 聚合 blob 成 Blob 交上层触发下载
        client.onfile = (stream, filename, mimetype) => {
          const chunks: BlobPart[] = [];
          stream.onblob = (data) => {
            chunks.push(guacDecodeBase64ToBytes(data));
            stream.sendAck('ok', Guacamole.Status.Code.SUCCESS);
          };
          stream.onend = () => {
            if (cancelled) {
              return;
            }
            fileRef.current?.({
              filename: filename || 'remote-file',
              blob: new Blob(chunks, { type: mimetype || 'application/octet-stream' }),
            });
          };
        };

        const stage = stageRef.current;
        if (!stage) {
          return;
        }
        const display = client.getDisplay();
        display.getElement().style.width = '100%';
        display.getElement().style.height = '100%';
        stage.appendChild(display.getElement());

        // 显示自适应：按 stage 尺寸等比缩放远端桌面
        const fitScale = () => {
          if (display.getWidth() > 0 && stage.clientWidth > 0) {
            display.scale = Math.min(
              stage.clientWidth / display.getWidth(),
              stage.clientHeight / display.getHeight(),
            );
          }
        };
        display.onresize = () => fitScale();
        const onWindowResize = () => fitScale();
        window.addEventListener('resize', onWindowResize);

        // 键鼠注入：监看模式不挂输入面（UI 层也无发送入口，双层约束）
        if (!readOnly) {
          const mouse = new Guacamole.Mouse(display.getElement());
          mouse.onmousedown =
            mouse.onmouseup =
            mouse.onmousemove =
              (state) => {
                client?.sendMouseState(state);
              };
          keyboard = new Guacamole.Keyboard(document);
          keyboard.onkeydown = (keysym) => client?.sendKeyEvent(1, keysym);
          keyboard.onkeyup = (keysym) => client?.sendKeyEvent(0, keysym);
        }

        cleanups.push(() => window.removeEventListener('resize', onWindowResize));

        client.connect();
        if (!cancelled) {
          setPhase('connected');
        }
      } catch (e) {
        if (!cancelled) {
          setPhase('error');
          setError(e instanceof Error ? e.message : '桌面连接失败');
        }
      }
    })();

    return () => {
      cancelled = true;
      cleanups.forEach((fn) => fn());
      try {
        clientRef.current?.disconnect();
      } catch {
        // 断开失败无需处理（会话侧 5 分钟票据超时兜底）
      }
      clientRef.current = undefined;
      if (stageRef.current) {
        stageRef.current.innerHTML = '';
      }
    };
  }, [
    active,
    agentId,
    protocol,
    port,
    username,
    password,
    domain,
    readOnly,
    record,
    width,
    height,
    ticketClient,
  ]);

  /** 发送文本到远端剪贴板（text/plain 单流；ack 非成功即报错） */
  const sendClipboard = useCallback((text: string) => {
    const client = clientRef.current;
    const trimmed = text.trim();
    if (!client || !trimmed) {
      return;
    }
    const stream = client.createClipboardStream('text/plain');
    stream.onack = (status) => {
      if (status.code !== Guacamole.Status.Code.SUCCESS) {
        notifyRef.current?.('error', `剪贴板发送失败（0x${status.code.toString(16)}）`);
      }
    };
    stream.sendBlob(guacEncodeBase64(trimmed));
    stream.sendEnd();
  }, []);

  /** 上传文件：createFileStream + BlobWriter 分块，完成后手动 sendEnd */
  const uploadFiles = useCallback((files: FileList | null) => {
    const client = clientRef.current;
    if (!client || !files || files.length === 0) {
      return;
    }
    Array.from(files).forEach((file) => {
      const stream = client.createFileStream(file.type || 'application/octet-stream', file.name);
      const writer = new Guacamole.BlobWriter(stream);
      writer.oncomplete = () => {
        stream.sendEnd();
        notifyRef.current?.('success', `${file.name} 上传完成`);
      };
      writer.onerror = () => notifyRef.current?.('error', `${file.name} 上传失败`);
      writer.sendBlob(file);
    });
  }, []);

  return { phase, error, clipboard, client: clientRef.current, sendClipboard, uploadFiles };
}
