/**
 * 录像回放面板组件测试。
 *
 * recordingPlayer（真实 SessionRecording 封装）mock 掉——底层解析/状态机
 * 的真实性由 recordingPlayer.test.ts 用真实库 + 样例录像锁定；这里只测
 * 组件自身：加载/错误/就绪三态、控制条联动、重试与关闭清理。
 */
import { fireEvent, render, screen, waitFor } from '@testing-library/react';
// jest-dom 匹配器的类型只随显式 import 进来（tests/ 目录走 tsconfig.jest 的
// types 配置，src/ 下的测试文件不吃那份配置）
import '@testing-library/jest-dom';
import RemoteRecordingPlayer from './RemoteRecordingPlayer';
import { createRecordingPlayer } from './recordingPlayer';
import type { RecordingPlayer, RecordingPlayerEvents } from './recordingPlayer';

jest.mock('./recordingPlayer', () => {
  const actual = jest.requireActual('./recordingPlayer');
  return { ...actual, createRecordingPlayer: jest.fn() };
});

const MockedCreate = createRecordingPlayer as jest.MockedFunction<typeof createRecordingPlayer>;

/** 构造假播放器 + 捕获事件句柄（测试里手动派发 onReady 等回调） */
function makeFakePlayer(overrides: Partial<RecordingPlayer> = {}) {
  // events 形参带默认值 → 类型上可选（|undefined），读侧统一断言
  const players: Array<{ events: RecordingPlayerEvents | undefined }> = [];
  const fake: RecordingPlayer = {
    element: document.createElement('div'),
    play: jest.fn(),
    pause: jest.fn(),
    toggle: jest.fn(),
    seek: jest.fn(),
    scale: jest.fn(),
    displaySize: jest.fn(() => ({ width: 1280, height: 720 })),
    isPlaying: jest.fn(() => false),
    getPosition: jest.fn(() => 0),
    getDuration: jest.fn(() => 4400),
    dispose: jest.fn(),
    ...overrides,
  };
  MockedCreate.mockImplementation((_blob, events) => {
    players.push({ events });
    return fake;
  });
  return {
    fake,
    events: () => players[players.length - 1]!.events!,
    createdCount: () => players.length,
  };
}

