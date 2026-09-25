/**
 * 远程桌面弹窗（Guacamole 像素面，P0 + 阶段二）——**薄壳**。
 *
 * 真正的能力都在公共件 `@/components/RemoteDesktop`（设计 §7 第 3 条抽取的
 * 四件：连接生命周期 hook / 票据获取客户端 / 错误与降级展示 / 监看接管与
 * 工具栏）。本文件只负责 wingman 侧的容器决策：Modal 外壳 + 默认票据
 * 客户端 + 标题拼装。
 *
 * 为什么留这一层：cockpit 侧要的是「任意容器里的面板」（抽屉/全屏页），
 * 而 wingman Agents 页要的是「弹窗」。两者共用同一份连接语义，抽公共件
 * 正是为了这个；本壳是可替换的容器适配器，不是逻辑所在地。
 *
 * 历史导出保留：`guacEncodeBase64` / `guacDecodeBase64` 由此转发，
 * 既有调用方与测试无需改路径（工具函数的所有权已移交公共件）。
 */
import { useMemo } from 'react';
import { Modal } from 'antd';
import RemoteDesktopPanel from '@/components/RemoteDesktop/RemoteDesktopPanel';
import { createWingmanTicketClient } from '@/components/RemoteDesktop';
import {
  guacDecodeBase64,
  guacEncodeBase64,
  REMOTE_PROTOCOL_LABEL,
  type RemoteProtocol,
  type TicketClient,
} from '@/components/RemoteDesktop';

export { guacDecodeBase64, guacEncodeBase64 };

export interface RemoteDesktopModalProps {
  open: boolean;
  onCancel: () => void;
  agentId: string;
  protocol: RemoteProtocol;
  port?: number;
  username?: string;
  password?: string;
  domain?: string;
  /** true 监看 / false 接管（后端按 desktop:control 校验） */
  readOnly?: boolean;
  /** 会话录制（需服务端已配置录制双路径，否则票据申请 400） */
  record?: boolean;
  width?: number;
  height?: number;
  /** 画布高度（px），默认 480 */
  stageHeight?: number;
  /**
   * 票据客户端覆盖（默认对接本仓 Go server）。cockpit 等第二方可注入
   * 自己的 API base，无需 fork 组件。
   */
  ticketClient?: TicketClient;
}

/** RemoteDesktopModal 远程桌面弹窗（容器适配器）。 */
export default function RemoteDesktopModal({
  open,
  onCancel,
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
  stageHeight = 480,
  ticketClient,
}: RemoteDesktopModalProps) {
  // 默认客户端模块级单例：每次渲染新建对象会让 hook 的 ticketClient
  // 依赖变化 → 重建会话 → 白白消耗一次性票据。
  const defaultClient = useMemo(() => createWingmanTicketClient(), []);
  const client = ticketClient ?? defaultClient;

  const params = useMemo(
    () => ({
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
    }),
    [agentId, protocol, port, username, password, domain, readOnly, record, width, height],
  );

  return (
    <Modal
      title={`远程桌面 - ${agentId} (${REMOTE_PROTOCOL_LABEL[protocol]}${readOnly ? ' · 监看' : ' · 接管'}${record ? ' · 录制中' : ''})`}
      open={open}
      onCancel={onCancel}
      footer={null}
      width={1024}
      destroyOnClose
      forceRender
    >
      <RemoteDesktopPanel
        active={open}
        params={params}
        ticketClient={client}
        height={stageHeight}
        onRetry={onCancel}
      />
    </Modal>
  );
}
