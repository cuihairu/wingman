/**
 * 远程桌面面板（公共件主出口）。
 *
 * 组件分层（设计 §7 第 3 条的四件都在这里汇合成可用单元）：
 *
 *   useGuacamoleSession   连接生命周期（票据→WS→guacd），零 UI
 *   RemoteDesktopToolbar  监看/接管 + 录制指示 + 剪贴板 + 文件
 *   RemoteErrorNotice     错误分类与降级展示
 *   base64                流载荷编解码
 *   types                 TicketClient / 协议能力
 *
 * 消费方（wingman Agents 页的 RemoteDesktopModal、cockpit drawer 等）只需：
 *   1) 提供 TicketClient（票据申请 + WS URL）；
 *   2) 传连接参数；
 *   3) 决定容器（Modal / Drawer / 全屏 div）——面板本身不假定容器。
 */
import { useCallback, useRef } from 'react';
import { Spin, message } from 'antd';
import RemoteDesktopToolbar from './RemoteDesktopToolbar';
import RemoteErrorNotice from './RemoteErrorNotice';
import { useGuacamoleSession } from './useGuacamoleSession';
import type { RemoteSessionParams, TicketClient } from './types';

export interface RemoteDesktopPanelProps {
  /** true 建连 / false 断开（受控） */
  active: boolean;
  /** 连接参数（任一字段变化即重建会话——票据一次性无法复用） */
  params: RemoteSessionParams;
  /** 票据客户端（注入式，便于第二方对接自己的 API base，DG-6） */
  ticketClient: TicketClient;
  /** 画布高度（px），默认 480 */
  height?: number;
  /** 关闭回调：仅在会话可重试（error 且 retryable）时于错误条展示 */
  onRetry?: () => void;
  /** 自定义提示（默认 antd message；第二方可换成自己的 toast） */
  notify?: (kind: 'success' | 'error', text: string) => void;
}

/**
 * RemoteDesktopPanel 远程桌面面板（无容器外壳）。内部自持连接状态机，
 * 卸载/参数变化即断开。
 */
export default function RemoteDesktopPanel({
  active,
  params,
  ticketClient,
  height = 480,
  onRetry,
  notify,
}: RemoteDesktopPanelProps) {
  const stageRef = useRef<HTMLDivElement>(null);

  const handleNotify = useCallback(
    (kind: 'success' | 'error', text: string) => {
      if (notify) {
        notify(kind, text);
        return;
      }
      if (kind === 'success') {
        message.success(text);
      } else {
        message.error(text);
      }
    },
    [notify],
  );

  // 文件下发：聚合成 Blob 后触发浏览器下载（设计 §15）
  const handleFile = useCallback(
    ({ filename, blob }: { filename: string; blob: Blob }) => {
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = filename;
      a.click();
      URL.revokeObjectURL(url);
      handleNotify('success', `已下载 ${filename}`);
    },
    [handleNotify],
  );

  const handleCopyToLocal = useCallback(
    async (text: string) => {
      try {
        await navigator.clipboard.writeText(text);
        handleNotify('success', '已复制到本地剪贴板');
      } catch {
        handleNotify('error', '复制失败（浏览器剪贴板权限）');
      }
    },
    [handleNotify],
  );

  const { phase, error, clipboard, sendClipboard, uploadFiles } = useGuacamoleSession({
    active,
    params,
    ticketClient,
    stageRef,
    onNotify: handleNotify,
    onFile: handleFile,
  });

  return (
    <div data-testid="remote-desktop-panel">
      {phase === 'connecting' && (
        // nest 模式（必须有 children）才允许 tip，否则 antd 5 报警告
        <Spin tip="正在建立桌面会话…" style={{ display: 'block', margin: '48px auto' }}>
          <div style={{ height: 120 }} />
        </Spin>
      )}
      {phase === 'error' && <RemoteErrorNotice error={error} onRetry={onRetry} />}
      <div
        ref={stageRef}
        data-testid="remote-stage"
        style={{ height, background: '#000', overflow: 'hidden' }}
      />
      <RemoteDesktopToolbar
        protocol={params.protocol}
        readOnly={params.readOnly ?? false}
        record={params.record ?? false}
        clipboard={clipboard}
        onSendClipboard={sendClipboard}
        onCopyToLocal={handleCopyToLocal}
        onUploadFiles={uploadFiles}
      />
    </div>
  );
}
