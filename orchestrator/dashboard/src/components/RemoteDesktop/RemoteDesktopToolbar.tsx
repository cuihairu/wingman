/**
 * 监看/接管状态条 + 工具栏（公共件，设计 §7 第 3 条第 4 件）。
 *
 * 抽出理由：模式语义（监看 = 无输入注入 + 无发送入口）是**安全约束**，
 * 必须两处 UI 用同一份判定，否则第二方很容易只隐藏一半。工具栏把
 * 「录制指示 / 剪贴板 / 文件传输」三块收在一个组件里，两处 dashboard
 * 只需传 props。
 *
 * 约束（设计 §14/§15/§16）：
 *  - 监看模式：隐藏剪贴板发送与上传启用（网关不解析指令，UI 层不给入口
 *    是无歧义的约束）；
 *  - VNC 无文件通道（RFB 协议层没有），整块文件 UI 不渲染；
 *  - 录制中明示「按键内容不会被录入录像」（安全默认，设计 §16）。
 */
import { useRef, useState } from 'react';
import { Badge, Button, Input, Space, Tooltip, Typography } from 'antd';
import {
  CopyOutlined,
  DesktopOutlined,
  EyeOutlined,
  FolderOpenOutlined,
  SendOutlined,
  UploadOutlined,
  VideoCameraOutlined,
} from '@ant-design/icons';
import { protocolCapabilities, type RemoteProtocol } from './types';

const { Text } = Typography;

export interface RemoteDesktopToolbarProps {
  protocol: RemoteProtocol;
  /** true 监看 / false 接管 */
  readOnly: boolean;
  /** 会话是否录制中（控制红色指示） */
  record: boolean;
  /** 最近一次收到的远端剪贴板文本（空串 = 无） */
  clipboard: string;
  /** 发送到远端剪贴板（监看模式下按钮不渲染） */
  onSendClipboard: (text: string) => void;
  /** 复制远端剪贴板到本机（需用户手势，由消费方执行 navigator.clipboard） */
  onCopyToLocal: (text: string) => void;
  /** 选择文件上传（监看模式禁用；VNC 无入口） */
  onUploadFiles: (files: FileList | null) => void;
  /** 文件浏览器（§15 SSH/SFTP 树）：已就绪（文件系统对象已上报）才可打开。
      可选（缺省视为未就绪/收起）——第二方可以完全不用浏览器。 */
  fileBrowserReady?: boolean;
  /** 浏览器当前是否展开 */
  fileBrowserOpen?: boolean;
  /** 展开/收起文件浏览器 */
  onToggleFileBrowser?: () => void;
}

/** RemoteDesktopToolbar 工具栏（无连接逻辑，纯粹受控渲染）。 */
export default function RemoteDesktopToolbar({
  protocol,
  readOnly,
  record,
  clipboard,
  onSendClipboard,
  onCopyToLocal,
  onUploadFiles,
  fileBrowserReady = false,
  fileBrowserOpen = false,
  onToggleFileBrowser,
}: RemoteDesktopToolbarProps) {
  const [draft, setDraft] = useState('');
  const fileInputRef = useRef<HTMLInputElement>(null);
  const caps = protocolCapabilities(protocol);

  const submitClipboard = () => {
    const text = draft.trim();
    if (!text) {
      return;
    }
    onSendClipboard(text);
    setDraft('');
  };

  const pickFiles = () => {
    fileInputRef.current?.click();
  };

  const onFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    onUploadFiles(e.target.files);
    // 复位 e.target 自身（不查 ref：input 恒在，查 ref 是不可达分支），
    // 否则连续选同一个文件不会再触发 change
    e.target.value = '';
  };

  return (
    <Space direction="vertical" style={{ width: '100%' }} size="small">
      {/* 会话模式条：监看/接管语义明示（监看 = 远端键鼠不被本机接管） */}
      <Space size="small">
        <Badge
          color={readOnly ? 'blue' : 'green'}
          text={
            <Text type="secondary" style={{ fontSize: 12 }}>
              {readOnly ? <EyeOutlined /> : <DesktopOutlined />}{' '}
              {readOnly ? '监看模式：键鼠输入不注入远端' : '接管模式：本机键鼠即远端键鼠'}
            </Text>
          }
        />
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
      </Space>

      {/* 剪贴板（设计 §14）：接收全协议可用；发送仅接管模式 */}
      <Space style={{ width: '100%', justifyContent: 'space-between' }} align="start">
        <Text type="secondary" style={{ whiteSpace: 'nowrap', lineHeight: '32px' }}>
          剪贴板
        </Text>
        <div style={{ flex: 1, minWidth: 0 }}>
          {clipboard ? (
            <Space>
              <Text ellipsis style={{ maxWidth: 460, display: 'inline-block' }}>
                {clipboard}
              </Text>
              <Button size="small" icon={<CopyOutlined />} onClick={() => onCopyToLocal(clipboard)}>
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
            value={draft}
            onChange={(e) => setDraft(e.target.value)}
            onPressEnter={submitClipboard}
            maxLength={65536}
          />
          <Button
            type="primary"
            icon={<SendOutlined />}
            onClick={submitClipboard}
            disabled={!draft.trim()}
          >
            发送
          </Button>
        </Space.Compact>
      )}

      {/* 文件传输（设计 §15）：SSH 走 SFTP、RDP 走驱动器重定向；VNC 无通道 */}
      {caps.fileTransfer && (
        <Space>
          <Button size="small" icon={<UploadOutlined />} disabled={readOnly} onClick={pickFiles}>
            上传文件{readOnly ? '（监看模式不可用）' : ''}
          </Button>
          <Text type="secondary" style={{ fontSize: 12 }}>
            {caps.fileTransferHint}；远端下发的文件自动下载
          </Text>
          <input
            ref={fileInputRef}
            type="file"
            multiple
            hidden
            onChange={onFileChange}
            data-testid="remote-file-input"
          />
        </Space>
      )}

      {/* 文件浏览器（§15 SSH/SFTP 树）：仅 SSH；SFTP 对象上报后才可用。
          监看模式同样可浏览/下载（只读动作），上传入口在浏览器内部按
          readOnly 隐藏——与剪贴板「收全协议、发仅接管」先例一致。 */}
      {protocol === 'ssh' && (
        <Tooltip
          title={fileBrowserReady ? '浏览远端文件（列目录/下载/上传）' : '等待 SFTP 通道就绪…'}
        >
          <Button
            size="small"
            icon={<FolderOpenOutlined />}
            disabled={!fileBrowserReady}
            type={fileBrowserOpen ? 'primary' : 'default'}
            onClick={() => onToggleFileBrowser?.()}
            data-testid="remote-fs-toggle"
          >
            文件浏览
          </Button>
        </Tooltip>
      )}
    </Space>
  );
}
