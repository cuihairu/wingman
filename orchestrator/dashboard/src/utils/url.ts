// Utility helpers for building absolute URLs for assets.
// Priority: window.WINGMAN_SERVER_ORIGIN > process.env.WINGMAN_SERVER_ORIGIN > current origin.

export function getServerOrigin(): string {
  if (typeof window !== 'undefined' && (window as any).WINGMAN_SERVER_ORIGIN) {
    return (window as any).WINGMAN_SERVER_ORIGIN as string;
  }

  const fromEnv = (process as any)?.env?.WINGMAN_SERVER_ORIGIN as string | undefined;
  if (fromEnv) return fromEnv;

  if (typeof window !== 'undefined' && window.location?.origin) {
    return window.location.origin;
  }

  return 'http://127.0.0.1:9527';
}

export function isAbsoluteUrl(u?: string): boolean {
  if (!u) return false;
  return /^(https?:|data:|blob:)/i.test(u);
}

export function assetURL(u?: string): string {
  if (!u) return '';
  if (isAbsoluteUrl(u)) return u;
  const base = getServerOrigin().replace(/\/$/, '');
  if (u.startsWith('/')) return base + u;
  return base + '/' + u;
}
