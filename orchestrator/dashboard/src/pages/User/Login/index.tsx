import { Footer } from '@/components';
import { BRAND } from '@/config/branding';
import { createSession, fetchCurrentUserGames } from '@/services/api';
import { setScope } from '@/stores/scope';
import { getMessage } from '@/utils/antdApp';
import { LockOutlined, UserOutlined } from '@ant-design/icons';
import { LoginForm, ProFormCheckbox, ProFormText } from '@ant-design/pro-components';
import { FormattedMessage, Helmet, history, SelectLang, useIntl, useModel } from '@umijs/max';
import { Alert, Modal } from 'antd';
import { createStyles } from 'antd-style';
import React, { useState } from 'react';
import { flushSync } from 'react-dom';
import Settings from '../../../../config/defaultSettings';

const useStyles = createStyles(({ token }) => ({
  action: {
    marginLeft: '8px',
    color: 'rgba(0, 0, 0, 0.2)',
    fontSize: '24px',
    verticalAlign: 'middle',
    cursor: 'pointer',
    transition: 'color 0.3s',
    '&:hover': {
      color: token.colorPrimaryActive,
    },
  },
  lang: {
    width: 42,
    height: 42,
    lineHeight: '42px',
    position: 'fixed',
    right: 16,
    borderRadius: token.borderRadius,
    ':hover': {
      backgroundColor: token.colorBgTextHover,
    },
  },
  container: {
    display: 'flex',
    flexDirection: 'column',
    height: '100vh',
    overflow: 'auto',
    backgroundImage:
      "url('https://mdn.alipayobjects.com/yuyan_qk0oxh/afts/img/V-_oS6r-i7wAAAAAAAAAAAAAFl94AQBr')",
    backgroundSize: '100% 100%',
  },
}));

const Lang = () => {
  const { styles } = useStyles();

  return (
    <div className={styles.lang} data-lang>
      {SelectLang && <SelectLang />}
    </div>
  );
};

const LoginMessage: React.FC<{ content: string }> = ({ content }) => (
  <Alert
    style={{
      marginBottom: 24,
    }}
    message={content}
    type="error"
    showIcon
  />
);

const Login: React.FC = () => {
  const [userLoginState] = useState<{ status?: string; type?: string }>({});
  const { initialState, setInitialState } = useModel('@@initialState');
  const [forgotOpen, setForgotOpen] = useState(false);
  const { styles } = useStyles();
  const intl = useIntl();

  const fetchUserInfo = async () => {
    const userInfo = await initialState?.fetchUserInfo?.();
    if (userInfo) {
      flushSync(() => {
        setInitialState((state) => ({
          ...state,
          currentUser: userInfo,
        }));
      });
    }
  };

  const handleSubmit = async (values: { username: string; password: string }) => {
    try {
      const response = await createSession({
        username: values.username,
        password: values.password,
      });
      localStorage.setItem('token', response.token);

      try {
        const gamesResp = await fetchCurrentUserGames();
        const games = Array.isArray(gamesResp?.games) ? gamesResp.games : [];
        const firstGame = games[0];
        const gameId = firstGame?.gameId || firstGame?.name;
        const env =
          (Array.isArray(firstGame?.envs) && firstGame.envs[0]) ||
          (Array.isArray(firstGame?.envMeta) && firstGame.envMeta[0]?.env) ||
          undefined;

        if (gameId || env) {
          setScope(
            {
              gameId: gameId || undefined,
              env: env || undefined,
            },
            { persist: true, emit: true },
          );
        }
      } catch {}

      getMessage()?.success(
        intl.formatMessage({
          id: 'pages.login.success',
          defaultMessage: '登录成功',
        }),
      );

      await fetchUserInfo();
      const urlParams = new URL(window.location.href).searchParams;
      history.push(urlParams.get('redirect') || '/');
    } catch (error) {
      const defaultLoginFailureMessage = intl.formatMessage({
        id: 'pages.login.failure',
        defaultMessage: '登录失败，请重试',
      });
      console.log(error);
      getMessage()?.error(defaultLoginFailureMessage);
    }
  };

  const { status } = userLoginState;

  return (
    <div className={styles.container}>
      <Helmet>
        <title>
          {intl.formatMessage({
            id: 'menu.login',
            defaultMessage: '登录',
          })}
          {' - '}
          {Settings.title}
        </title>
      </Helmet>
      <Lang />
      <div
        style={{
          flex: '1',
          padding: '32px 0',
        }}
      >
        <LoginForm
          contentStyle={{
            minWidth: 280,
            width: 'min(420px, calc(100vw - 32px))',
            maxWidth: 'calc(100vw - 32px)',
          }}
          logo={<img alt="logo" src={BRAND.logo || '/logo.svg'} />}
          title={BRAND.title || 'Wingman'}
          subTitle={BRAND.subTitle || intl.formatMessage({ id: 'pages.layouts.userLayout.title' })}
          initialValues={{
            autoLogin: true,
          }}
          actions={[]}
          onFinish={async (values) => {
            await handleSubmit(values as { username: string; password: string });
          }}
        >
          {status === 'error' && (
            <LoginMessage
              content={intl.formatMessage({
                id: 'pages.login.accountLogin.errorMessage',
                defaultMessage: '用户名或密码错误',
              })}
            />
          )}

          <ProFormText
            name="username"
            fieldProps={{
              size: 'large',
              prefix: <UserOutlined />,
            }}
            placeholder={intl.formatMessage({
              id: 'pages.login.username.placeholder',
              defaultMessage: '用户名',
            })}
            rules={[
              {
                required: true,
                message: (
                  <FormattedMessage
                    id="pages.login.username.required"
                    defaultMessage="请输入用户名"
                  />
                ),
              },
            ]}
          />
          <ProFormText.Password
            name="password"
            fieldProps={{
              size: 'large',
              prefix: <LockOutlined />,
            }}
            placeholder={intl.formatMessage({
              id: 'pages.login.password.placeholder',
              defaultMessage: '密码',
            })}
            rules={[
              {
                required: true,
                message: (
                  <FormattedMessage
                    id="pages.login.password.required"
                    defaultMessage="请输入密码"
                  />
                ),
              },
            ]}
          />

          <div
            style={{
              marginBottom: 24,
            }}
          >
            <ProFormCheckbox noStyle name="autoLogin">
              <FormattedMessage id="pages.login.rememberMe" defaultMessage="自动登录" />
            </ProFormCheckbox>
            <a style={{ float: 'right' }} onClick={() => setForgotOpen(true)}>
              <FormattedMessage id="pages.login.forgotPassword" defaultMessage="忘记密码" />
            </a>
          </div>
        </LoginForm>
        <Modal
          title={intl.formatMessage({
            id: 'pages.login.forgotPassword',
            defaultMessage: '忘记密码',
          })}
          open={forgotOpen}
          onCancel={() => setForgotOpen(false)}
          onOk={() => setForgotOpen(false)}
        >
          <div>
            <p>请联系管理员为你的账户重置密码。</p>
            <p>如果你是管理员，请在用户管理中为目标账户设置新密码。</p>
          </div>
        </Modal>
      </div>
      <Footer />
    </div>
  );
};

export default Login;
