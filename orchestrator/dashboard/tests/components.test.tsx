/**
 * 基础组件：PageStatePanel（全 tone / badge / actions / extra）、
 * HeaderDropdown、RightContent（SelectLang / Question）、
 * AvatarDropdown（loading / 正常 / 菜单点击登出与跳转）、
 * ScreenshotView（帧更新节流 / 开关 / 刷新）。
 */
import { render, screen, fireEvent, act } from '@testing-library/react';
import React from 'react';

const mockUseModel = jest.fn();
const historyMock = { push: jest.fn(), replace: jest.fn() };
const mockCapturedMenuProps: Record<string, any> = {};

jest.mock('@umijs/max', () => ({
  __esModule: true,
  history: historyMock,
  useModel: (...args: unknown[]) => mockUseModel(...args),
  SelectLang: () => <div data-testid="select-lang" />,
}));

jest.mock('@/components/HeaderDropdown', () => ({
  __esModule: true,
  default: (props: any) => {
    mockCapturedMenuProps.menu = props.menu;
    return <div data-testid="hd-capture">{props.children}</div>;
  },
}));

import PageStatePanel from '@/components/PageStatePanel';
import HeaderDropdown from '@/components/HeaderDropdown';
import { Question, SelectLang } from '@/components/RightContent';
import { AvatarDropdown, AvatarName } from '@/components/RightContent/AvatarDropdown';
import ScreenshotView from '@/components/ScreenshotView';

describe('PageStatePanel', () => {
  it('默认 info 且不带 badge', () => {
    render(<PageStatePanel title="标题" description="描述" />);
    expect(screen.getByText('标题')).toBeInTheDocument();
    expect(screen.getByText('描述')).toBeInTheDocument();
  });

  it('四种 tone 均可渲染', () => {
    const tones = ['info', 'success', 'warning', 'error'] as const;
    for (const tone of tones) {
      const { unmount } = render(<PageStatePanel title={`t-${tone}`} tone={tone} />);
      expect(screen.getByText(`t-${tone}`)).toBeInTheDocument();
      unmount();
    }
  });

  it('badgeText + actions + extra', () => {
    render(
      <PageStatePanel
        title="有徽标"
        badgeText="404"
        actions={<button>操作</button>}
        extra={<div>额外内容</div>}
      />,
    );
    expect(screen.getByText('404')).toBeInTheDocument();
    expect(screen.getByText('有徽标')).toBeInTheDocument();
    expect(screen.getByText('操作')).toBeInTheDocument();
    expect(screen.getByText('额外内容')).toBeInTheDocument();
  });
});

describe('RightContent', () => {
  it('SelectLang 透传渲染', () => {
    render(<SelectLang />);
    expect(screen.getByTestId('select-lang')).toBeInTheDocument();
  });

  it('Question 点击打开文档链接', () => {
    const openSpy = jest.spyOn(window, 'open').mockImplementation(() => null);
    const { container } = render(<Question />);
    fireEvent.click(container.firstElementChild!);
    expect(openSpy).toHaveBeenCalledWith(
      'https://github.com/cuihairu/wingman/tree/main/docs/guide/dashboard.md',
    );
    openSpy.mockRestore();
  });
});

