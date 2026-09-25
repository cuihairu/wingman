/**
 * 远程桌面公共件出口（Guacamole 像素面，设计 §7 第 3 条）。
 *
 * 消费方只需从这里 import；第二方 dashboard（cockpit）按同一入口接入，
 * 避免各自复制连接管理逻辑导致协议细节分叉（设计 §7 第 3 条明确禁止
 * 复制粘贴）。四件的对应关系：
 *
 *  - 连接生命周期 hook → useGuacamoleSession
 *  - 票据获取客户端   → TicketClient（接口）+ createWingmanTicketClient
 *  - 错误与降级展示   → RemoteErrorNotice / classifyRemoteError
 *  - 监看接管与工具栏 → RemoteDesktopToolbar
 */
import { createRemoteTicket, guacamoleWSPath } from '@/services/remote';
import type { RemoteSessionParams, TicketClient } from './types';

/** wingman 默认票据客户端（对接本仓 Go server 的 /api/remote/*） */
export const createWingmanTicketClient = (): TicketClient => ({
  issueTicket: (params: RemoteSessionParams) => createRemoteTicket(params),
  tunnelURL: (ticket: string) => guacamoleWSPath(ticket),
});

export { default as RemoteDesktopPanel } from './RemoteDesktopPanel';
export { default as RemoteDesktopToolbar } from './RemoteDesktopToolbar';
export { default as RemoteErrorNotice, classifyRemoteError } from './RemoteErrorNotice';
export type {
  ClassifiedRemoteError,
  RemoteErrorKind,
  RemoteErrorNoticeProps,
} from './RemoteErrorNotice';
export { useGuacamoleSession } from './useGuacamoleSession';
export type { GuacamoleSessionState, UseGuacamoleSessionOptions } from './useGuacamoleSession';
export { guacDecodeBase64, guacDecodeBase64ToBytes, guacEncodeBase64 } from './base64';
export { protocolCapabilities, REMOTE_PROTOCOL_LABEL } from './types';
export type {
  ProtocolCapabilities,
  RemotePhaseName,
  RemoteProtocol,
  RemoteSessionParams,
  RemoteTicket,
  TicketClient,
} from './types';
