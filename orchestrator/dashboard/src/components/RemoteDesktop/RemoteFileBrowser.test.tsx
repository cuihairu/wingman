/**
 * 远端文件浏览器组件测试（设计 §15 SSH/SFTP 树第一版）。
 *
 * 文件系统对象全程用结构化假体（types.ts 最小面），Guacamole 只 mock
 * BlobWriter/Status（上传路径需要）。覆盖：列目录渲染与排序、目录导航
 * （进入/上一级/面包屑）、刷新、下载、上传（成功/失败/监看隐藏）、失败反馈。
 */
import { fireEvent, render, waitFor } from '@testing-library/react';
import React from 'react';
import Guacamole from 'guacamole-common-js';
import RemoteFileBrowser from './RemoteFileBrowser';
import type { RemoteFileEntry, RemoteFileSystemObject, RemoteStreamIn } from './types';

jest.mock('guacamole-common-js', () => ({
  __esModule: true,
  default: {
    BlobWriter: jest.fn().mockImplementation((stream: unknown) => ({
      sendBlob: jest.fn(),
      oncomplete: null,
      onerror: null,
      onprogress: null,
      stream,
    })),
    Status: { Code: { SUCCESS: 0x0000 } },
  },
}));

const MockedBlobWriter = Guacamole.BlobWriter as unknown as jest.Mock;

const WAIT_TIMEOUT = 5000;
const waitForUI: typeof waitFor = (callback, options) =>
  waitFor(callback, { timeout: WAIT_TIMEOUT, ...options });

/** 假输入流（目录体/文件内容投喂；setter 转发闭包，不能快照引用） */
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
  return { stream, emitBlob: (d: string) => blobHandler(d), emitEnd: () => endHandler() };
}

const ENTRIES: RemoteFileEntry[] = [
  { name: 'zed.txt', directory: false, mimetype: 'text/plain', size: 2048 },
  { name: 'logs', directory: true, mimetype: '', size: 0 },
  { name: 'abc.txt', directory: false, mimetype: 'text/plain', size: 10 },
];

/** 假 fs：requestInputStream 只记录请求，由用例按需 reply 回放目录体（路径感知） */
function fakeFs() {
  const requests: Array<{ name: string; cb: (s: RemoteStreamIn, m: string) => void }> = [];
  const fs: RemoteFileSystemObject = {
    index: 0,
    requestInputStream: jest.fn((name: string, cb: (s: RemoteStreamIn, m: string) => void) => {
      requests.push({ name, cb });
    }),
    createOutputStream: jest.fn(() => ({
      sendBlob: jest.fn(),
      sendEnd: jest.fn(),
      onack: undefined,
    })),
  };
  const reply = () => {
    const last = requests[requests.length - 1];
    const stream = fakeInputStream();
    last.cb(stream.stream, 'text/json');
    // /logs 下放一层子目录，供多级导航与面包屑中段跳转用
    const body =
      last.name === '/logs' ? [{ name: 'sub', directory: true, mimetype: '', size: 0 }] : ENTRIES;
    stream.emitBlob(btoa(JSON.stringify(body)));
    stream.emitEnd();
  };
  return { fs, requests, reply };
}

beforeEach(() => {
  // BlobWriter mock 是模块级共享的：清掉 results，让用例里 results[0]
  // 恒定指向本用例创建的 writer
  MockedBlobWriter.mockClear();
});

