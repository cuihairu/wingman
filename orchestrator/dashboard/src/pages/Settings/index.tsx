import React, { useEffect, useMemo, useState } from 'react';
import { Button, Card, Form, Input, InputNumber, Select, Space, Tag, App, Spin } from 'antd';
import { PageContainer } from '@ant-design/pro-components';
import { useModel } from '@umijs/max';
import { fetchJSON } from '@/services/core/http';

interface ServerSettings {
  serverPort?: string | number;
  logLevel?: string;
  maxScripts?: string | number;
  [key: string]: any;
}

export default function SettingsPage() {
  const { message } = App.useApp();
  const initialState = useModel('@@initialState');
  const accessTokens = (initialState?.initialState?.currentUser as any)?.access || '';
  const canAdmin = useMemo(() => {
    const set = new Set(accessTokens.split(',').map((t: string) => t.trim().toLowerCase()));
    return set.has('*') || set.has('admin');
  }, [accessTokens]);

  const [settings, setSettings] = useState<ServerSettings>({});
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);
  const [form] = Form.useForm();

  const load = async () => {
    setLoading(true);
    try {
      const resp = await fetchJSON<{ success: boolean; data: ServerSettings }>('/api/settings');
      if (resp?.success) {
        setSettings(resp.data || {});
        form.setFieldsValue({
          logLevel: resp.data?.logLevel || 'info',
          maxScripts: Number(resp.data?.maxScripts ?? 10),
          serverPort: resp.data?.serverPort ?? '',
        });
      }
    } catch (err: any) {
      message.error(err?.message || '加载设置失败');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    load();
  }, []);

  const onSave = async () => {
    try {
      const values = await form.validateFields();
      setSaving(true);
      await fetchJSON('/api/settings', {
        method: 'PUT',
        body: JSON.stringify({
          logLevel: values.logLevel,
          maxScripts: String(values.maxScripts),
        }),
      });
      message.success('设置已保存');
      load();
    } catch (err: any) {
      if (err?.errorFields) return;
      message.error(err?.message || '保存失败（可能需要管理员权限）');
    } finally {
      setSaving(false);
    }
  };

  return (
    <PageContainer>
      <Card title="服务器设置" extra={!canAdmin && <Tag>只读（需管理员）</Tag>}>
        <Spin spinning={loading}>
          <Form form={form} layout="vertical" style={{ maxWidth: 480 }} disabled={!canAdmin}>
            <Form.Item label="HTTP 端口（只读）" name="serverPort">
              <Input disabled placeholder="9527" />
            </Form.Item>
            <Form.Item
              label="日志级别"
              name="logLevel"
              rules={[{ required: true, message: '请选择日志级别' }]}
            >
              <Select
                options={[
                  { label: 'Debug', value: 'debug' },
                  { label: 'Info', value: 'info' },
                  { label: 'Warn', value: 'warn' },
                  { label: 'Error', value: 'error' },
                ]}
              />
            </Form.Item>
            <Form.Item
              label="最大并发脚本数"
              name="maxScripts"
              rules={[{ required: true, message: '请输入数量' }]}
            >
              <InputNumber min={1} max={100} style={{ width: '100%' }} />
            </Form.Item>
            <Form.Item>
              <Space>
                <Button type="primary" loading={saving} onClick={onSave} disabled={!canAdmin}>
                  保存
                </Button>
                <Button onClick={load}>刷新</Button>
              </Space>
            </Form.Item>
          </Form>

          {Object.keys(settings).length > 0 && (
            <Card type="inner" title="原始键值（只读）" size="small" style={{ marginTop: 16 }}>
              <pre style={{ margin: 0, fontSize: 12, maxHeight: 240, overflow: 'auto' }}>
                {JSON.stringify(settings, null, 2)}
              </pre>
            </Card>
          )}
        </Spin>
      </Card>

      <Card title="调试（直连模式）" style={{ marginTop: 16 }}>
        <p style={{ color: '#666' }}>
          Lua 调试由 VSCode EmmyLua 直连 runtime 调试端口（默认 9966），Go server 不中转。
          各 agent 的调试端点见 <code>GET /api/debugger/info</code>。
        </p>
      </Card>
    </PageContainer>
  );
}
