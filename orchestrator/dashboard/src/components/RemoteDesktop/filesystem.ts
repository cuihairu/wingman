/**
 * 远端文件系统操作助手（公共件，设计 §15 SSH/SFTP 树第一版）。
 *
 * 全部经 Guacamole 对象流协议（get/put）与 guacd 通信，网关纯透传——
 * 这里是浏览器侧唯一的协议编排点，cockpit 复用同一份（§7 第 3 条：
 * 协议细节禁止复制分叉）。线协议 1.5.x 没有删除/重命名指令，故只有
 * 列目录/下载/上传三种操作（见 types.ts RemoteFileSystemObject 注）。
 *
 * ack 纪律与剪贴板/文件下载一致：每个 blob 都必须 ack，否则 guacd 停发。
 */
import Guacamole from 'guacamole-common-js';
import type { OutputStream } from 'guacamole-common-js';
import { guacDecodeBase64, guacDecodeBase64ToBytes } from './base64';
import type { RemoteFileEntry, RemoteFileSystemObject } from './types';

/** 拼接对象内绝对路径（根目录不留双斜杠）。 */
export function joinRemotePath(directory: string, name: string): string {
  const dir = directory.endsWith('/') && directory !== '/' ? directory.slice(0, -1) : directory;
  return dir === '/' ? `/${name}` : `${dir}/${name}`;
}

/** 取路径末段作文件名（下载落盘名）。 */
export function remoteBaseName(path: string): string {
  const trimmed = path.endsWith('/') && path !== '/' ? path.slice(0, -1) : path;
  const idx = trimmed.lastIndexOf('/');
  return idx >= 0 ? trimmed.slice(idx + 1) : trimmed;
}

/** 传输进度回调：current 已传输字节；total 已知时给总字节（下载侧由调用方按 entry.size 补）。 */
export type RemoteProgressCallback = (current: number, total?: number) => void;

/** 大目录列取护栏（解码后字符数；异常目录不打爆内存，超限停 ack 靠流控止住）。 */
export const DEFAULT_LIST_MAX_BYTES = 8 * 1024 * 1024;

/**
 * 列目录：请求 name = 目录路径 的输入流，body 是 UTF-8 JSON 数组
 * （[{name, directory, mimetype, size}]，guacd SFTP 契约）。协议一次 get
 * 返回整个目录体（无服务端分页），分页是浏览器侧 UI 行为；此处只做
 * 大小护栏（maxBytes，超限拒绝并停止 ack——guacd 依 ack 纪律停发）。
 */
export async function listRemoteDirectory(
  fs: RemoteFileSystemObject,
  path: string,
  opts: { maxBytes?: number } = {},
): Promise<RemoteFileEntry[]> {
  const maxBytes = opts.maxBytes ?? DEFAULT_LIST_MAX_BYTES;
  return new Promise<RemoteFileEntry[]>((resolve, reject) => {
    fs.requestInputStream(path, (stream) => {
      let text = '';
      let settled = false;
      stream.onblob = (data) => {
        text += guacDecodeBase64(data);
        if (text.length > maxBytes) {
          if (!settled) {
            settled = true;
            reject(new Error('目录内容过大（超过安全上限）'));
          }
          return; // 不 ack → guacd 停发（流控），内存不再增长
        }
        stream.sendAck('ok', Guacamole.Status.Code.SUCCESS);
      };
      stream.onend = () => {
        if (settled) {
          return;
        }
        settled = true;
        try {
          const parsed: unknown = JSON.parse(text);
          if (!Array.isArray(parsed)) {
            reject(new Error('目录内容格式异常（非数组）'));
            return;
          }
          resolve(parsed as RemoteFileEntry[]);
        } catch {
          reject(new Error('目录内容解析失败（非 JSON）'));
        }
      };
    });
  });
}

/**
 * 下载文件：请求 name = 文件路径 的输入流，聚合为 Blob；逐块上报进度。
 * 下载侧协议没有错误 ack 可判（get 的 ack 由 common-js 输入侧处理），失败
 * 检测靠无进展看门狗（stallTimeoutMs，默认 60s，0 关闭）——停滞即 reject，
 * 上层可重试。
 */
export async function downloadRemoteFile(
  fs: RemoteFileSystemObject,
  path: string,
  opts: { onProgress?: RemoteProgressCallback; stallTimeoutMs?: number } = {},
): Promise<{ filename: string; blob: Blob }> {
  return new Promise<{ filename: string; blob: Blob }>((resolve, reject) => {
    fs.requestInputStream(path, (stream, mimetype) => {
      const chunks: BlobPart[] = [];
      let received = 0;
      let settled = false;
      let timer: ReturnType<typeof setTimeout> | null = null;
      const settle = (ok: boolean, err?: Error) => {
        if (settled) {
          return;
        }
        settled = true;
        if (timer) {
          clearTimeout(timer);
          timer = null;
        }
        if (ok) {
          resolve({
            filename: remoteBaseName(path) || 'remote-file',
            blob: new Blob(chunks, { type: mimetype || 'application/octet-stream' }),
          });
        } else {
          reject(err ?? new Error('下载失败'));
        }
      };
      const stallMs = opts.stallTimeoutMs ?? 60_000;
      const arm = () => {
        if (stallMs <= 0) {
          return;
        }
        if (timer) {
          clearTimeout(timer);
        }
        timer = setTimeout(() => {
          settle(false, new Error(`下载超时（${Math.round(stallMs / 1000)} 秒无进展回应）`));
        }, stallMs);
      };

      stream.onblob = (data) => {
        const bytes = guacDecodeBase64ToBytes(data);
        chunks.push(bytes);
        received += bytes.byteLength;
        stream.sendAck('ok', Guacamole.Status.Code.SUCCESS);
        opts.onProgress?.(received);
        arm();
      };
      stream.onend = () => settle(true);
      arm();
    });
  });
}

