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

/**
 * 列目录：请求 name = 目录路径 的输入流，body 是 UTF-8 JSON 数组
 * （[{name, directory, mimetype, size}]，guacd SFTP 契约）。
 */
export async function listRemoteDirectory(
  fs: RemoteFileSystemObject,
  path: string,
): Promise<RemoteFileEntry[]> {
  return new Promise<RemoteFileEntry[]>((resolve, reject) => {
    fs.requestInputStream(path, (stream) => {
      let text = '';
      stream.onblob = (data) => {
        text += guacDecodeBase64(data);
        stream.sendAck('ok', Guacamole.Status.Code.SUCCESS);
      };
      stream.onend = () => {
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

/** 下载文件：请求 name = 文件路径 的输入流，聚合为 Blob。 */
export async function downloadRemoteFile(
  fs: RemoteFileSystemObject,
  path: string,
): Promise<{ filename: string; blob: Blob }> {
  return new Promise<{ filename: string; blob: Blob }>((resolve, reject) => {
    fs.requestInputStream(path, (stream, mimetype) => {
      const chunks: BlobPart[] = [];
      stream.onblob = (data) => {
        chunks.push(guacDecodeBase64ToBytes(data));
        stream.sendAck('ok', Guacamole.Status.Code.SUCCESS);
      };
      stream.onend = () => {
        resolve({
          filename: remoteBaseName(path) || 'remote-file',
          blob: new Blob(chunks, { type: mimetype || 'application/octet-stream' }),
        });
      };
    });
  });
}

/**
 * 上传文件到指定目录（createOutputStream → put，写在该目录下同名路径）。
 * BlobWriter 分块；完成回调里手动 sendEnd（1.5.0 BlobWriter 无 sendFile）。
 */
export async function uploadRemoteFile(
  fs: RemoteFileSystemObject,
  file: File,
  directory: string,
): Promise<void> {
  return new Promise<void>((resolve, reject) => {
    const stream = fs.createOutputStream(
      file.type || 'application/octet-stream',
      joinRemotePath(directory, file.name),
    );
    // 结构面 RemoteStreamOut 与真实 OutputStream 只差 index 元数据；
    // BlobWriter 只透传 sendBlob/sendEnd（不读 index），在此收口转换。
    const writer = new Guacamole.BlobWriter(stream as unknown as OutputStream);
    writer.oncomplete = () => {
      stream.sendEnd();
      resolve();
    };
    writer.onerror = () => reject(new Error(`${file.name} 上传失败`));
    writer.sendBlob(file);
  });
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
