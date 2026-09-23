/**
 * 远程桌面弹窗（Guacamole 像素面，P0）。
 * 打开时：申请一次性票据 → WS 隧道连 /api/remote/guacamole → guacd
 * 会话建立 → display 挂载 + 键鼠注入。关闭即断开；重连须重新申请
 * （票据一次性语义）。
 */
import { useEffect, useRef, useState } from 'react';
import { Modal, Spin, Typography } from 'antd';
import { Client, Keyboard, Mouse, WebSocketTunnel } from 'guacamole-common-js';
import { createRemoteTicket, guacamoleWSPath, type RemoteProtocol } from '@/services/remote';

const { Text } = Typography;

export interface RemoteDesktopModalProps {
  open: boolean;
  onCancel: () => void;
  agentId: string;
  protocol: RemoteProtocol;
  port?: number;
  username?: string;
  password?: string;
  /** true 监看 / false 接管（后端按 desktop:control 校验） */
  readOnly?: boolean;
  width?: number;
  height?: number;
}

const PROTOCOL_LABEL: Record<RemoteProtocol, string> = {
  rdp: 'RDP',
  vnc: 'VNC',
  ssh: 'SSH',
};

export default function RemoteDesktopModal({
  open,
  onCancel,
  agentId,
  protocol,
  port,
  username,
  password,
  readOnly = false,
  width = 1280,
  height = 800,
}: RemoteDesktopModalProps) {
  const stageRef = useRef<HTMLDivElement>(null);
  const clientRef = useRef<Client>();
  const [connecting, setConnecting] = useState(false);
  const [error, setError] = useState('');

  useEffect(() => {
    if (!open) {
      return;
    }
    let cancelled = false;
    let client: Client | undefined;
    let keyboard: Keyboard | undefined;
    const cleanups: Array<() => void> = [];

    setConnecting(true);
    setError('');

    (async () => {
      try {
        const { ticket } = await createRemoteTicket({
          agentId,
          protocol,
          port,
          username,
          password,
          readOnly,
          width,
          height,
        });
        if (cancelled) {
          return;
        }

        const tunnel = new WebSocketTunnel(guacamoleWSPath(ticket));
        client = new Client(tunnel);
        clientRef.current = client;
        client.onerror = () => {
          if (!cancelled) {
            setError('桌面会话异常断开，请重新打开');
          }
        };

        const stage = stageRef.current;
        if (!stage) {
          return;
        }
        const display = client.getDisplay();
        display.getElement().style.width = '100%';
        display.getElement().style.height = '100%';
        stage.appendChild(display.getElement());

        // 显示自适应：按 stage 尺寸缩放远端桌面
        const fitScale = () => {
          if (display.getWidth() > 0 && stage.clientWidth > 0) {
            display.scale = Math.min(stage.clientWidth / display.getWidth(), stage.clientHeight / display.getHeight());
          }
        };
        display.onresize = () => fitScale();
        const onWindowResize = () => fitScale();
        window.addEventListener('resize', onWindowResize);

        // 键鼠注入：监看模式不挂输入事件
        if (!readOnly) {
          const mouse = new Mouse(display.getElement());
          mouse.onmousedown = mouse.onmouseup = mouse.onmousemove = (state) => {
            client?.sendMouseState(state);
          };
          keyboard = new Keyboard(document);
          keyboard.onkeydown = (keysym) => client?.sendKeyEvent(1, keysym);
          keyboard.onkeyup = (keysym) => client?.sendKeyEvent(0, keysym);
        }

        cleanups.push(() => window.removeEventListener('resize', onWindowResize));

        client.connect();
        if (!cancelled) {
          setConnecting(false);
        }
      } catch (e) {
        if (!cancelled) {
          setConnecting(false);
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
  }, [open, agentId, protocol, port, username, password, readOnly, width, height]);

  return (
    <Modal
      title={`远程桌面 - ${agentId} (${PROTOCOL_LABEL[protocol]}${readOnly ? ' · 监看' : ' · 接管'})`}
      open={open}
      onCancel={onCancel}
      footer={null}
      width={1024}
      destroyOnClose
      forceRender
    >
      {connecting && <Spin tip="正在建立桌面会话…" style={{ display: 'block', margin: '48px auto' }} />}
      {!connecting && error && <Text type="danger">{error}</Text>}
      <div ref={stageRef} style={{ height: 560, background: '#000', overflow: 'hidden' }} />
    </Modal>
  );
}
