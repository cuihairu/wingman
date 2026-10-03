import { Footer, Question, SelectLang, AvatarDropdown, AvatarName } from '@/components';
import MessagesBell from '@/components/MessagesBell';
import { UserOutlined } from '@ant-design/icons';
import type { Settings as LayoutSettings } from '@ant-design/pro-components';
import { SettingDrawer } from '@ant-design/pro-components';
import type { RunTimeLayoutConfig } from '@umijs/max';
import { history } from '@umijs/max';
import defaultSettings from '../config/defaultSettings';
import { errorConfig } from './requestErrorConfig';
import { getMyPermissions, getMyProfile } from '@/services/api';
import React, { useEffect } from 'react';
import { App as AntdApp, Grid } from 'antd';
import { setAppApi } from './utils/antdApp';

const isDev = process.env.NODE_ENV === 'development';
const loginPath = '/user/login';

// 登录页等 layout:false 路由不经过 ProLayout 的 childrenRender —— AntdApp 若只挂
// 在那里，登录页上 getMessage() 恒为 undefined，requestErrorConfig 的 errorHandler
// 与页面 catch 的 toast 全部静默（2026-10-03 线上实测 401/429 均无任何提示）。
// 把 provider 提到 rootContainer，全站（含 layout:false 页面）都能拿到
// context-aware message/notification API。
const AppApiRegistrar: React.FC = () => {
  const inst = AntdApp.useApp();
  useEffect(() => {
    setAppApi({ message: inst.message, notification: inst.notification });
  }, [inst]);
  return null;
};

export function rootContainer(container: React.ReactNode) {
  return (
    <AntdApp>
      <AppApiRegistrar />
      {container}
    </AntdApp>
  );
}

type InitialCurrentUser = {
  name?: string;
  userid?: string;
  access?: string;
  roles?: any[];
  avatar?: string;
};

/**
 * @see  https://umijs.org/zh-CN/plugins/plugin-initial-state
 * */
export async function getInitialState(): Promise<{
  settings?: Partial<LayoutSettings>;
  currentUser?: InitialCurrentUser;
  loading?: boolean;
  fetchUserInfo?: () => Promise<InitialCurrentUser | undefined>;
}> {
  const fetchUserInfo = async () => {
    try {
      const token = localStorage.getItem('token');
      if (!token) return undefined;
      // 简化：直接使用 token 作为用户名
      const profile = await getMyProfile();
      const roleNames: string[] = Array.isArray(profile?.roles) ? profile.roles : ['user'];
      let permissionIDs: string[] = [];
      try {
        const perms = await getMyPermissions();
        permissionIDs =
          (perms as any)?.permissionIDs ||
          (perms as any)?.permissionIds ||
          (perms as any)?.permission_ids ||
          [];
      } catch {
        permissionIDs = [];
      }
      const accessTokens = Array.from(new Set([...(permissionIDs || []), ...(roleNames || [])]))
        .map((t) =>
          String(t || '')
            .trim()
            .toLowerCase(),
        )
        .filter(Boolean);
      return {
        name: profile?.nickname || profile?.displayName || profile?.username || 'user',
        userid: profile?.username || 'user',
        access: accessTokens.join(','),
        roles: roleNames,
        avatar: profile?.avatar,
      } as any;
    } catch (error: any) {
      if (error?.response?.status === 401 || error?.response?.status === 400) {
        localStorage.removeItem('token');
      }
      history.push(loginPath);
      return undefined;
    }
  };

  const { location } = history;
  if (location.pathname !== loginPath) {
    const currentUser = await fetchUserInfo();
    return {
      fetchUserInfo,
      currentUser,
      settings: defaultSettings as Partial<LayoutSettings>,
    };
  }
  return {
    fetchUserInfo,
    settings: defaultSettings as Partial<LayoutSettings>,
  };
}

// ProLayout 支持的api https://procomponents.ant.design/components/layout
export const layout: RunTimeLayoutConfig = ({ initialState, setInitialState }) => {
  const isAuthed = !!initialState?.currentUser;

  const HeaderActions: React.FC = () => {
    const screens = Grid.useBreakpoint();
    const isMobile = !screens.md;
    if (!isAuthed) {
      return (
        <>
          <Question key="doc" />
          <SelectLang key="SelectLang" />
        </>
      );
    }
    if (isMobile) {
      return (
        <>
          <SelectLang key="SelectLang" />
        </>
      );
    }
    return (
      <>
        <Question key="doc" />
        <MessagesBell key="messages" />
        <SelectLang key="SelectLang" />
      </>
    );
  };

  return {
    actionsRender: () => [<HeaderActions key="header-actions" />] as any,
    splitMenus: false,
    suppressSiderWhenMenuEmpty: true,
    avatarProps: {
      src: initialState?.currentUser?.avatar || undefined,
      icon: initialState?.currentUser?.avatar ? undefined : <UserOutlined />,
      title: <AvatarName />,
      render: (_, avatarChildren) => {
        return <AvatarDropdown menu>{avatarChildren}</AvatarDropdown>;
      },
    },
    footerRender: () => <Footer />,
    onPageChange: () => {
      const { location } = history;
      if (!initialState?.currentUser && location.pathname !== loginPath) {
        history.push(loginPath);
      }
    },
    links: [],
    menuHeaderRender: undefined,
    childrenRender: (children) => {
      return (
        <>
          {children}
          {isDev && (
            <SettingDrawer
              disableUrlParams
              enableDarkTheme
              settings={initialState?.settings}
              onSettingChange={(settings) => {
                setInitialState((preInitialState) => ({
                  ...preInitialState,
                  settings,
                }));
              }}
            />
          )}
        </>
      );
    },
    ...initialState?.settings,
  };
};

/**
 * @name request 配置
 * @doc https://umijs.org/docs/max/request#配置
 */
export const request = {
  ...errorConfig,
};
