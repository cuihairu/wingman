import { Outlet, Link, useLocation, useNavigate } from 'umi';
import { Layout, Menu, theme, Button, Dropdown, Avatar } from 'antd';
import {
  HomeOutlined,
  CodeOutlined,
  WindowsOutlined,
  SettingOutlined,
  LogoutOutlined,
  UserOutlined,
} from '@ant-design/icons';
import { useEffect, useState } from 'react';
import './index.less';

const { Header, Content, Sider } = Layout;

interface User {
  id: number;
  username: string;
  role: string;
}

export default function Layouts() {
  const navigate = useNavigate();
  const location = useLocation();
  const [user, setUser] = useState<User | null>(null);
  const [loading, setLoading] = useState(true);

  const {
    token: { colorBgContainer, borderRadiusLG },
  } = theme.useToken();

  useEffect(() => {
    // Check authentication
    const token = localStorage.getItem('wingman_token');
    const userStr = localStorage.getItem('wingman_user');

    if (!token || !userStr) {
      navigate('/login');
      return;
    }

    try {
      setUser(JSON.parse(userStr));
    } catch (e) {
      navigate('/login');
    }
    setLoading(false);
  }, [navigate]);

  const handleLogout = () => {
    localStorage.removeItem('wingman_token');
    localStorage.removeItem('wingman_user');
    navigate('/login');
  };

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

  const userMenuItems = [
    {
      key: 'logout',
      icon: <LogoutOutlined />,
      label: '退出登录',
      onClick: handleLogout,
    },
  ];

  if (loading) {
    return <div>Loading...</div>;
  }

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
        <Header
          style={{
            background: colorBgContainer,
            padding: '0 24px',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}
        >
          <div className="header-title">Wingman Dashboard</div>
          <Dropdown menu={{ items: userMenuItems }} placement="bottomRight">
            <Button type="text" style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
              <Avatar size="small" icon={<UserOutlined />} />
              <span>{user?.username}</span>
            </Button>
          </Dropdown>
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
