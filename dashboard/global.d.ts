/// <reference types="umi" />

declare module 'wingman-api';

declare namespace Wingman {
  interface Point {
    x: number;
    y: number;
  }

  interface Color {
    r: number;
    g: number;
    b: number;
    a?: number;
  }

  interface Rect {
    x: number;
    y: number;
    width: number;
    height: number;
  }

  interface WindowInfo {
    handle: number;
    title: string;
    bounds: Rect;
    isForeground: boolean;
  }

  interface ProcessInfo {
    pid: number;
    name: string;
    path: string;
  }

  interface ScriptInfo {
    name: string;
    path: string;
    status: 'stopped' | 'running' | 'paused';
    lastRun?: number;
  }

  interface ApiResponse<T = any> {
    success: boolean;
    data?: T;
    error?: string;
  }
}
