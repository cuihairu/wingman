import { Form, Input, Button, Card, message } from 'antd';
import { UserOutlined, LockOutlined } from '@ant-design/icons';
import { useNavigate } from 'umi';
import { useState } from 'react';

interface LoginResponse {
  success: boolean;
  token?: string;
  user?: {
    id: number;
    username: string;
    role: string;
  };
  message?: string;
}

export default function LoginPage() {
  const navigate = useNavigate();
  const [loading, setLoading] = useState(false);

  const onFinish = async (values: { username: string; password: string }) => {
    setLoading(true);
    try {
      const response = await fetch('/api/auth/login', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(values),
      });

      const data: LoginResponse = await response.json();

      if (data.success && data.token) {
        message.success('登录成功');
        localStorage.setItem('wingman_token', data.token);
        localStorage.setItem('wingman_user', JSON.stringify(data.user));
        navigate('/');
      } else {
        message.error(data.message || '登录失败');
      }
    } catch (error) {
      message.error('网络错误，请检查服务器是否启动');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div style={{
      height: '100vh',
      display: 'flex',
      justifyContent: 'center',
      alignItems: 'center',
      background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)'
    }}>
      <Card
        title="Wingman Dashboard"
        style={{ width: 400, boxShadow: '0 4px 12px rgba(0,0,0,0.15)' }}
      >
        <Form
          name="login"
          initialValues={{ username: 'admin', password: 'admin' }}
          onFinish={onFinish}
          size="large"
        >
          <Form.Item
            name="username"
            rules={[{ required: true, message: '请输入用户名' }]}
          >
            <Input
              prefix={<UserOutlined />}
              placeholder="用户名 (默认: admin)"
            />
          </Form.Item>

          <Form.Item
            name="password"
            rules={[{ required: true, message: '请输入密码' }]}
          >
            <Input.Password
              prefix={<LockOutlined />}
              placeholder="密码 (默认: admin)"
            />
          </Form.Item>

          <Form.Item>
            <Button type="primary" htmlType="submit" block loading={loading}>
              登录
            </Button>
          </Form.Item>

          <div style={{ textAlign: 'center', color: '#888', fontSize: '12px' }}>
            默认账号: admin / admin
          </div>
        </Form>
      </Card>
    </div>
  );
}