describe('RemoteFileBrowser', () => {
  it('挂载即列根目录，目录在前、名称升序，大小人类可读', async () => {
    const { fs, requests, reply } = fakeFs();
    const { getByText, findByText } = render(
      <RemoteFileBrowser fs={fs} readOnly={false} notify={jest.fn()} />,
    );
    expect(requests[0].name).toBe('/');
    reply();
    expect(await findByText('logs')).toBeTruthy();
    expect(getByText('2.0 KB')).toBeTruthy();
    // 行序：目录 logs 排最前，文件按名称升序（guacd 顺序不保证，排序在组件）
    const order = Array.from(document.body.querySelectorAll('td'))
      .map((td) => td.textContent)
      .filter((t) => t === 'logs' || t === 'abc.txt' || t === 'zed.txt');
    expect(order).toEqual(['logs', 'abc.txt', 'zed.txt']);
  });

  it('点击目录进入，上一级与面包屑均可回退/跳转', async () => {
    const { fs, requests, reply } = fakeFs();
    const { getByText, getByTitle, findByText } = render(
      <RemoteFileBrowser fs={fs} readOnly={false} notify={jest.fn()} />,
    );
    reply();
    // 根 → logs → logs/sub（两级深入）
    fireEvent.click(await findByText('logs'));
    await waitForUI(() => expect(requests[1]?.name).toBe('/logs'));
    reply();
    fireEvent.click(await findByText('sub'));
    await waitForUI(() => expect(requests[2]?.name).toBe('/logs/sub'));
    reply();
    // 面包屑中段（logs 为链接）跳回上一级目录
    fireEvent.click(getByText('logs'));
    await waitForUI(() => expect(requests[3]?.name).toBe('/logs'));
    reply();
    // 面包屑根段跳回根目录
    fireEvent.click(getByText('根目录'));
    await waitForUI(() => expect(requests[4]?.name).toBe('/'));
    reply();
    // 上一级按钮：进入 logs 后一键回根
    fireEvent.click(await findByText('logs'));
    await waitForUI(() => expect(requests[5]?.name).toBe('/logs'));
    reply();
    fireEvent.click(getByTitle('上一级'));
    await waitForUI(() => expect(requests[6]?.name).toBe('/'));
  });

  it('刷新按钮对当前目录重复请求', async () => {
    const { fs, requests, reply } = fakeFs();
    const { getByText, findByText } = render(
      <RemoteFileBrowser fs={fs} readOnly={false} notify={jest.fn()} />,
    );
    reply();
    await findByText('logs');
    fireEvent.click(getByText('刷新'));
    await waitForUI(() => expect(requests.length).toBe(2));
    expect(requests[1].name).toBe('/');
    reply();
  });

  it('下载文件：聚合 Blob 触发浏览器下载并提示', async () => {
    const urlMock = { createObjectURL: jest.fn(() => 'blob:mock'), revokeObjectURL: jest.fn() };
    Object.assign(URL, urlMock);
    const clicks: string[] = [];
    const origCreate = document.createElement.bind(document);
    const createSpy = jest.spyOn(document, 'createElement').mockImplementation((tag: string) => {
      if (tag === 'a') {
        const a = origCreate('a');
        Object.defineProperty(a, 'click', { value: () => clicks.push(a.download) });
        return a;
      }
      return origCreate(tag);
    });
    const notify = jest.fn();
    try {
      const { fs, requests, reply } = fakeFs();
      const { findByTestId, findByText } = render(
        <RemoteFileBrowser fs={fs} readOnly={false} notify={notify} />,
      );
      reply();
      fireEvent.click(await findByTestId('remote-fs-download-zed.txt'));
      // 下载请求按「当前目录 + 文件名」发出，应答文件内容
      await waitForUI(() => expect(requests[1]?.name).toBe('/zed.txt'));
      const stream = fakeInputStream();
      requests[1].cb(stream.stream, 'text/plain');
      stream.emitBlob(btoa('file-data'));
      stream.emitEnd();

      await waitForUI(() => expect(clicks).toContain('zed.txt'));
      expect(urlMock.createObjectURL).toHaveBeenCalled();
      expect(notify).toHaveBeenCalledWith('success', '已下载 zed.txt');
      expect(await findByText('2.0 KB')).toBeTruthy();
    } finally {
      createSpy.mockRestore();
    }
  });

  it('上传：选中文件 put 到当前目录，完成后提示并刷新', async () => {
    const { fs, requests, reply } = fakeFs();
    const notify = jest.fn();
    const { getByText, findByTestId, findByText } = render(
      <RemoteFileBrowser fs={fs} readOnly={false} notify={notify} />,
    );
    reply();
    await findByText('logs');

    const input = (await findByTestId('remote-fs-upload-input')) as HTMLInputElement;
    const clickSpy = jest.spyOn(input, 'click');
    fireEvent.click(getByText('上传到当前目录'));
    expect(clickSpy).toHaveBeenCalled();

    const file = new File(['up'], 'new.txt', { type: 'text/plain' });
    fireEvent.change(input, { target: { files: [file] } });
    await waitForUI(() =>
      expect(fs.createOutputStream).toHaveBeenCalledWith('text/plain', '/new.txt'),
    );
    const writer = MockedBlobWriter.mock.results[0].value;
    writer.oncomplete();
    await waitForUI(() => expect(notify).toHaveBeenCalledWith('success', 'new.txt 上传完成'));
    // 完成后自动刷新当前目录
    await waitForUI(() => expect(requests.length).toBe(2));
    reply();
  });

  it('上传失败：重试一次仍失败才报错，报错后仍刷新列表', async () => {
    const { fs, requests, reply } = fakeFs();
    const notify = jest.fn();
    const { findByTestId, findByText } = render(
      <RemoteFileBrowser fs={fs} readOnly={false} notify={notify} />,
    );
    reply();
    await findByText('logs');
    const input = (await findByTestId('remote-fs-upload-input')) as HTMLInputElement;
    const file = new File(['x'], 'bad.bin', { type: '' });
    fireEvent.change(input, { target: { files: [file] } });
    await waitForUI(() => expect(MockedBlobWriter).toHaveBeenCalledTimes(1));
    MockedBlobWriter.mock.results[0].value.onerror();
    // 有限重试：第一次失败后自动再试一次（第二次 put）
    await waitForUI(() => expect(MockedBlobWriter).toHaveBeenCalledTimes(2));
    MockedBlobWriter.mock.results[1].value.onerror();
    await waitForUI(() =>
      expect(notify).toHaveBeenCalledWith('error', 'bad.bin 上传失败（本地读取失败）'),
    );
    // 出错不中断流程：列表仍会重取
    await waitForUI(() => expect(requests.length).toBe(2));
    reply();
  });

  it('下载失败后重试成功：提示成功且审计回报 attempts=2', async () => {
    const urlMock = { createObjectURL: jest.fn(() => 'blob:mock'), revokeObjectURL: jest.fn() };
    Object.assign(URL, urlMock);
    const audit = jest.fn();
    let calls = 0;
    const fs: RemoteFileSystemObject = {
      index: 0,
      requestInputStream: jest.fn((name: string, cb: (s: RemoteStreamIn, m: string) => void) => {
        calls += 1;
        if (calls === 1) {
          // 首次列目录，渲染出下载入口
          const stream = fakeInputStream();
          cb(stream.stream, 'text/json');
          stream.emitBlob(btoa(JSON.stringify(ENTRIES)));
          stream.emitEnd();
          return;
        }
        if (calls === 2) {
          throw new Error('sftp hiccup'); // 下载第 1 次尝试失败
        }
        // 重试：正常回放文件流
        const stream = fakeInputStream();
        cb(stream.stream, 'text/plain');
        stream.emitBlob(btoa('data'));
        stream.emitEnd();
      }),
      createOutputStream: jest.fn(),
    };
    const notify = jest.fn();
    const { findByTestId } = render(
      <RemoteFileBrowser fs={fs} readOnly={false} notify={notify} audit={audit} />,
    );
    fireEvent.click(await findByTestId('remote-fs-download-zed.txt'));
    await waitForUI(() => expect(notify).toHaveBeenCalledWith('success', '已下载 zed.txt'));
    expect(audit).toHaveBeenCalledWith(
      expect.objectContaining({ action: 'download', path: '/zed.txt', result: 'ok', attempts: 2 }),
    );
  });

  it('下载成功走审计回调（含路径/大小/attempts），失败回报 error', async () => {
    const urlMock = { createObjectURL: jest.fn(() => 'blob:mock'), revokeObjectURL: jest.fn() };
    Object.assign(URL, urlMock);
    const audit = jest.fn();
    const notify = jest.fn();
    const { fs, requests, reply } = fakeFs();
    const { findByTestId, unmount } = render(
      <RemoteFileBrowser fs={fs} readOnly={false} notify={notify} audit={audit} />,
    );
    reply();
    fireEvent.click(await findByTestId('remote-fs-download-zed.txt'));
    await waitForUI(() => expect(requests[1]?.name).toBe('/zed.txt'));
    const stream = fakeInputStream();
    requests[1].cb(stream.stream, 'text/plain');
    stream.emitBlob(btoa('file-data'));
    stream.emitEnd();
    await waitForUI(() => expect(audit).toHaveBeenCalledTimes(1));
    expect(audit).toHaveBeenCalledWith({
      action: 'download',
      path: '/zed.txt',
      result: 'ok',
      sizeBytes: 9,
      attempts: 1,
    });
    unmount();

    // 失败（重试耗尽）：result=fail + error 文案
    const audit2 = jest.fn();
    let calls = 0;
    const fs2: RemoteFileSystemObject = {
      index: 0,
      requestInputStream: jest.fn((_n: string, _cb: (s: RemoteStreamIn, m: string) => void) => {
        calls += 1;
        if (calls === 1) {
          const stream = fakeInputStream();
          _cb(stream.stream, 'text/json');
          stream.emitBlob(btoa(JSON.stringify(ENTRIES)));
          stream.emitEnd();
          return;
        }
        throw new Error('sftp gone');
      }),
      createOutputStream: jest.fn(),
    };
    const { findByTestId: findByTestId2 } = render(
      <RemoteFileBrowser fs={fs2} readOnly={false} notify={notify} audit={audit2} />,
    );
    fireEvent.click(await findByTestId2('remote-fs-download-zed.txt'));
    await waitForUI(() => expect(notify).toHaveBeenCalledWith('error', 'sftp gone'));
    await waitForUI(() => expect(audit2).toHaveBeenCalledTimes(1));
    expect(audit2).toHaveBeenCalledWith({
      action: 'download',
      path: '/zed.txt',
      result: 'fail',
      sizeBytes: 2048,
      error: 'sftp gone',
    });
  });

  it('上传成功走审计回调；不传 audit 时浏览器照常工作', async () => {
    const { fs, requests, reply } = fakeFs();
    const audit = jest.fn();
    const notify = jest.fn();
    const { getByText, findByTestId, findByText } = render(
      <RemoteFileBrowser fs={fs} readOnly={false} notify={notify} audit={audit} />,
    );
    reply();
    await findByText('logs');
    const input = (await findByTestId('remote-fs-upload-input')) as HTMLInputElement;
    const file = new File(['up'], 'new.txt', { type: 'text/plain' });
    fireEvent.change(input, { target: { files: [file] } });
    await waitForUI(() => expect(fs.createOutputStream).toHaveBeenCalled());
    MockedBlobWriter.mock.results[0].value.oncomplete();
    await waitForUI(() => expect(notify).toHaveBeenCalledWith('success', 'new.txt 上传完成'));
    expect(audit).toHaveBeenCalledWith({
      action: 'upload',
      path: '/new.txt',
      result: 'ok',
      sizeBytes: 2,
      attempts: 1,
    });
    await waitForUI(() => expect(requests.length).toBe(2));
    reply();
  });

  it('浏览器侧分页：超过 pageSize 出现翻页，小列表隐藏', async () => {
    const { fs, reply } = fakeFs();
    const view1 = render(
      <RemoteFileBrowser fs={fs} readOnly={false} notify={jest.fn()} pageSize={2} />,
    );
    reply();
    expect(await view1.findByText('logs')).toBeTruthy();
    // 第 1 页：排序后 logs、abc.txt；zed.txt 在第 2 页
    expect(view1.queryByText('zed.txt')).toBeNull();
    expect(view1.getByText('共 3 项')).toBeTruthy();
    fireEvent.click(view1.getByTitle('2'));
    expect(await view1.findByText('zed.txt')).toBeTruthy();
    view1.unmount();

    // 默认 pageSize=50：3 条不出翻页（独立挂载，避免与上一棵树混淆）
    const { fs: fs2, reply: reply2 } = fakeFs();
    const view2 = render(<RemoteFileBrowser fs={fs2} readOnly={false} notify={jest.fn()} />);
    reply2();
    await view2.findByText('logs');
    expect(view2.queryByTitle('2')).toBeNull();
  });

  it('传输进度条：分块到达时显示、完成/失败后清除', async () => {
    const urlMock = { createObjectURL: jest.fn(() => 'blob:mock'), revokeObjectURL: jest.fn() };
    Object.assign(URL, urlMock);
    const { fs, requests, reply } = fakeFs();
    const { getByTestId, findByTestId, queryByTestId } = render(
      <RemoteFileBrowser fs={fs} readOnly={false} notify={jest.fn()} />,
    );
    reply();
    fireEvent.click(await findByTestId('remote-fs-download-zed.txt'));
    await waitForUI(() => expect(requests[1]?.name).toBe('/zed.txt'));
    const stream = fakeInputStream();
    requests[1].cb(stream.stream, 'text/plain');
    // 第一块（1024/2048 = 50%）：进度区出现，未完成前不消失
    stream.emitBlob(btoa('x'.repeat(1024)));
    await waitForUI(() => expect(getByTestId('remote-fs-transfers')).toBeTruthy());
    expect(getByTestId('remote-fs-progress-download:zed.txt')).toBeTruthy();
    // 完成后进度条清除
    stream.emitBlob(btoa('y'.repeat(1024)));
    stream.emitEnd();
    await waitForUI(() => expect(queryByTestId('remote-fs-transfers')).toBeNull());
  });

  it('监看模式：上传入口不渲染（浏览与下载不受限）', async () => {
    const { fs, reply } = fakeFs();
    const { queryByText, queryByTestId, findByText, findByTestId } = render(
      <RemoteFileBrowser fs={fs} readOnly notify={jest.fn()} />,
    );
    reply();
    await findByText('logs');
    expect(queryByText('上传到当前目录')).toBeNull();
    expect(queryByTestId('remote-fs-upload-input')).toBeNull();
    // 下载入口仍在
    expect(await findByTestId('remote-fs-download-zed.txt')).toBeTruthy();
  });

  it('列目录失败：notify 报错，不崩溃', async () => {
    const fs: RemoteFileSystemObject = {
      index: 0,
      requestInputStream: jest.fn((_name: string, cb: (s: RemoteStreamIn, m: string) => void) => {
        const stream = fakeInputStream();
        cb(stream.stream, 'text/json');
        stream.emitBlob(btoa('broken'));
        stream.emitEnd();
      }),
      createOutputStream: jest.fn(),
    };
    const notify = jest.fn();
    const { findByTestId } = render(<RemoteFileBrowser fs={fs} readOnly={false} notify={notify} />);
    await findByTestId('remote-file-browser');
    await waitForUI(() =>
      expect(notify).toHaveBeenCalledWith('error', '目录内容解析失败（非 JSON）'),
    );
  });

  it('用户取消选择（files 为空）不发起上传', async () => {
    const { fs, requests, reply } = fakeFs();
    const notify = jest.fn();
    const { findByTestId } = render(<RemoteFileBrowser fs={fs} readOnly={false} notify={notify} />);
    reply();
    const input = (await findByTestId('remote-fs-upload-input')) as HTMLInputElement;
    fireEvent.change(input, { target: { files: [] } });
    expect(fs.createOutputStream).not.toHaveBeenCalled();
    expect(requests.length).toBe(1);
  });

  // requestInputStream 在 Promise executor 里同步抛错 → promise 以抛出值
  // reject。抛非 Error 值时组件必须走兜底文案，而不是把对象渲染成 "undefined"。
  function blowingFs(blow: () => unknown) {
    let calls = 0;
    const fs: RemoteFileSystemObject = {
      index: 0,
      requestInputStream: jest.fn((name: string, cb: (s: RemoteStreamIn, m: string) => void) => {
        calls += 1;
        if (calls === 1) {
          // 首次（列根目录）正常应答，让列表渲染出来
          const stream = fakeInputStream();
          cb(stream.stream, 'text/json');
          stream.emitBlob(
            btoa(
              JSON.stringify([{ name: 'x', directory: false, mimetype: 'text/plain', size: 1 }]),
            ),
          );
          stream.emitEnd();
          return;
        }
        throw blow();
      }),
      createOutputStream: jest.fn(),
    };
    return fs;
  }

  it('列目录请求抛非 Error 值 → 兜底文案', async () => {
    const notify = jest.fn();
    const fs: RemoteFileSystemObject = {
      index: 0,
      requestInputStream: jest.fn(() => {
        throw 'raw'; // 故意非 Error：走兜底分支
      }),
      createOutputStream: jest.fn(),
    };
    const { findByTestId } = render(<RemoteFileBrowser fs={fs} readOnly={false} notify={notify} />);
    await findByTestId('remote-file-browser');
    await waitForUI(() => expect(notify).toHaveBeenCalledWith('error', '目录读取失败'));
  });

  it('上传请求流创建同步抛非 Error 值 → 兜底文案', async () => {
    const notify = jest.fn();
    // 首次列目录正常应答（让上传入口渲染出来），createOutputStream 同步抛
    let listed = false;
    const fs: RemoteFileSystemObject = {
      index: 0,
      requestInputStream: jest.fn((_name: string, cb: (s: RemoteStreamIn, m: string) => void) => {
        if (listed) {
          return;
        }
        listed = true;
        const stream = fakeInputStream();
        cb(stream.stream, 'text/json');
        stream.emitBlob(btoa(JSON.stringify(ENTRIES)));
        stream.emitEnd();
      }),
      createOutputStream: jest.fn(() => {
        throw 'raw'; // 故意非 Error：uploadRemoteFile 的 executor 抛出 → reject
      }),
    };
    const { findByTestId, findByText } = render(
      <RemoteFileBrowser fs={fs} readOnly={false} notify={notify} />,
    );
    await findByText('logs');
    const input = (await findByTestId('remote-fs-upload-input')) as HTMLInputElement;
    fireEvent.change(input, { target: { files: [new File(['x'], 'f.bin', { type: '' })] } });
    await waitForUI(() => expect(notify).toHaveBeenCalledWith('error', 'f.bin 上传失败'));
    // 出错后列表仍刷新（本次 requestInputStream 不再应答，loading 挂着即可）
  });

  it('下载请求抛 Error / 非 Error → 各自文案', async () => {
    const notify = jest.fn();
    // arm 0：Error 实例 → 透出 message
    const { findByTestId, unmount } = render(
      <RemoteFileBrowser
        fs={blowingFs(() => new Error('sftp gone'))}
        readOnly={false}
        notify={notify}
      />,
    );
    fireEvent.click(await findByTestId('remote-fs-download-x'));
    await waitForUI(() => expect(notify).toHaveBeenCalledWith('error', 'sftp gone'));
    unmount();

    // arm 1：非 Error 值 → 条目名兜底文案
    const notify2 = jest.fn();
    const { findByTestId: findByTestId2 } = render(
      <RemoteFileBrowser fs={blowingFs(() => 'raw')} readOnly={false} notify={notify2} />,
    );
    fireEvent.click(await findByTestId2('remote-fs-download-x'));
    await waitForUI(() => expect(notify2).toHaveBeenCalledWith('error', 'x 下载失败'));
  });
});
