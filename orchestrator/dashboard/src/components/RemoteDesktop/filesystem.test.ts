/**
 * 远端文件系统操作助手测试（设计 §15 SSH/SFTP 树，第二版含进度/重试/护栏）。
 *
 * guacamole-common-js 全程 mock（与 index.test.tsx 同款工厂）；文件系统
 * 对象用 types.ts 的结构化最小面直接伪造——不触碰 Guacamole 类。
 * 重点锁协议编排的不变式：
 *   1. 每个 blob 都必须 ack（不 ack guacd 停发）；
 *   2. 目录体是 UTF-8 JSON 数组（guacd SFTP 契约），坏载荷显式报错；
 *   3. 上传走 createOutputStream（put）且 BlobWriter 完成后手动 sendEnd；
 *   4. 上传失败必须从 onack（错误 ack）判定——1.5.0 对错误 ack 停发不报错
 *      （onerror 仅本地读失败触发），失败时 Promise 不许悬挂；
 *   5. 看门狗兜底协议静默；重试按序退避并回报实际尝试次数。
 */
import Guacamole from 'guacamole-common-js';
import {
  downloadRemoteFile,
  formatRemoteSize,
  joinRemotePath,
  listRemoteDirectory,
  remoteBaseName,
  uploadRemoteFile,
  withRetry,
} from './filesystem';
import type { RemoteFileSystemObject, RemoteStreamIn } from './types';

jest.mock('guacamole-common-js', () => ({
  __esModule: true,
  default: {
    BlobWriter: jest.fn().mockImplementation((stream: unknown) => ({
      sendBlob: jest.fn(),
      oncomplete: null,
      onerror: null,
      onprogress: null,
      onack: null,
      stream,
    })),
    Status: { Code: { SUCCESS: 0x0000, UNSUPPORTED: 0x0100 } },
  },
}));

const MockedBlobWriter = Guacamole.BlobWriter as unknown as jest.Mock;

beforeEach(() => {
  MockedBlobWriter.mockClear();
});

/** 假输入流：捕获注册的 onblob/onend，供逐块投喂（setter 转发闭包） */
function fakeInputStream() {
  let blobHandler: (data: string) => void = () => {};
  let endHandler: () => void = () => {};
  const stream: RemoteStreamIn = {
    sendAck: jest.fn(),
    set onblob(fn: (d: string) => void) {
      blobHandler = fn;
    },
    get onblob() {
      return blobHandler;
    },
    set onend(fn: () => void) {
      endHandler = fn;
    },
    get onend() {
      return endHandler;
    },
  };
  return {
    stream,
    emitBlob: (d: string) => blobHandler(d),
    emitEnd: () => endHandler(),
  };
}

/** 假文件系统对象：记录 requestInputStream/createOutputStream 调用 */
function fakeFs() {
  const requests: Array<{ name: string; cb: (stream: RemoteStreamIn, mimetype: string) => void }> =
    [];
  const puts: Array<{ mimetype: string; name: string; stream: unknown }> = [];
  const fs: RemoteFileSystemObject = {
    index: 0,
    requestInputStream: jest.fn(
      (name: string, cb: (stream: RemoteStreamIn, mimetype: string) => void) => {
        requests.push({ name, cb });
      },
    ),
    createOutputStream: jest.fn((mimetype: string, name: string) => {
      const stream = { sendBlob: jest.fn(), sendEnd: jest.fn(), onack: undefined };
      puts.push({ mimetype, name, stream });
      return stream;
    }),
  };
  return { fs, requests, puts };
}

describe('路径助手', () => {
  it('joinRemotePath：根目录不产生双斜杠，尾斜杠归一', () => {
    expect(joinRemotePath('/', 'a.txt')).toBe('/a.txt');
    expect(joinRemotePath('/home', 'u')).toBe('/home/u');
    expect(joinRemotePath('/home/', 'u')).toBe('/home/u');
    expect(joinRemotePath('/', 'a/b')).toBe('/a/b');
  });

  it('remoteBaseName：取路径末段，根目录为空串', () => {
    expect(remoteBaseName('/home/u/report.txt')).toBe('report.txt');
    expect(remoteBaseName('report.txt')).toBe('report.txt');
    expect(remoteBaseName('/')).toBe('');
    expect(remoteBaseName('/home/')).toBe('home');
  });
});

describe('formatRemoteSize', () => {
  it('非正数与非有限值显示占位符', () => {
    expect(formatRemoteSize(0)).toBe('-');
    expect(formatRemoteSize(-5)).toBe('-');
    expect(formatRemoteSize(Number.NaN)).toBe('-');
  });
  it('B/KB/MB 进位与舍入口径', () => {
    expect(formatRemoteSize(512)).toBe('512 B');
    expect(formatRemoteSize(1500)).toBe('1.5 KB');
    expect(formatRemoteSize(512000)).toBe('500 KB'); // ≥100 取整
    expect(formatRemoteSize(1048576)).toBe('1.0 MB');
    expect(formatRemoteSize(5 * 1024 ** 4)).toBe('5.0 TB'); // TB 封顶不再进位
  });
});

