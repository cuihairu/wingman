/**
 * 远程桌面弹窗（Guacamole 像素面，P0 + 阶段二）。
 * 打开时：申请一次性票据 → WS 隧道连 /api/remote/guacamole → guacd
 * 会话建立 → display 挂载 + 键鼠注入。关闭即断开；重连须重新申请
 * （票据一次性语义）。
 *
 * 阶段二（设计 §14/§15/§16）：
 * - 剪贴板：onclipboard 收（逐块 ack）+ createClipboardStream 发
 *   （监看模式隐藏发送 UI，只保留接收）
 * - 文件传输：上传 createFileStream + BlobWriter；下载 onfile 聚合
 *   成 Blob 触发浏览器下载。VNC 无文件通道（RFB 协议层没有），
 *   整块文件 UI 隐藏
 * - 录制：record 票据 + 红色录制中指示（按键内容服务端永不录制）
 */
import { useEffect, useRef, useState } from 'react';
import { Badge, Button, Divider, Input, Modal, Spin, Space, Typography, message } from 'antd';
import { CopyOutlined, SendOutlined, UploadOutlined, VideoCameraOutlined } from '@ant-design/icons';
// UMD 包：构造器全在 default 导出上（具名导入 webpack 生产构建解析失败）；
// 类型经 import type 引用，转译后擦除、不触碰运行时形状。
import Guacamole from 'guacamole-common-js';
import type { Client, Keyboard } from 'guacamole-common-js';
import { createRemoteTicket, guacamoleWSPath, type RemoteProtocol } from '@/services/remote';

const { Text } = Typography;

// ---------- base64（Guacamole 流载荷是 base64 文本；Unicode 安全编解码） ----------

/** 文本 → base64（经 UTF-8 字节，绕过 btoa 的 latin1 限制） */
export function guacEncodeBase64(text: string): string {
  const bytes = new TextEncoder().encode(text);
  let binary = '';
  bytes.forEach((b) => {
    binary += String.fromCharCode(b);
  });
  return btoa(binary);
}

