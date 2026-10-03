/**
 * rootContainer 把 AntdApp provider 提到站点根部：layout:false 的登录页不经过
 * ProLayout 的 childrenRender，若 provider 只挂在那里，登录页 getMessage()
 * 恒 undefined —— errorHandler 与页面 catch 的 toast 全部静默。
 */
jest.mock('@umijs/max', () => ({
  __esModule: true,
  history: { push: jest.fn(), replace: jest.fn(), location: { pathname: '/' } },
  useModel: () => ({ initialState: undefined, setInitialState: jest.fn() }),
}));

jest.mock('@/services/api', () => ({
  __esModule: true,
  getMyPermissions: jest.fn(),
  getMyProfile: jest.fn(),
}));

jest.mock('@/components', () => ({
  __esModule: true,
  Footer: () => null,
  Question: () => null,
  SelectLang: () => null,
  AvatarDropdown: () => null,
  AvatarName: () => null,
}));

jest.mock('@/components/MessagesBell', () => ({ __esModule: true, default: () => null }));

import { render, waitFor } from '@testing-library/react';
import { rootContainer } from '@/app';
import { getMessage, getNotification, setAppApi } from '@/utils/antdApp';

describe('app rootContainer', () => {
  afterEach(() => {
    (setAppApi as any)(null);
  });

  it('站点根部挂载 AntdApp 并注册 message/notification API', async () => {
    const { container } = render(rootContainer(<div id="page-root" />));
    expect(container.querySelector('#page-root')).not.toBeNull();

    // AppApiRegistrar 在 effect 中注册，等一拍后 getMessage/getNotification 可用
    await waitFor(() => {
      expect(getMessage()).toBeTruthy();
      expect(getNotification()).toBeTruthy();
    });
    expect(typeof getMessage()!.error).toBe('function');
    expect(typeof getNotification()!.open).toBe('function');
  });
});
