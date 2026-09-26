/**
 * 远程桌面公共件的类型层（Guacamole 像素面）。
 *
 * 设计 docs/remote-gateway-guacamole-design.md §7 第 3 条约定抽取边界四件：
 * 连接生命周期 hook、票据获取客户端、错误与降级展示、监看/接管切换与工具栏。
 * 本文件是四件共同依赖的类型契约——**不 import 任何 React 或 wingman 私有
 * 模块**，任何消费方（cockpit dashboard 等）都能直接复用。
 */

/** 桌面协议（与后端 guacSupportedProtocol 对齐：rdp/vnc/ssh） */
export type RemoteProtocol = 'rdp' | 'vnc' | 'ssh';

/** 协议展示名（cockpit 侧可能有自己的协议命名，映射放在消费方） */
export const REMOTE_PROTOCOL_LABEL: Record<RemoteProtocol, string> = {
  rdp: 'RDP',
  vnc: 'VNC',
  ssh: 'SSH',
};

/** 一次远程会话的连接参数（票据申请输入） */
export interface RemoteSessionParams {
  /** 目标 agent（host 取服务端注册表上报 IP，前端不可指定地址） */
  agentId: string;
  protocol: RemoteProtocol;
  /** 协议默认端口（rdp 3389 / vnc 5900 / ssh 22），0 或缺省用默认 */
  port?: number;
  username?: string;
  password?: string;
  domain?: string;
  /** 只读监看（false 为接管，需 desktop:control 权限） */
  readOnly?: boolean;
  /** 会话录制（设计 §16）：服务端未配置录制双路径时票据申请被 400 拒绝 */
  record?: boolean;
  width?: number;
  height?: number;
}

/** 一次性连接票据（5 分钟有效，单次消费） */
export interface RemoteTicket {
  ticket: string;
  /** RFC3339 */
  expiresAt: string;
}

/**
 * 票据获取客户端（§7 第 3 条第 2 件）。
 *
 * 抽成接口而非直接调 wingman 的 `@/services/remote`：DG-6 约定 guacd 网关、
 * 票据与审计「一套不各写一遍」，但两边的部署前缀/鉴权方式可能不同（cockpit
 * 可能挂自己的 API base）。消费方实现本接口即可，公共件零改动。
 */
export interface TicketClient {
  /** 申请一次性票据 */
  issueTicket(params: RemoteSessionParams): Promise<RemoteTicket>;
  /** 票据 → WS 隧道 URL（票据即凭证，query 通道见设计 §13.1） */
  tunnelURL(ticket: string): string;
}

/** 连接状态机（§7 第 3 条第 1 件的对外形状） */
export type RemotePhaseName = 'idle' | 'connecting' | 'connected' | 'error';

/** 能力矩阵：各协议在 Guacamole 层的差异（设计 §15 文件通道） */
export interface ProtocolCapabilities {
  /** 文件传输通道（RFB 协议层无文件通道，VNC 恒 false） */
  fileTransfer: boolean;
  /** 文件通道说明（消费方可替换为自己的措辞） */
  fileTransferHint: string;
}

/**
 * 协议能力派生。纯函数——公共件与消费方共用同一判定，
 * 避免「VNC 显示上传按钮但后端无通道」这类分叉。
 */
export function protocolCapabilities(protocol: RemoteProtocol): ProtocolCapabilities {
  if (protocol === 'ssh') {
    return { fileTransfer: true, fileTransferHint: '经 SFTP 传到远端家目录' };
  }
  if (protocol === 'rdp') {
    return {
      fileTransfer: true,
      fileTransferHint: '经 RDP 驱动器重定向（远端"计算机"里可见共享盘）',
    };
  }
  return { fileTransfer: false, fileTransferHint: '' };
}

/** guacd 目录 listing 里的一条文件项（JSON 契约，设计 §15） */
export interface RemoteFileEntry {
  name: string;
  directory: boolean;
  mimetype: string;
  /** 字节数；目录恒 0 */
  size: number;
}

/** 远端输入流（Guacamole.InputStream 的结构化最小面，便于伪造测试） */
export interface RemoteStreamIn {
  onblob?: (data: string) => void;
  onend?: () => void;
  sendAck(message: string, code: number): void;
}

/** 远端输出流（Guacamole.OutputStream 的结构化最小面） */
export interface RemoteStreamOut {
  onack?: (status: { code: number; message: string }) => void;
  sendBlob(data: string): void;
  sendEnd(): void;
}

/**
 * 远端文件系统对象（guacd `filesystem` 指令下发的 Guacamole.Object 的
 * 结构化最小面）。SSH 会话由 guacd 随 SFTP 子系统上报；RDP 驱动器也走
 * 同一对象协议。目录/文件内容都以「name = 对象内绝对路径」的流下发。
 *
 * 协议边界（1.5.x 线协议只有 get/put）：**没有删除/重命名指令**——
 * 文件浏览器只提供列目录/下载/上传，删除属协议层不可行（不是 UI 取舍）。
 */
export interface RemoteFileSystemObject {
  index: number;
  requestInputStream(
    name: string,
    bodyCallback?: (stream: RemoteStreamIn, mimetype: string) => void,
  ): void;
  createOutputStream(mimetype: string, name: string): RemoteStreamOut;
}

/**
 * 文件操作审计上报载荷（§15.1 第二版，设计 §15.1「审计」小节）。客户端
 * best-effort 上报下载/上传的最终结果；服务端按 ticket 反解会话/agent/
 * 操作者（请求体同类字段不采信），list 高频不审计。
 */
export interface RemoteFileOpAudit {
  /** 当前会话票据（服务端反查键；会话关闭后上报则落 no_session 降级行） */
  ticket?: string;
  action: 'download' | 'upload';
  /** 远端绝对路径（含文件名） */
  path: string;
  result: 'ok' | 'fail';
  /** 文件字节数（客户端已知时带） */
  sizeBytes?: number;
  /** 实际尝试次数（重试后成功 >1；上限 5） */
  attempts?: number;
  /** 最终失败原因（result=fail 时带，服务端截断落库） */
  error?: string;
}
