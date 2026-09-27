/**
 * RTL 异步等待窗口（waitFor/findBy 的 asyncUtilTimeout）按 CPU 超售
 * 程度等比放大。单一来源：tests/setupRTL.jsx 的全局 configure 与
 * jest.config.ts 的 testTimeout、各测试文件的显式 waitFor 超时都从这里取值。
 *
 * 背景：固定 wall-clock 窗口（RTL 默认 1s、此前的固定 5s）按「独占机器」
 * 标定；共享机上全量 jest（13 个 worker）叠加外部负载把 14 核压到 6~7 倍
 * 超售时，任何按独占标定的窗口都会在「断言条件正确、只是机器慢」时假红
 * （实测 load ≈ 89 全量 4 套件 11 用例假红；同负载单跑 --runInBand 8/8
 * 全绿，纯时序非逻辑回归）。
 *
 * 口径：窗口 = 基准 5s × 超售系数（1 分钟 loadavg / 核数，向上取整，
 * 下限 1），封顶 60s。空闲机器与 CI runner 系数为 1，行为与固定 5s
 * 完全一致；只有真超售的机器才放大，且放大的只是「等条件」的上界——
 * 条件满足即返回，绿路径零额外耗时。
 */
import os from 'os';

const BASE_WINDOW_MS = 5000;
const CAP_MS = 60000;

function cpuStarvationFactor(): number {
  const cores = Math.max(1, os.cpus().length);
  return Math.max(1, Math.ceil(os.loadavg()[0] / cores));
}

export const RTL_ASYNC_UTIL_TIMEOUT = Math.min(BASE_WINDOW_MS * cpuStarvationFactor(), CAP_MS);