describe('AvatarDropdown', () => {
  beforeEach(() => {
    jest.clearAllMocks();
  });

  it('AvatarName 渲染当前用户名', () => {
    mockUseModel.mockReturnValue({
      initialState: { currentUser: { name: '管理员' } },
      setInitialState: jest.fn(),
    });
    render(<AvatarName />);
    expect(screen.getByText('管理员')).toBeInTheDocument();
  });

  it('AvatarName 在 initialState 为 null 时渲染空 span', () => {
    mockUseModel.mockReturnValue({ initialState: null, setInitialState: jest.fn() });
    const { container } = render(<AvatarName />);
    expect(container.querySelector('span.anticon')).toBeInTheDocument();
  });

  it('logout 时 localStorage 异常不阻断跳转', async () => {
    mockUseModel.mockReturnValue({
      initialState: { currentUser: { name: 'op' } },
      setInitialState: jest.fn(),
    });
    const removeItem = window.localStorage.removeItem as jest.Mock;
    removeItem.mockImplementationOnce(() => {
      throw new Error('blocked');
    });
    render(<AvatarDropdown>op</AvatarDropdown>);

    await act(async () => {
      mockCapturedMenuProps.menu.onClick({ key: 'logout' });
    });

    expect(historyMock.replace).toHaveBeenCalled();
  });

  it('已在登录页时不重复跳转', async () => {
    mockUseModel.mockReturnValue({
      initialState: { currentUser: { name: 'op' } },
      setInitialState: jest.fn(),
    });
    window.history.replaceState({}, '', '/user/login');
    render(<AvatarDropdown>op</AvatarDropdown>);

    await act(async () => {
      mockCapturedMenuProps.menu.onClick({ key: 'logout' });
    });

    expect(historyMock.replace).not.toHaveBeenCalled();
    window.history.replaceState({}, '', '/');
  });

  it('URL 带 redirect 参数时登出不再追加跳转', async () => {
    mockUseModel.mockReturnValue({
      initialState: { currentUser: { name: 'op' } },
      setInitialState: jest.fn(),
    });
    window.history.replaceState({}, '', '/?redirect=%2Fsomewhere');
    render(<AvatarDropdown>op</AvatarDropdown>);

    await act(async () => {
      mockCapturedMenuProps.menu.onClick({ key: 'logout' });
    });

    expect(historyMock.replace).not.toHaveBeenCalled();
    window.history.replaceState({}, '', '/');
  });

  it('initialState 缺失时渲染 loading Spin', () => {
    mockUseModel.mockReturnValue({ initialState: undefined, setInitialState: jest.fn() });
    const { container } = render(<AvatarDropdown />);
    expect(container.querySelector('.ant-spin')).toBeInTheDocument();
  });

  it('currentUser 缺失时渲染 loading Spin', () => {
    mockUseModel.mockReturnValue({ initialState: {}, setInitialState: jest.fn() });
    const { container } = render(<AvatarDropdown />);
    expect(container.querySelector('.ant-spin')).toBeInTheDocument();
  });

  it('正常态渲染 HeaderDropdown 并传递菜单', () => {
    mockUseModel.mockReturnValue({
      initialState: { currentUser: { name: 'op' } },
      setInitialState: jest.fn(),
    });
    render(
      <AvatarDropdown menu>
        <span>op-avatar</span>
      </AvatarDropdown>,
    );
    expect(screen.getByTestId('hd-capture')).toBeInTheDocument();
    // menu=true 时包含个人中心 + 分隔线 + 退出登录
    const items = mockCapturedMenuProps.menu.items;
    expect(items.some((item: any) => item?.key === 'center')).toBe(true);
    expect(items.some((item: any) => item?.type === 'divider')).toBe(true);
    expect(items.some((item: any) => item?.key === 'logout')).toBe(true);
  });

  it('menu=false 时无个人中心项', () => {
    mockUseModel.mockReturnValue({
      initialState: { currentUser: { name: 'op' } },
      setInitialState: jest.fn(),
    });
    render(<AvatarDropdown>op</AvatarDropdown>);
    const items = mockCapturedMenuProps.menu.items;
    expect(items.some((item: any) => item?.key === 'center')).toBe(false);
    expect(items.some((item: any) => item?.key === 'logout')).toBe(true);
  });

  it('菜单 logout：flushSync 清空用户并跳转登录页（携带 redirect）', async () => {
    const setInitialState = jest.fn();
    mockUseModel.mockReturnValue({
      initialState: { currentUser: { name: 'op' } },
      setInitialState,
    });
    render(<AvatarDropdown>op</AvatarDropdown>);

    await act(async () => {
      mockCapturedMenuProps.menu.onClick({ key: 'logout' });
    });

    expect(setInitialState).toHaveBeenCalled();
    expect(historyMock.replace).toHaveBeenCalledWith({
      pathname: '/user/login',
      search: expect.stringContaining('redirect'),
    });
  });

  it('菜单非 logout 项跳转账户页', async () => {
    mockUseModel.mockReturnValue({
      initialState: { currentUser: { name: 'op' } },
      setInitialState: jest.fn(),
    });
    render(<AvatarDropdown>op</AvatarDropdown>);

    await act(async () => {
      mockCapturedMenuProps.menu.onClick({ key: 'center' });
    });

    expect(historyMock.push).toHaveBeenCalledWith('/admin/account/center');
  });
});

describe('ScreenshotView', () => {
  beforeEach(() => {
    jest.useFakeTimers();
  });

  afterEach(() => {
    jest.useRealTimers();
  });

  it('空帧时显示等待文案，关闭开关显示关闭文案', () => {
    const { container } = render(<ScreenshotView height={300} />);

    expect(screen.getByText('等待画面...')).toBeInTheDocument();

    const switchEl = container.querySelector('.ant-switch')!;
    act(() => {
      fireEvent.click(switchEl);
    });
    expect(screen.getByText('预览已关闭')).toBeInTheDocument();
  });

  it('refresh 按钮触发 loading 状态', () => {
    const { container } = render(<ScreenshotView />);
    const refreshBtn = container.querySelector('button.ant-btn')!;
    act(() => {
      fireEvent.click(refreshBtn);
    });
    expect(refreshBtn.className).toContain('ant-btn-loading');
  });

  it('订阅 screenshot 帧后 50ms 节流更新画面，img onLoad 清除 loading', async () => {
    const wsModule = await import('@/services/websocket');
    class FakeWS {
      static OPEN = 1;
      readyState = 0;
      onopen: (() => void) | null = null;
      onmessage: ((e: { data: string }) => void) | null = null;
      onclose: (() => void) | null = null;
      onerror: (() => void) | null = null;
      close = jest.fn();
      send = jest.fn();
      simulateOpen() {
        this.readyState = FakeWS.OPEN;
        this.onopen?.();
      }
      simulateMessage(payload: unknown) {
        this.onmessage?.({ data: JSON.stringify(payload) });
      }
    }
    (global as any).WebSocket = FakeWS;
    wsModule.default.connect();
    const socket = (wsModule.default as unknown as { ws: FakeWS }).ws;
    socket!.simulateOpen();

    const { container } = render(<ScreenshotView />);

    act(() => {
      socket!.simulateMessage({
        type: 'screenshot',
        data: { image: 'data:image/png;base64,x', width: 800, height: 600, timestamp: 1700000000000 },
      });
    });
    act(() => {
      jest.advanceTimersByTime(60);
    });

    const img = screen.getByAltText('Screenshot') as HTMLImageElement;
    expect(img.src).toContain('data:image/png');

    // refresh 置 loading，img onLoad 后清除
    const refreshBtn = container.querySelector('button.ant-btn')!;
    act(() => {
      fireEvent.click(refreshBtn);
    });
    expect(refreshBtn.className).toContain('ant-btn-loading');
    act(() => {
      fireEvent.load(img);
    });
    expect(refreshBtn.className).not.toContain('ant-btn-loading');

    (global as any).WebSocket = undefined;
  });
});

