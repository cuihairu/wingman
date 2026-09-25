/**
 * Guacamole 流载荷 base64 编解码（公共件）。
 *
 * Guacamole 的 blob/clipboard 载荷是 base64 文本，但 `btoa`/`atob` 只吃
 * latin1——中文/emoji 会抛 InvalidCharacterError。经 UTF-8 字节中转是
 * 浏览器端唯一无依赖的正确做法（与 guacamole-common-js 内部
 * Guacamole.StringUtils 的处理同思路）。
 */

/** 文本 → base64（经 UTF-8 字节，绕过 btoa 的 latin1 限制） */
export function guacEncodeBase64(text: string): string {
  const bytes = new TextEncoder().encode(text);
  let binary = '';
  bytes.forEach((b) => {
    binary += String.fromCharCode(b);
  });
  return btoa(binary);
}

/** base64 → 文本（经 UTF-8 字节） */
export function guacDecodeBase64(data: string): string {
  const binary = atob(data);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) {
    bytes[i] = binary.charCodeAt(i);
  }
  return new TextDecoder().decode(bytes);
}

/** base64 → Uint8Array（文件下载聚合用：二进制不经文本中转） */
export function guacDecodeBase64ToBytes(data: string): Uint8Array<ArrayBuffer> {
  const binary = atob(data);
  // 显式 new ArrayBuffer 再包 Uint8Array：TS 5.7+ 的 TypedArray 泛型据此
  // 推断为 Uint8Array<ArrayBuffer>（BlobPart 的要求）；直接
  // new Uint8Array(len) 会落到 ArrayBufferLike（含 SharedArrayBuffer），
  // 传给 Blob 构造即类型报错。
  const bytes = new Uint8Array(new ArrayBuffer(binary.length));
  for (let i = 0; i < binary.length; i += 1) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}
