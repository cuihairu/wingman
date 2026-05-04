/**
 * Wingman Client 类型定义
 */

/**
 * 响应数据结构
 */
export interface Response<T = any> {
  success: boolean;
  data?: T;
  error?: string;
  requestId?: number;
}

/**
 * 请求消息
 */
export interface RequestMessage {
  id: number;
  method: string;
  params: Record<string, any>;
}

/**
 * 响应消息
 */
export interface ResponseMessage {
  id: number;
  success: boolean;
  data?: any;
  error?: string;
}

/**
 * 事件消息
 */
export interface EventMessage {
  event: string;
  data?: any;
}

/**
 * 屏幕尺寸
 */
export interface Size {
  width: number;
  height: number;
}

/**
 * 点坐标
 */
export interface Point {
  x: number;
  y: number;
}

/**
 * 颜色
 */
export interface Color {
  r: number;
  g: number;
  b: number;
  a?: number;
}

/**
 * 矩形区域
 */
export interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

/**
 * 窗口边界
 */
export interface Bounds {
  x: number;
  y: number;
  width: number;
  height: number;
}

/**
 * CPU 信息
 */
export interface CpuInfo {
  vendor: string;
  brand: string;
  cores: number;
  frequency: number;
}

/**
 * 内存信息
 */
export interface MemoryInfo {
  total: number;
  available: number;
  used: number;
  free: number;
}

/**
 * 磁盘信息
 */
export interface DiskInfo {
  path: string;
  total: number;
  free: number;
  used: number;
}

/**
 * GPU 信息
 */
export interface GpuInfo {
  name: string;
  vendor: string;
  memory: number;
}

/**
 * 操作系统信息
 */
export interface OsInfo {
  platform: string;
  version: string;
  architecture: string;
  hostname: string;
}

/**
 * 网络适配器
 */
export interface NetworkAdapter {
  name: string;
  description: string;
  mac: string;
  ipv4: string;
  ipv6?: string;
}

/**
 * 显示器信息
 */
export interface DisplayInfo {
  name: string;
  width: number;
  height: number;
  refreshRate: number;
  isPrimary: boolean;
}

/**
 * HTTP 响应
 */
export interface HttpResponse {
  status: number;
  success: boolean;
  body: string;
  headers?: Record<string, string>;
}

/**
 * 客户端配置
 */
export interface ClientConfig {
  host?: string;
  port?: number;
  timeout?: number;
  autoReconnect?: boolean;
  reconnectInterval?: number;
}

/**
 * 事件处理器
 */
export type EventHandler = (data: any) => void;