describe('listRemoteDirectory', () => {
  it('目录体分块聚合后解析为条目数组，逐块 ack', async () => {
    const { fs, requests } = fakeFs();
    const pending = listRemoteDirectory(fs, '/');
    const stream = fakeInputStream();
    requests[0].cb(stream.stream, 'text/json');

    // 目录体故意拆成三块（JSON 跨 blob 到达）：聚合后必须完整可解析
    stream.emitBlob(btoa('[{"name":"a","directory":true,"mimetype":"","size":0},'));
    stream.emitBlob(btoa('{"name":"b.txt","directory":false,"mimetype":"text/plain","size":3}'));
    stream.emitBlob(btoa(']'));
    stream.emitEnd();

    await expect(pending).resolves.toEqual([
      { name: 'a', directory: true, mimetype: '', size: 0 },
      { name: 'b.txt', directory: false, mimetype: 'text/plain', size: 3 },
    ]);
    expect(stream.stream.sendAck).toHaveBeenCalledTimes(3);
    expect(fs.requestInputStream).toHaveBeenCalledWith('/', expect.any(Function));
  });

  it('目录体不是 JSON 时报错', async () => {
    const { fs, requests } = fakeFs();
    const pending = listRemoteDirectory(fs, '/x');
    const stream = fakeInputStream();
    requests[0].cb(stream.stream, 'text/json');
    stream.emitBlob(btoa('not json'));
    stream.emitEnd();
    await expect(pending).rejects.toThrow('目录内容解析失败');
  });

  it('目录体是合法 JSON 但非数组时报错', async () => {
    const { fs, requests } = fakeFs();
    const pending = listRemoteDirectory(fs, '/x');
    const stream = fakeInputStream();
    requests[0].cb(stream.stream, 'text/json');
    stream.emitBlob(btoa('{"name":"a"}'));
    stream.emitEnd();
    await expect(pending).rejects.toThrow('非数组');
  });

  it('大目录护栏：超限 reject 且停止 ack（流控止住 guacd）', async () => {
    const { fs, requests } = fakeFs();
    const pending = listRemoteDirectory(fs, '/huge', { maxBytes: 16 });
    const stream = fakeInputStream();
    requests[0].cb(stream.stream, 'text/json');
    stream.emitBlob(btoa('{"name":"a"}')); // 14 字节，未超限 → ack
    stream.emitBlob(btoa('{"name":"bbbbbb"}')); // 累计超限 → 拒绝且不 ack
    await expect(pending).rejects.toThrow('目录内容过大');
    expect(stream.stream.sendAck).toHaveBeenCalledTimes(1);
    // 迟到的 end 不改变已拒绝的结果
    stream.emitEnd();
    await expect(pending).rejects.toThrow('目录内容过大');
  });
});

describe('downloadRemoteFile', () => {
  it('按字节聚合为 Blob，文件名取路径末段', async () => {
    const { fs, requests } = fakeFs();
    const pending = downloadRemoteFile(fs, '/home/u/dump.bin');
    const stream = fakeInputStream();
    requests[0].cb(stream.stream, 'application/octet-stream');

    stream.emitBlob(btoa('AB')); // 65,66
    stream.emitEnd();

    const { filename, blob } = await pending;
    expect(filename).toBe('dump.bin');
    expect(blob.type).toBe('application/octet-stream');
    expect(blob.size).toBe(2);
  });

  it('mimetype 缺省回退 application/octet-stream；空路径名回退固定名', async () => {
    const { fs, requests } = fakeFs();
    const pending = downloadRemoteFile(fs, '/');
    const stream = fakeInputStream();
    requests[0].cb(stream.stream, '');
    stream.emitEnd();
    const { filename, blob } = await pending;
    expect(filename).toBe('remote-file');
    expect(blob.type).toBe('application/octet-stream');
  });

  it('逐块上报已收字节进度', async () => {
    const { fs, requests } = fakeFs();
    const onProgress = jest.fn();
    const pending = downloadRemoteFile(fs, '/a.bin', { onProgress });
    const stream = fakeInputStream();
    requests[0].cb(stream.stream, 'application/octet-stream');
    stream.emitBlob(btoa('AB')); // 2 字节
    stream.emitBlob(btoa('CD')); // 累计 4 字节
    stream.emitEnd();
    await pending;
    expect(onProgress).toHaveBeenNthCalledWith(1, 2);
    expect(onProgress).toHaveBeenNthCalledWith(2, 4);
  });

  it('下载看门狗：停滞时 reject（上层可重试）', async () => {
    const { fs, requests } = fakeFs();
    const pending = downloadRemoteFile(fs, '/stall.bin', { stallTimeoutMs: 20 });
    const stream = fakeInputStream();
    requests[0].cb(stream.stream, 'application/octet-stream');
    await expect(pending).rejects.toThrow('下载超时');
  });
});