/**
 * 上传文件到指定目录（createOutputStream → put，写在该目录下同名路径）。
 * BlobWriter 分块；完成回调里手动 sendEnd（1.5.0 BlobWriter 无 sendFile）。
 *
 * 失败判定：1.5.0 对错误 ack 只停发不报错（onerror 仅本地读文件失败触发，
 * dist/esm 实测），失败必须从 onack（status.code !== 0）判定；再叠一个
 * 无进展看门狗（stallTimeoutMs，默认 60s，0 关闭）兜底协议静默。
 */
export async function uploadRemoteFile(
  fs: RemoteFileSystemObject,
  file: File,
  directory: string,
  opts: { onProgress?: RemoteProgressCallback; stallTimeoutMs?: number } = {},
): Promise<void> {
  return new Promise<void>((resolve, reject) => {
    const stream = fs.createOutputStream(
      file.type || 'application/octet-stream',
      joinRemotePath(directory, file.name),
    );
    // 结构面 RemoteStreamOut 与真实 OutputStream 只差 index 元数据；
    // BlobWriter 只透传 sendBlob/sendEnd（不读 index），在此收口转换。
    const writer = new Guacamole.BlobWriter(stream as unknown as OutputStream);

    let settled = false;
    let timer: ReturnType<typeof setTimeout> | null = null;
    const settle = (ok: boolean, err?: Error) => {
      if (settled) {
        return;
      }
      settled = true;
      if (timer) {
        clearTimeout(timer);
        timer = null;
      }
      if (ok) {
        resolve();
      } else {
        reject(err ?? new Error(`${file.name} 上传失败`));
      }
    };
    // 看门狗量的是「无进展时长」而非总时长：每次 ack/进度推进都重置，
    // 大文件传输不受总时长限制
    const stallMs = opts.stallTimeoutMs ?? 60_000;
    const arm = () => {
      if (stallMs <= 0) {
        return;
      }
      if (timer) {
        clearTimeout(timer);
      }
      timer = setTimeout(() => {
        settle(
          false,
          new Error(`${file.name} 上传超时（${Math.round(stallMs / 1000)} 秒无进展回应）`),
        );
      }, stallMs);
    };

    writer.onack = (status) => {
      if (status.code !== 0) {
        settle(
          false,
          new Error(`${file.name} 上传失败（协议错误码 0x${status.code.toString(16)}）`),
        );
        return;
      }
      arm();
    };
    writer.onprogress = (_blob: Blob, offset: number) => {
      arm();
      opts.onProgress?.(offset, file.size);
    };
    writer.oncomplete = () => {
      stream.sendEnd();
      settle(true);
    };
    writer.onerror = () => settle(false, new Error(`${file.name} 上传失败（本地读取失败）`));
    arm();
    writer.sendBlob(file);
  });
}

/**
 * 有限重试： attempts 总尝试次数（默认 2 = 失败重试 1 次），第 n 次重试前
 * 延迟 baseDelayMs*n（默认基数 300ms）。返回实际尝试次数供审计上报。
 * sleep 可注入（测试免真实等待）。
 */
export async function withRetry<T>(
  op: () => Promise<T>,
  opts: { attempts?: number; baseDelayMs?: number; sleep?: (ms: number) => Promise<void> } = {},
): Promise<{ value: T; attempts: number }> {
  const attempts = Math.max(1, opts.attempts ?? 2);
  const sleep =
    opts.sleep ??
    ((ms: number) =>
      new Promise<void>((r) => {
        setTimeout(r, ms);
      }));
  let lastErr: unknown;
  for (let attempt = 1; attempt <= attempts; attempt += 1) {
    try {
      return { value: await op(), attempts: attempt };
    } catch (err) {
      lastErr = err;
      if (attempt < attempts) {
        await sleep((opts.baseDelayMs ?? 300) * attempt);
      }
    }
  }
  throw lastErr;
}

/** 按字节人类可读（浏览器树里直接展示；目录不显示大小）。 */
export function formatRemoteSize(bytes: number): string {
  if (!Number.isFinite(bytes) || bytes <= 0) {
    return '-';
  }
  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  let value = bytes;
  let unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit += 1;
  }
  return `${value >= 100 || unit === 0 ? Math.round(value) : value.toFixed(1)} ${units[unit]}`;
}