describe('RemoteRecordingPlayer', () => {
  beforeEach(() => {
    MockedCreate.mockReset();
  });

  it('关闭时不加载（open=false 不触发 load，也不创建播放器）', () => {
    const load = jest.fn(() => Promise.resolve(new Blob(['x'])));
    render(<RemoteRecordingPlayer open={false} name="a.mjs" load={load} onClose={() => {}} />);
    expect(load).not.toHaveBeenCalled();
    expect(MockedCreate).not.toHaveBeenCalled();
  });

  it('打开后加载中显示提示，onReady 后挂画面并显示时长与控制条', async () => {
    const h = makeFakePlayer();
    const load = jest.fn(() => Promise.resolve(new Blob(['rec-bytes'])));
    const onClose = jest.fn();
    render(<RemoteRecordingPlayer open name="agent-1.mjs" load={load} onClose={onClose} />);

    expect(screen.getByText('正在加载录像…')).toBeInTheDocument();
    expect(screen.getByText('回放：agent-1.mjs')).toBeInTheDocument();

    await waitFor(() => expect(MockedCreate).toHaveBeenCalled());
    // 播放器拿到的就是 load 回来的 Blob
    const blob = MockedCreate.mock.calls[0][0] as Blob;
    expect(blob.size).toBeGreaterThan(0);
    h.events().onReady?.(4400);

    await waitFor(() =>
      expect(screen.getByTestId('remote-recording-stage')).toContainElement(h.fake.element),
    );
    expect(screen.getByText('00:00 / 00:04')).toBeInTheDocument();
    expect(screen.getByRole('button', { name: '播放' })).toBeEnabled();
    // 就绪后加载提示撤掉
    expect(screen.queryByText('正在加载录像…')).not.toBeInTheDocument();
  });

  it('播放/暂停按钮走 toggle，播放态由事件回流（图标/无障碍名切换）', async () => {
    const h = makeFakePlayer();
    render(
      <RemoteRecordingPlayer
        open
        name="a.mjs"
        load={() => Promise.resolve(new Blob(['x']))}
        onClose={() => {}}
      />,
    );
    await waitFor(() => expect(MockedCreate).toHaveBeenCalled());
    h.events().onReady?.(4400);
    const btn = await screen.findByRole('button', { name: '播放' });

    fireEvent.click(btn);
    expect(h.fake.toggle).toHaveBeenCalledTimes(1);
    // 播放状态不自己猜——由底层 onPlayingChange 回流
    h.events().onPlayingChange?.(true);
    expect(await screen.findByRole('button', { name: '暂停' })).toBeInTheDocument();

    h.events().onPlayingChange?.(false);
    expect(await screen.findByRole('button', { name: '播放' })).toBeInTheDocument();
  });

  it('位置事件驱动进度条与时间文本（不手工推进）', async () => {
    const h = makeFakePlayer();
    render(
      <RemoteRecordingPlayer
        open
        name="a.mjs"
        load={() => Promise.resolve(new Blob(['x']))}
        onClose={() => {}}
      />,
    );
    await waitFor(() => expect(MockedCreate).toHaveBeenCalled());
    h.events().onReady?.(4400);
    h.events().onPosition?.(1300);

    expect(await screen.findByText('00:01 / 00:04')).toBeInTheDocument();
    const slider = screen.getByRole('slider');
    expect(slider).toHaveAttribute('aria-valuenow', '1300');
    expect(slider).toHaveAttribute('aria-valuemax', '4400');
  });

  it('load 失败显示错误与重试；重试重新走 load → 新播放器', async () => {
    const load = jest
      .fn()
      .mockRejectedValueOnce(new Error('网络错误'))
      .mockResolvedValueOnce(new Blob(['x']));
    const h = makeFakePlayer();
    render(<RemoteRecordingPlayer open name="a.mjs" load={load} onClose={() => {}} />);

    expect(await screen.findByText('录像无法回放')).toBeInTheDocument();
    expect(screen.getByText('网络错误')).toBeInTheDocument();

    fireEvent.click(screen.getByText('重试'));
    // 第一次失败期间 dispose 不该被调（还没创建过播放器）
    expect(h.fake.dispose).not.toHaveBeenCalled();
    await waitFor(() => expect(load).toHaveBeenCalledTimes(2));
    await waitFor(() => expect(MockedCreate).toHaveBeenCalledTimes(1));
    h.events().onReady?.(4400);
    expect(await screen.findByText('00:00 / 00:04')).toBeInTheDocument();
  });

  it('底层解析错误（onError）同样进入错误态', async () => {
    const h = makeFakePlayer();
    render(
      <RemoteRecordingPlayer
        open
        name="a.mjs"
        load={() => Promise.resolve(new Blob(['x']))}
        onClose={() => {}}
      />,
    );
    await waitFor(() => expect(MockedCreate).toHaveBeenCalled());
    h.events().onError?.('Non-numeric character in element length.');
    expect(await screen.findByText('录像无法回放')).toBeInTheDocument();
    expect(screen.getByText('Non-numeric character in element length.')).toBeInTheDocument();
  });

  it('关闭与卸载都走 dispose 并清空画面挂载点', async () => {
    const h = makeFakePlayer();
    const onClose = jest.fn();
    const props = {
      name: 'a.mjs',
      load: () => Promise.resolve(new Blob(['x'])),
      onClose,
    };
    const view = render(<RemoteRecordingPlayer open {...props} />);
    await waitFor(() => expect(MockedCreate).toHaveBeenCalled());
    h.events().onReady?.(4400);
    await waitFor(() =>
      expect(screen.getByTestId('remote-recording-stage')).toContainElement(h.fake.element),
    );

    // antd 两字纯文本按钮会插空格（关 闭）
    fireEvent.click(screen.getByText('关 闭'));
    expect(onClose).toHaveBeenCalledTimes(1);

    // 父组件响应 onClose 置 open=false → effect 清理：dispose + 画面摘除
    view.rerender(<RemoteRecordingPlayer open={false} {...props} />);
    await waitFor(() => expect(h.fake.dispose).toHaveBeenCalledTimes(1));
    expect(screen.queryByTestId('remote-recording-stage')).not.toBeInTheDocument();
    view.unmount();
  });

  it('换名字重开：旧播放器 dispose、新 load 拉新 Blob', async () => {
    const h = makeFakePlayer();
    const { rerender } = render(
      <RemoteRecordingPlayer
        open
        name="a.mjs"
        load={() => Promise.resolve(new Blob(['first']))}
        onClose={() => {}}
      />,
    );
    await waitFor(() => expect(MockedCreate).toHaveBeenCalledTimes(1));
    h.events().onReady?.(4400);

    rerender(
      <RemoteRecordingPlayer
        open
        name="b.mjs"
        load={() => Promise.resolve(new Blob(['second']))}
        onClose={() => {}}
      />,
    );
    await waitFor(() => expect(MockedCreate).toHaveBeenCalledTimes(2));
    expect(h.fake.dispose).toHaveBeenCalledTimes(1);
    expect(await screen.findByText('回放：b.mjs')).toBeInTheDocument();
  });

  it('fitScale：就绪后按 stage 尺寸等比缩放（jsdom 无布局，stub 客户区）', async () => {
    // displaySize 可变：模拟「分辨率未落定 → 落定」两态
    let dims = { width: 0, height: 0 };
    const h = makeFakePlayer({ displaySize: () => dims });
    render(
      <RemoteRecordingPlayer
        open
        name="a.mjs"
        load={() => Promise.resolve(new Blob(['x']))}
        onClose={() => {}}
      />,
    );
    await waitFor(() => expect(MockedCreate).toHaveBeenCalled());
    // Modal 内容渲染进 body portal：用 screen 查（container 里没有）
    const stage = screen.getByTestId('remote-recording-stage');
    Object.defineProperty(stage, 'clientWidth', { value: 640, configurable: true });
    Object.defineProperty(stage, 'clientHeight', { value: 480, configurable: true });

    // 分辨率未知（width=0）时不动缩放
    h.events().onDisplayResize?.(0, 0);
    expect(h.fake.scale).not.toHaveBeenCalled();

    // 分辨率落定事件触发自适应：1280x720 → min(640/1280, 480/720) = 0.5
    dims = { width: 1280, height: 720 };
    h.events().onDisplayResize?.(1280, 720);
    await waitFor(() => expect(h.fake.scale).toHaveBeenCalledWith(expect.closeTo(0.5)));
  });
});