describe('uploadRemoteFile', () => {
  it('put 到「目录 + 文件名」路径，BlobWriter 完成后手动 sendEnd', async () => {
    const { fs, puts } = fakeFs();
    const file = new File(['hello'], 'a.txt', { type: 'text/plain' });
    const pending = uploadRemoteFile(fs, file, '/home/u');

    expect(fs.createOutputStream).toHaveBeenCalledWith('text/plain', '/home/u/a.txt');
    const writer = MockedBlobWriter.mock.results[0].value;
    writer.oncomplete();
    await expect(pending).resolves.toBeUndefined();
    expect((puts[0].stream as { sendEnd: jest.Mock }).sendEnd).toHaveBeenCalledTimes(1);
    expect(writer.sendBlob).toHaveBeenCalledWith(file);
  });

  it('BlobWriter 报错时 reject（错误文案含文件名）', async () => {
    const { fs } = fakeFs();
    const file = new File(['x'], 'bad.bin', { type: '' });
    const pending = uploadRemoteFile(fs, file, '/');
    const writer = MockedBlobWriter.mock.results[0].value;
    writer.onerror();
    await expect(pending).rejects.toThrow('bad.bin 上传失败');
    // mimetype 缺省回退
    expect(fs.createOutputStream).toHaveBeenCalledWith('application/octet-stream', '/bad.bin');
  });

  it('错误 ack 必须落定 Promise（1.5.0 停发不报错，失败从 onack 判定）', async () => {
    const { fs } = fakeFs();
    const file = new File(['hello'], 'denied.bin', { type: 'text/plain' });
    const pending = uploadRemoteFile(fs, file, '/');
    const writer = MockedBlobWriter.mock.results[0].value;
    writer.onack({ code: 0x0200, message: 'server refused' });
    await expect(pending).rejects.toThrow('denied.bin 上传失败（协议错误码 0x200）');
  });

  it('onprogress 逐 ack 上报进度（offset 与文件总字节）', async () => {
    const { fs } = fakeFs();
    const file = new File(['hello'], 'up.txt', { type: 'text/plain' });
    const onProgress = jest.fn();
    const pending = uploadRemoteFile(fs, file, '/', { onProgress });
    const writer = MockedBlobWriter.mock.results[0].value;
    writer.onprogress(new Blob(), 3);
    writer.onprogress(new Blob(), 5);
    expect(onProgress).toHaveBeenNthCalledWith(1, 3, 5);
    expect(onProgress).toHaveBeenNthCalledWith(2, 5, 5);
    writer.oncomplete();
    await expect(pending).resolves.toBeUndefined();
  });

  it('上传看门狗：无进展回应时 reject 兜底', async () => {
    const { fs } = fakeFs();
    const file = new File(['x'], 'slow.bin', { type: '' });
    const pending = uploadRemoteFile(fs, file, '/', { stallTimeoutMs: 20 });
    await expect(pending).rejects.toThrow('上传超时');
  });

  it('看门狗可关闭（stallTimeoutMs=0）且 settle 后不再计时', async () => {
    const { fs } = fakeFs();
    const file = new File(['x'], 'calm.bin', { type: '' });
    const pending = uploadRemoteFile(fs, file, '/', { stallTimeoutMs: 0 });
    const writer = MockedBlobWriter.mock.results[0].value;
    writer.oncomplete();
    await expect(pending).resolves.toBeUndefined();
    // 迟到的错误 ack 不改变已落定的结果（settle-once）
    writer.onack({ code: 0x0200, message: 'late' });
    await expect(pending).resolves.toBeUndefined();
  });
});

describe('withRetry', () => {
  it('首次成功不重试，attempts=1', async () => {
    const op = jest.fn().mockResolvedValue('ok');
    await expect(withRetry(op)).resolves.toEqual({ value: 'ok', attempts: 1 });
    expect(op).toHaveBeenCalledTimes(1);
  });

  it('失败后按序退避重试并回报实际尝试次数', async () => {
    const sleeps: number[] = [];
    const op = jest.fn().mockRejectedValueOnce(new Error('boom')).mockResolvedValueOnce('fine');
    const result = await withRetry(op, {
      attempts: 3,
      baseDelayMs: 100,
      sleep: async (ms) => {
        sleeps.push(ms);
      },
    });
    expect(result).toEqual({ value: 'fine', attempts: 2 });
    expect(sleeps).toEqual([100]);
    expect(op).toHaveBeenCalledTimes(2);
  });

  it('全部失败抛最后一次错误', async () => {
    const op = jest.fn().mockRejectedValue(new Error('still down'));
    await expect(withRetry(op, { attempts: 3, sleep: async () => undefined })).rejects.toThrow(
      'still down',
    );
    expect(op).toHaveBeenCalledTimes(3);
  });

  it('attempts 归一化（0/负数至少尝试 1 次）', async () => {
    const op = jest.fn().mockResolvedValue('v');
    await expect(withRetry(op, { attempts: 0 })).resolves.toEqual({ value: 'v', attempts: 1 });
  });
});