/** base64 → 文本（经 UTF-8 字节） */
export function guacDecodeBase64(data: string): string {
  const binary = atob(data);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) {
    bytes[i] = binary.charCodeAt(i);
  }
  return new TextDecoder().decode(bytes);
}

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
  /** 会话录制（需服务端已配置录制双路径，否则票据申请 400） */
  record?: boolean;
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
  record = false,
  width = 1280,
  height = 800,
}: RemoteDesktopModalProps) {
  const stageRef = useRef<HTMLDivElement>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const clientRef = useRef<Client>();
  const [connecting, setConnecting] = useState(false);
  const [error, setError] = useState('');
  // 剪贴板面板：received = 远端最近一次剪贴板内容；draft = 待发送文本
  const [receivedClipboard, setReceivedClipboard] = useState('');
  const [clipboardDraft, setClipboardDraft] = useState('');
  // 文件通道能力：SSH=SFTP / RDP=驱动器重定向；VNC 无（协议层不存在）
  const fileTransferSupported = protocol === 'ssh' || protocol === 'rdp';

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
    setReceivedClipboard('');

    (async () => {
      try {
        const { ticket } = await createRemoteTicket({
          agentId,
          protocol,
          port,
          username,
          password,
          readOnly,
          record,
          width,
          height,
        });
        if (cancelled) {
          return;
        }

        const tunnel = new Guacamole.WebSocketTunnel(guacamoleWSPath(ticket));
        client = new Guacamole.Client(tunnel);
        clientRef.current = client;
        client.onerror = () => {
          if (!cancelled) {
            setError('桌面会话异常断开，请重新打开');
          }
        };

        // 剪贴板接收（§14）：只收 text/*；每个 blob 都要 ack，否则远端停发
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
              setReceivedClipboard(text);
            }
          };
        };

        // 文件下载（§15）：onfile 聚合 blob 成 Blob，触发浏览器下载
        client.onfile = (stream, filename, mimetype) => {
          const chunks: BlobPart[] = [];
          stream.onblob = (data) => {
            const binary = atob(data);
            const bytes = new Uint8Array(binary.length);
            for (let i = 0; i < binary.length; i += 1) {
              bytes[i] = binary.charCodeAt(i);
            }
            chunks.push(bytes);
            stream.sendAck('ok', Guacamole.Status.Code.SUCCESS);
          };
          stream.onend = () => {
            if (cancelled) {
              return;
            }
            const blob = new Blob(chunks, { type: mimetype || 'application/octet-stream' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename || 'remote-file';
            a.click();
            URL.revokeObjectURL(url);
            message.success(`已下载 ${filename}`);
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

        // 显示自适应：按 stage 尺寸缩放远端桌面
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

        // 键鼠注入：监看模式不挂输入事件
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
  }, [open, agentId, protocol, port, username, password, readOnly, record, width, height]);

  // 剪贴板发送（§14）：text/plain 单流；监看会话无发送能力（UI 已隐藏，
  // 这里再兜底一层）
  const sendClipboard = () => {
    const client = clientRef.current;
    const text = clipboardDraft.trim();
    if (!client || !text) {
      return;
    }
    const stream = client.createClipboardStream('text/plain');
    stream.onack = (status) => {
      if (status.code !== Guacamole.Status.Code.SUCCESS) {
        message.error(`剪贴板发送失败（0x${status.code.toString(16)}）`);
      }
    };
    stream.sendBlob(guacEncodeBase64(text));
    stream.sendEnd();
    setClipboardDraft('');
  };

  // 文件上传（§15）：createFileStream + BlobWriter 分块；完成后手动
  // sendEnd（1.5.0 的 BlobWriter 不代发 end）
  const uploadFiles = (files: FileList | null) => {
    const client = clientRef.current;
    if (!client || !files || files.length === 0) {
      return;
    }
    Array.from(files).forEach((file) => {
      const stream = client.createFileStream(file.type || 'application/octet-stream', file.name);
      const writer = new Guacamole.BlobWriter(stream);
      writer.oncomplete = () => {
        stream.sendEnd();
        message.success(`${file.name} 上传完成`);
      };
      writer.onerror = () => message.error(`${file.name} 上传失败`);
      writer.sendBlob(file);
    });
    if (fileInputRef.current) {
      fileInputRef.current.value = '';
    }
  };

  const copyClipboardToLocal = async () => {
    if (!receivedClipboard) {
      return;
    }
    try {
      await navigator.clipboard.writeText(receivedClipboard);
      message.success('已复制到本地剪贴板');
    } catch {
      message.error('复制失败（浏览器剪贴板权限）');
    }
  };

  return (
    <Modal
      title={`远程桌面 - ${agentId} (${PROTOCOL_LABEL[protocol]}${readOnly ? ' · 监看' : ' · 接管'}${record ? ' · 录制中' : ''})`}
      open={open}
      onCancel={onCancel}
      footer={null}
      width={1024}
      destroyOnClose
      forceRender
    >
      {connecting && (
        <Spin tip="正在建立桌面会话…" style={{ display: 'block', margin: '48px auto' }} />
      )}
      {!connecting && error && <Text type="danger">{error}</Text>}
      <div ref={stageRef} style={{ height: 480, background: '#000', overflow: 'hidden' }} />

      {/* 阶段二工具区：录制指示 + 剪贴板 + 文件传输 */}
      <Divider style={{ margin: '12px 0 8px' }} />
      <Space direction="vertical" style={{ width: '100%' }} size="small">
        {record && (
          <Badge
            color="red"
            status="processing"
            text={
              <Text type="secondary">
                <VideoCameraOutlined /> 会话录制中（按键内容不会被录入录像）
              </Text>
            }
          />
        )}

        {/* 剪贴板（§14）：接收全协议可用；发送仅接管模式 */}
        <Space style={{ width: '100%', justifyContent: 'space-between' }} align="start">
          <Text type="secondary" style={{ whiteSpace: 'nowrap', lineHeight: '32px' }}>
            剪贴板
          </Text>
          <div style={{ flex: 1, minWidth: 0 }}>
            {receivedClipboard ? (
              <Space>
                <Text
                  ellipsis
                  style={{ maxWidth: 460, display: 'inline-block', verticalAlign: 'bottom' }}
                >
                  {receivedClipboard}
                </Text>
                <Button size="small" icon={<CopyOutlined />} onClick={copyClipboardToLocal}>
                  复制到本地
                </Button>
              </Space>
            ) : (
              <Text type="secondary" style={{ fontSize: 12 }}>
                远端复制内容将显示在这里
              </Text>
            )}
          </div>
        </Space>
        {!readOnly && (
          <Space.Compact style={{ width: '100%' }}>
            <Input
              placeholder="输入文本发送到远端剪贴板"
              value={clipboardDraft}
              onChange={(e) => setClipboardDraft(e.target.value)}
              onPressEnter={sendClipboard}
              maxLength={65536}
            />
            <Button
              type="primary"
              icon={<SendOutlined />}
              onClick={sendClipboard}
              disabled={!clipboardDraft.trim()}
            >
              发送
            </Button>
          </Space.Compact>
        )}

        {/* 文件传输（§15）：SSH 走 SFTP、RDP 走驱动器重定向；VNC 无通道 */}
        {fileTransferSupported && (
          <Space>
            <Button
              size="small"
              icon={<UploadOutlined />}
              disabled={readOnly}
              onClick={() => fileInputRef.current?.click()}
            >
              上传文件{readOnly ? '（监看模式不可用）' : ''}
            </Button>
            <Text type="secondary" style={{ fontSize: 12 }}>
              {protocol === 'ssh'
                ? '经 SFTP 传到远端家目录'
                : '经 RDP 驱动器重定向（远端“计算机”里可见共享盘）'}
              ；远端下发的文件自动下载
            </Text>
            <input
              ref={fileInputRef}
              type="file"
              multiple
              hidden
              onChange={(e) => uploadFiles(e.target.files)}
            />
          </Space>
        )}
      </Space>
    </Modal>
  );
}
