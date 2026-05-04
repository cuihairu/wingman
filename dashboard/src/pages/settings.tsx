import {
  Card,
  Form,
  Input,
  InputNumber,
  Switch,
  Button,
  Space,
  Divider,
  message,
} from 'antd';
import { useEffect, useState } from 'react';

interface SettingsData {
  server: {
    host: string;
    port: number;
  };
  script: {
    defaultPath: string;
    autoStart: boolean;
    maxRunning: number;
  };
  hotkey: {
    pause: string;
    stop: string;
  };
  performance: {
    maxCpu: number;
    maxMemory: number;
  };
}

export default function SettingsPage() {
  const [form] = Form.useForm();
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);

  useEffect(() => {
    loadSettings();
  }, []);

  const loadSettings = async () => {
    setLoading(true);
    // TODO: 从 API 加载设置
    setTimeout(() => {
      const defaultSettings: SettingsData = {
        server: {
          host: 'localhost',
          port: 8080,
        },
        script: {
          defaultPath: 'C:\\\\scripts',
          autoStart: false,
          maxRunning: 10,
        },
        hotkey: {
          pause: 'F10',
          stop: 'F11',
        },
        performance: {
          maxCpu: 80,
          maxMemory: 2048,
        },
      };
      form.setFieldsValue(defaultSettings);
      setLoading(false);
    }, 500);
  };

  const handleSave = async () => {
    try {
      setSaving(true);
      const values = await form.validateFields();
      // TODO: 保存设置到 API
      console.log('Saving settings:', values);
      setTimeout(() => {
        message.success('设置已保存');
        setSaving(false);
      }, 500);
    } catch (error) {
      setSaving(false);
    }
  };

  const handleReset = () => {
    form.resetFields();
    message.info('已重置为默认设置');
  };

  return (
    <div>
      <Card title="系统设置" loading={loading}>
        <Form form={form} layout="vertical">
          <Divider orientation="left">服务器设置</Divider>
          <Form.Item
            label="服务器地址"
            name={['server', 'host']}
            rules={[{ required: true, message: '请输入服务器地址' }]}
          >
            <Input placeholder="localhost" />
          </Form.Item>
          <Form.Item
            label="服务器端口"
            name={['server', 'port']}
            rules={[{ required: true, message: '请输入服务器端口' }]}
          >
            <InputNumber min={1} max={65535} style={{ width: '100%' }} />
          </Form.Item>

          <Divider orientation="left">脚本设置</Divider>
          <Form.Item
            label="默认脚本路径"
            name={['script', 'defaultPath']}
            rules={[{ required: true, message: '请输入默认脚本路径' }]}
          >
            <Input placeholder="C:\\scripts" />
          </Form.Item>
          <Form.Item
            label="最大同时运行脚本数"
            name={['script', 'maxRunning']}
            rules={[{ required: true, message: '请输入最大运行数' }]}
          >
            <InputNumber min={1} max={100} style={{ width: '100%' }} />
          </Form.Item>
          <Form.Item
            label="自动启动脚本"
            name={['script', 'autoStart']}
            valuePropName="checked"
          >
            <Switch />
          </Form.Item>

          <Divider orientation="left">热键设置</Divider>
          <Form.Item
            label="暂停热键"
            name={['hotkey', 'pause']}
            rules={[{ required: true, message: '请输入热键' }]}
          >
            <Input placeholder="F10" />
          </Form.Item>
          <Form.Item
            label="停止热键"
            name={['hotkey', 'stop']}
            rules={[{ required: true, message: '请输入热键' }]}
          >
            <Input placeholder="F11" />
          </Form.Item>

          <Divider orientation="left">性能设置</Divider>
          <Form.Item
            label="最大 CPU 使用率 (%)"
            name={['performance', 'maxCpu']}
            rules={[{ required: true, message: '请输入最大 CPU 使用率' }]}
          >
            <InputNumber min={1} max={100} style={{ width: '100%' }} />
          </Form.Item>
          <Form.Item
            label="最大内存使用 (MB)"
            name={['performance', 'maxMemory']}
            rules={[{ required: true, message: '请输入最大内存使用' }]}
          >
            <InputNumber min={128} max={16384} style={{ width: '100%' }} />
          </Form.Item>

          <Form.Item>
            <Space>
              <Button type="primary" onClick={handleSave} loading={saving}>
                保存设置
              </Button>
              <Button onClick={handleReset}>重置默认</Button>
            </Space>
          </Form.Item>
        </Form>
      </Card>
    </div>
  );
}
