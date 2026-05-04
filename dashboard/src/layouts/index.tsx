import { Outlet, Link, useLocation } from 'umi';
import { Layout, Menu, theme } from 'antd';
import {
  HomeOutlined,
  CodeOutlined,
  WindowsOutlined,
  SettingOutlined,
} from '@ant-design/icons';
import './index.less';

const { Header, Content, Sider } = Layout;

export default function Layouts() {
  const {
    token: { colorBgContainer, borderRadiusLG },
  } = theme.useToken();
  const location = useLocation();

  const menuItems = [
    {
      key: '/',
      icon: <HomeOutlined />,
      label: <Link to="/">首页</Link>,
    },
    {
      key: '/scripts',
      icon: <CodeOutlined />,
      label: <Link to="/scripts">脚本管理</Link>,
    },
    {
      key: '/windows',
      icon: <WindowsOutlined />,
      label: <Link to="/windows">窗口监控</Link>,
    },
    {
      key: '/settings',
      icon: <SettingOutlined />,
      label: <Link to="/settings">设置</Link>,
    },
  ];

  return (
    <Layout style={{ minHeight: '100vh' }}>
      <Sider width={256} theme="dark">
        <div className="logo">
          <h1>Wingman</h1>
          <span>游戏自动化控制</span>
        </div>
        <Menu
          theme="dark"
          mode="inline"
          selectedKeys={[location.pathname]}
          items={menuItems}
        />
      </Sider>
      <Layout>
        <Header style={{ background: colorBgContainer, padding: '0 24px' }}>
          <div className="header-title">Wingman Dashboard</div>
        </Header>
        <Content style={{ margin: '24px' }}>
          <div
            style={{
              padding: 24,
              minHeight: 360,
              background: colorBgContainer,
              borderRadius: borderRadiusLG,
            }}
          >
            <Outlet />
          </div>
        </Content>
      </Layout>
    </Layout>
  );
}
