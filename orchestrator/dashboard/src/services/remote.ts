/**
 * 远程桌面（Guacamole 像素面）服务
 * @description 票据申请 + WS 隧道参数。后端见 orchestrator/server
 *              internal/handlers/guacamole.go；设计见
 *              docs/remote-gateway-guacamole-design.md。
 */

import { request } from '@umijs/max';
import type { ApiResponse } from './wingman';

/** 桌面协议（与后端 guacSupportedProtocol 对齐） */
export type RemoteProtocol = 'rdp' | 'vnc' | 'ssh';

export interface RemoteTicketParams {
  /** 目标 agent（host 取注册表上报 IP，前端不可指定地址） */
  agentId: string;
  protocol: RemoteProtocol;
  /** 协议默认端口（rdp 3389 / vnc 5900 / ssh 22），0 或缺省用默认 */
  port?: number;
  username?: string;
  password?: string;
  domain?: string;
  /** 只读监看（false 为接管，需 desktop:control 权限） */
  readOnly?: boolean;
  width?: number;
  height?: number;
}

export interface RemoteTicket {
  /** 一次性连接票据（5 分钟有效），经 WS query 传给网关 */
  ticket: string;
  expiresAt: string;
}

/**
 * 申请一次性桌面连接票据。鉴权走登录 token（desktop:view/
 * desktop:control 权限），票据本身即 WS 凭证。
 */
export async function createRemoteTicket(params: RemoteTicketParams): Promise<RemoteTicket> {
  const res = await request<ApiResponse<RemoteTicket>>('/api/remote/tickets', {
    method: 'POST',
    data: params,
  });
  if (!res?.success || !res.data?.ticket) {
    throw new Error(res?.error || '申请桌面连接票据失败');
  }
  return res.data;
}

/** Guacamole WS 隧道地址（票据即凭证，路径与后端路由对齐） */
export function guacamoleWSPath(ticket: string): string {
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  return `${proto}://${location.host}/api/remote/guacamole?ticket=${encodeURIComponent(ticket)}`;
}
