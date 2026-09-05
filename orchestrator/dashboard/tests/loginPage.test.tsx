/**
 * Login 页补全：登录失败提示、忘记密码弹窗、登录后游戏 scope 写入与
 * redirect 跳转。services/api 使用本地 mock 控制成功/失败分支。
 */
import { render, screen, fireEvent, act, waitFor } from '@testing-library/react';
import React from 'react';

const historyMock = { push: jest.fn(), replace: jest.fn() };
const mockCreateSession = jest.fn();
const mockFetchCurrentUserGames = jest.fn();

jest.mock('@umijs/max', () => ({
  __esModule: true,
  history: historyMock,
  useModel: () => mockModelState,
  useIntl: () => ({ formatMessage: ({ defaultMessage }: any) => defaultMessage }),
  FormattedMessage: ({ defaultMessage }: any) => <>{defaultMessage}</>,
  SelectLang: () => null,
  Helmet: ({ children }: any) => <>{children}</>,
}));

jest.mock('@/services/api', () => ({
  __esModule: true,
  createSession: (...args: unknown[]) => mockCreateSession(...args),
  fetchCurrentUserGames: (...args: unknown[]) => mockFetchCurrentUserGames(...args),
}));

// BRAND 留空以覆盖 logo/title/subTitle 的回退分支
jest.mock('@/config/branding', () => ({
  __esModule: true,
  BRAND: { logo: '', title: '', subTitle: '' },
}));

const mockModelState = {
  initialState: {
    fetchUserInfo: jest.fn(async () => ({ username: 'admin', roles: ['admin'] })),
  } as Record<string, unknown>,
  setInitialState: jest.fn(),
};

import Login from '@/pages/User/Login';

async function submitLoginForm(username = 'admin', password = 'pw') {
  const root = render(<Login />);
  await root.findAllByText('Wingman');

  await act(async () => {
    fireEvent.change(await root.findByPlaceholderText('用户名'), {
      target: { value: username },
    });
    fireEvent.change(await root.findByPlaceholderText('密码'), {
      target: { value: password },
    });
  });

  const submit = await root.findByRole('button', { name: /登\s*录/ });
  await act(async () => {
    fireEvent.click(submit);
  });
  return root;
}

describe('Login 页扩展分支', () => {
  beforeEach(() => {
    jest.clearAllMocks();
    jest.spyOn(console, 'log').mockImplementation(() => {});
  });

  afterEach(() => {
    jest.restoreAllMocks();
  });

  it('createSession 失败时不跳转', async () => {
    mockCreateSession.mockRejectedValueOnce(new Error('bad credentials'));

    const root = await submitLoginForm();

    await waitFor(() => {
      expect(mockCreateSession).toHaveBeenCalled();
    });
    expect(historyMock.push).not.toHaveBeenCalled();

    root.unmount();
  });

  it('登录成功且返回游戏列表时写入 scope 并跳转 redirect', async () => {
    mockCreateSession.mockResolvedValueOnce({ token: 'tok-9', user: { username: 'admin', roles: ['admin'] } });
    mockFetchCurrentUserGames.mockResolvedValueOnce({
      games: [{ gameId: 'g-9', name: 'fallback-name', envs: ['prod', 'dev'] }],
    });

    window.history.replaceState({}, '', '/?redirect=%2Fmonitor');

    const root = await submitLoginForm();

    await waitFor(() => {
      expect(historyMock.push).toHaveBeenCalledWith('/monitor');
    });
    expect((localStorage.setItem as jest.Mock).mock.calls.some(([k]) => k === 'token')).toBe(true);
    expect(
      (localStorage.setItem as jest.Mock).mock.calls.some(([k]) => k === 'game_id'),
    ).toBe(true);

    window.history.replaceState({}, '', '/');
    root.unmount();
  });

  it('游戏列表加载失败不影响登录成功跳转', async () => {
    mockCreateSession.mockResolvedValueOnce({ token: 'tok-8', user: { username: 'admin', roles: [] } });
    mockFetchCurrentUserGames.mockRejectedValueOnce(new Error('games unavailable'));

    const root = await submitLoginForm();

    await waitFor(() => {
      expect(historyMock.push).toHaveBeenCalledWith('/');
    });
    root.unmount();
  });

  it('games 响应为 undefined 时按空列表处理', async () => {
    mockCreateSession.mockResolvedValueOnce({ token: 'tok-7', user: { username: 'admin', roles: [] } });
    mockFetchCurrentUserGames.mockResolvedValueOnce(undefined);

    const root = await submitLoginForm();

    await waitFor(() => {
      expect(historyMock.push).toHaveBeenCalledWith('/');
    });
    root.unmount();
  });

  it('envMeta 派生 env 且无 gameId 时只写入 env', async () => {
    mockCreateSession.mockResolvedValueOnce({ token: 'tok-6', user: { username: 'admin', roles: [] } });
    mockFetchCurrentUserGames.mockResolvedValueOnce({
      games: [{ envMeta: [{ env: 'meta-env' }] }],
    });

    const root = await submitLoginForm();

    await waitFor(() => {
      expect(
        (localStorage.setItem as jest.Mock).mock.calls.some(([k]) => k === 'env'),
      ).toBe(true);
    });
    expect(
      (localStorage.setItem as jest.Mock).mock.calls.some(([k]) => k === 'game_id'),
    ).toBe(false);
    root.unmount();
  });

  it('仅有 gameId 无 env 时只写入 gameId', async () => {
    mockCreateSession.mockResolvedValueOnce({ token: 'tok-5', user: { username: 'admin', roles: [] } });
    mockFetchCurrentUserGames.mockResolvedValueOnce({
      games: [{ gameId: 'only-game' }],
    });

    const root = await submitLoginForm();

    await waitFor(() => {
      expect(
        (localStorage.setItem as jest.Mock).mock.calls.some(([k]) => k === 'game_id'),
      ).toBe(true);
    });
    expect(
      (localStorage.setItem as jest.Mock).mock.calls.some(([k]) => k === 'env'),
    ).toBe(false);
    root.unmount();
  });

  it('initialState 无 fetchUserInfo 时跳过用户信息刷新', async () => {
    mockModelState.initialState = { nickname: 'n' };
    mockCreateSession.mockResolvedValueOnce({ token: 'tok-4', user: { username: 'admin', roles: [] } });
    mockFetchCurrentUserGames.mockResolvedValueOnce({ games: [] });

    const root = await submitLoginForm();

    await waitFor(() => {
      expect(historyMock.push).toHaveBeenCalledWith('/');
    });
    mockModelState.initialState = {
      fetchUserInfo: jest.fn(async () => ({ username: 'admin', roles: ['admin'] })),
    };
    root.unmount();
  });

  it('忘记密码弹窗打开后可通过取消/确定关闭', async () => {
    const root = await submitLoginForm();

    const link = await root.findByText('忘记密码');
    await act(async () => {
      fireEvent.click(link);
    });

    expect(await screen.findByText('请联系管理员为你的账户重置密码。')).toBeInTheDocument();

    // jsdom 无 CSS 过渡，Modal 关闭动画不结束，DOM 保留；仅验证两个关闭回调可执行
    const cancelBtn = screen.getByRole('button', { name: /取\s*消/ });
    const okBtn = screen.getByRole('button', { name: /确\s*定/ });
    await act(async () => {
      fireEvent.click(cancelBtn);
      fireEvent.click(okBtn);
    });

    root.unmount();
  });
});
