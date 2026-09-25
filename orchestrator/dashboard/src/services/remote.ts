/**
 * 远程桌面（Guacamole 像素面）服务
 * @description 票据申请 + WS 隧道参数 + 会话录像检索。后端见
 *              orchestrator/server internal/handlers/guacamole.go 与
 *              recordings.go；设计见 docs/remote-gateway-guacamole-design.md。
 *
 * 连接协议/参数类型由公共件 `@/components/RemoteDesktop/types` 唯一定义
 * （设计 §7 第 3 条不允许第二方复制分叉），本模块只负责「与本仓 Go server
 * 说话」的具体端点绑定。
 */

import { request } from '@umijs/max';
import type {
  RemoteProtocol,
  RemoteSessionParams,
  RemoteTicket,
} from '@/components/RemoteDesktop/types';
import type { ApiResponse } from './wingman';

/** @deprecated 票据申请入参请用公共件的 RemoteSessionParams */
export type RemoteTicketParams = RemoteSessionParams;

/** @deprecated 协议枚举请用公共件的 RemoteProtocol */
export type { RemoteProtocol };

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

/**
 * Guacamole WS 隧道地址（票据即凭证，路径与后端路由对齐）。
 *
 * loc 可注入：https 页面必须升级为 wss（否则 TLS 页面会把像素面隧道降级成
 * 明文）。jsdom 无法改写 location.protocol，故留参数便于单测覆盖该分支，
 * 也让 SSR/测试环境能显式指定来源。
 */
export function guacamoleWSPath(
  ticket: string,
  loc: Pick<Location, 'protocol' | 'host'> = location,
): string {
  const proto = loc.protocol === 'https:' ? 'wss' : 'ws';
  return `${proto}://${loc.host}/api/remote/guacamole?ticket=${encodeURIComponent(ticket)}`;
}

// ---------- 会话录像检索（设计 §16：desktop:view 列/下载，desktop:control 删） ----------

export interface RecordingEntry {
  /** 录像文件名（{agentID}-{sessionID}.mjs，网关侧生成） */
  name: string;
  sizeBytes: number;
  /** RFC3339（UTC）修改时间 */
  modifiedAt: string;
}

/** 列出会话录像（按修改时间倒序）；录制未配置时后端返回 501 + 指引 */
export async function listRecordings(): Promise<RecordingEntry[]> {
  const res = await request<ApiResponse<RecordingEntry[]>>('/api/remote/recordings');
  if (!res?.success) {
    // 501 的 error/hint 直抛，由调用方展示配置指引
    throw new Error(res?.error || '获取会话录像列表失败');
  }
  return res.data || [];
}

/**
 * 下载会话录像到浏览器（.mjs 为 Guacamole session 格式，
 * 可用官方 guacenc 离线转 mp4）。鉴权走 request 拦截器的
 * Bearer 头，不能用裸 <a href>（不带 token）。
 */
export async function downloadRecording(name: string): Promise<void> {
  const res = await request<Blob>(`/api/remote/recordings/${encodeURIComponent(name)}/download`, {
    responseType: 'blob',
  });
  const blob = res instanceof Blob ? res : new Blob([res as unknown as BlobPart]);
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = name;
  a.click();
  URL.revokeObjectURL(url);
}

/** 删除会话录像（desktop:control；服务端记 desktop.recording_delete 审计） */
export async function deleteRecording(name: string): Promise<void> {
  const res = await request<ApiResponse<null>>(
    `/api/remote/recordings/${encodeURIComponent(name)}`,
    { method: 'DELETE' },
  );
  if (!res?.success) {
    throw new Error(res?.error || '删除会话录像失败');
  }
}
