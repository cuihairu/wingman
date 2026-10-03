/**
 * 登录限流（服务端 RateLimitMiddleware，5 次失败/15 分钟 → 封禁 30 分钟）
 * 的 429 响应文案：用服务端返回的 retry_after_seconds 渲染「请 N 分钟后再试」，
 * 字段缺失时退化为不带时长的通用提示。requestErrorConfig（全局错误处理）与
 * 登录页（本地 catch 兜底）共用，保证两处文案一致。
 */
export function formatRateLimitMessage(retryAfterSeconds?: unknown): string {
  const sec = Number(retryAfterSeconds);
  if (Number.isFinite(sec) && sec > 0) {
    const minutes = Math.max(1, Math.ceil(sec / 60));
    return `尝试过于频繁，请 ${minutes} 分钟后再试`;
  }
  return '尝试过于频繁，请稍后再试';
}
