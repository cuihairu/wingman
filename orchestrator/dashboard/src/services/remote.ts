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
  RemoteFileOpAudit,
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
 * 拉取会话录像内容为 Blob（§16 回放面板用；鉴权走 request 拦截器的
 * Bearer 头，不能用裸 <a href>——不带 token）。
 */
export async function fetchRecordingBlob(name: string): Promise<Blob> {
  const res = await request<Blob>(`/api/remote/recordings/${encodeURIComponent(name)}/download`, {
    responseType: 'blob',
  });
  return res instanceof Blob ? res : new Blob([res as unknown as BlobPart]);
}

/**
 * 下载会话录像到浏览器（.mjs 为 Guacamole session 格式，
 * 可用官方 guacenc 离线转 mp4；也可直接在浏览器内回放——
 * RemoteRecordingPlayer 走 fetchRecordingBlob）。
 */
export async function downloadRecording(name: string): Promise<void> {
  const blob = await fetchRecordingBlob(name);
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

// ---------- 会话审计报表（设计 §11 P1：可聚合的会话终态视图） ----------

/** 一条已结束的远程桌面会话（不含凭证与画面内容） */
export interface RemoteSessionEntry {
  id: number;
  /** 网关会话 ID，与录像文件名同源可关联 */
  sessionId: string;
  agentId: string;
  operator: string;
  protocol: RemoteProtocol;
  host: string;
  port: number;
  /** true 监看 / false 接管 */
  readOnly: boolean;
  record: boolean;
  /** 录像文件名（record 时），可直接跳录像检索 */
  recordingName?: string;
  /** closed=正常断开 / failed=建连失败 */
  status: 'closed' | 'failed';
  failReason?: string;
  startedAt: string;
  endedAt?: string;
  durationMs: number;
}

/** 区间汇总（不受分页影响） */
export interface RemoteSessionSummary {
  total: number;
  closed: number;
  failed: number;
  recorded: number;
  control: number;
  viewOnly: number;
  totalMsSum: number;
}

/** 维度聚合行 */
export interface RemoteSessionGroup {
  key: string;
  count: number;
  msSum: number;
  failed: number;
}

/** 时间趋势桶 */
export interface RemoteSessionBucket {
  bucket: string;
  count: number;
  failed: number;
  msSum: number;
}

/** 报表查询参数（全部可选，空值即不筛选） */
export interface RemoteSessionQuery {
  page?: number;
  size?: number;
  agentId?: string;
  protocol?: RemoteProtocol;
  operator?: string;
  status?: 'closed' | 'failed';
  /** view=监看 / control=接管 */
  mode?: 'view' | 'control';
  record?: boolean;
  /** RFC3339 */
  start?: string;
  end?: string;
  /** 分组维度，默认 protocol */
  groupBy?: 'protocol' | 'operator' | 'agentId';
  /** 时间桶粒度，默认 day */
  bucket?: 'hour' | 'day' | 'week' | 'month';
}

export interface RemoteSessionReport {
  data: RemoteSessionEntry[];
  total: number;
  page: number;
  size: number;
  summary: RemoteSessionSummary;
  groups: RemoteSessionGroup[];
  buckets: RemoteSessionBucket[];
}

/**
 * 查询远程桌面会话审计报表。权限 desktop:view（报表只读，不含接管能力）。
 *
 * 一次请求同时返回列表 + 汇总 + 维度聚合 + 时间趋势：报表的四块视图口径
 * 必须一致，分成四个端点迟早出现「列表 12 条、汇总 13 条」的对不上。
 */
export async function listRemoteSessions(
  query: RemoteSessionQuery = {},
): Promise<RemoteSessionReport> {
  const params = new URLSearchParams();
  Object.entries(query).forEach(([key, value]) => {
    if (value !== undefined && value !== null && value !== '') {
      params.set(key, String(value));
    }
  });
  const qs = params.toString();
  const res = await request<ApiResponse<RemoteSessionReport>>(
    `/api/remote/sessions${qs ? `?${qs}` : ''}`,
  );
  if (!res?.success) {
    throw new Error(res?.error || '获取会话审计报表失败');
  }
  const payload = res.data;
  return {
    data: payload?.data || [],
    total: payload?.total || 0,
    page: payload?.page || 1,
    size: payload?.size || 20,
    summary: payload?.summary || {
      total: 0,
      closed: 0,
      failed: 0,
      recorded: 0,
      control: 0,
      viewOnly: 0,
      totalMsSum: 0,
    },
    groups: payload?.groups || [],
    buckets: payload?.buckets || [],
  };
}

// ---------- 文件操作审计上报（设计 §15.1 第二版） ----------

/**
 * 上报 SFTP 浏览器文件操作（下载/上传）的最终结果，供服务端落审计。
 * best-effort：网络失败不打断操作主流程（调用方 fire-and-forget 吞错）。
 * 服务端按 ticket 反解会话/agent/操作者，请求体同类字段不采信；list 高频
 * 不审计（与 agents 列表同策略）。
 */
export async function reportRemoteFileOp(op: RemoteFileOpAudit): Promise<void> {
  await request<ApiResponse<null>>('/api/remote/file-ops', {
    method: 'POST',
    data: op,
  });
}
