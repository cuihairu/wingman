import React, { useState } from 'react';
import { Alert, Button, Card, Form, Input, Select, Space, Typography, message } from 'antd';
import { PageContainer } from '@ant-design/pro-components';
import { createFeedback } from '@/services/api/support';

const { Paragraph } = Typography;

export default function SupportFeedbackPage() {
  const [form] = Form.useForm();
  const [submitting, setSubmitting] = useState(false);

  const submit = async () => {
    const values = await form.validateFields();
    setSubmitting(true);
    try {
      await createFeedback({
        category: values.category,
        priority: values.priority,
        content: values.content,
        source: 'support_feedback_page',
      });
      message.success('反馈已提交');
      form.resetFields();
    } catch (error: any) {
      message.error(error?.message || '提交失败');
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <PageContainer>
      <Card title="反馈中心">
        <Space direction="vertical" size={16} style={{ width: '100%' }}>
          <Alert
            type="info"
            showIcon
            message="提交后会生成站内消息"
            description="权限申请、问题反馈和功能建议都会写入后端反馈记录，并在消息中心留下可追踪通知。"
          />
          <Form
            form={form}
            layout="vertical"
            initialValues={{ category: 'general', priority: 'normal' }}
          >
            <Form.Item name="category" label="反馈类型" rules={[{ required: true }]}>
              <Select
                options={[
                  { label: '一般反馈', value: 'general' },
                  { label: '权限申请', value: 'permission_request' },
                  { label: '问题报告', value: 'bug' },
                  { label: '功能建议', value: 'feature' },
                ]}
              />
            </Form.Item>
            <Form.Item name="priority" label="优先级" rules={[{ required: true }]}>
              <Select
                options={[
                  { label: '普通', value: 'normal' },
                  { label: '高', value: 'high' },
                  { label: '紧急', value: 'urgent' },
                ]}
              />
            </Form.Item>
            <Form.Item
              name="content"
              label="内容"
              rules={[
                { required: true, message: '请输入反馈内容' },
                { min: 5, message: '内容至少 5 个字符' },
              ]}
            >
              <Input.TextArea rows={8} placeholder="请描述背景、期望结果、复现步骤或权限使用场景。" />
            </Form.Item>
            <Paragraph type="secondary">
              反馈会进入后端持久化记录；如果是权限申请，建议附上权限标识和业务理由。
            </Paragraph>
            <Button type="primary" loading={submitting} onClick={() => submit().catch(() => {})}>
              提交反馈
            </Button>
          </Form>
        </Space>
      </Card>
    </PageContainer>
  );
}
