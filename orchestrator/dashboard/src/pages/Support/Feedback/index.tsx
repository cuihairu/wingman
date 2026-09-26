import { useIntl } from '@umijs/max';
import React, { useState } from 'react';
import { Alert, Button, Card, Form, Input, Select, Space, Typography, message } from 'antd';
import { PageContainer } from '@ant-design/pro-components';
import { createFeedback } from '@/services/api/support';

const { Paragraph } = Typography;

export default function SupportFeedbackPage() {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
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
      message.success(formatMessage('pages.supportFeedback.submitted'));
      form.resetFields();
    } catch (error: any) {
      message.error(error?.message || formatMessage('pages.supportFeedback.submitFailed'));
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <PageContainer>
      <Card title={formatMessage('pages.supportFeedback.title')}>
        <Space direction="vertical" size={16} style={{ width: '100%' }}>
          <Alert
            type="info"
            showIcon
            message={formatMessage('pages.supportFeedback.alertMessage')}
            description={formatMessage('pages.supportFeedback.alertDescription')}
          />
          <Form
            form={form}
            layout="vertical"
            initialValues={{ category: 'general', priority: 'normal' }}
          >
            <Form.Item
              name="category"
              label={formatMessage('pages.supportFeedback.category')}
              rules={[{ required: true }]}
            >
              <Select
                options={[
                  {
                    label: formatMessage('pages.supportFeedback.categoryGeneral'),
                    value: 'general',
                  },
                  {
                    label: formatMessage('pages.supportFeedback.categoryPermission'),
                    value: 'permission_request',
                  },
                  { label: formatMessage('pages.supportFeedback.categoryBug'), value: 'bug' },
                  {
                    label: formatMessage('pages.supportFeedback.categoryFeature'),
                    value: 'feature',
                  },
                ]}
              />
            </Form.Item>
            <Form.Item
              name="priority"
              label={formatMessage('pages.supportFeedback.priority')}
              rules={[{ required: true }]}
            >
              <Select
                options={[
                  { label: formatMessage('pages.supportFeedback.priorityNormal'), value: 'normal' },
                  { label: formatMessage('pages.supportFeedback.priorityHigh'), value: 'high' },
                  { label: formatMessage('pages.supportFeedback.priorityUrgent'), value: 'urgent' },
                ]}
              />
            </Form.Item>
            <Form.Item
              name="content"
              label={formatMessage('pages.supportFeedback.content')}
              rules={[
                { required: true, message: formatMessage('pages.supportFeedback.contentRequired') },
                { min: 5, message: formatMessage('pages.supportFeedback.contentMinLength') },
              ]}
            >
              <Input.TextArea
                rows={8}
                placeholder={formatMessage('pages.supportFeedback.contentPlaceholder')}
              />
            </Form.Item>
            <Paragraph type="secondary">
              {formatMessage('pages.supportFeedback.persistHint')}
            </Paragraph>
            <Button type="primary" loading={submitting} onClick={() => submit().catch(() => {})}>
              {formatMessage('pages.supportFeedback.submit')}
            </Button>
          </Form>
        </Space>
      </Card>
    </PageContainer>
  );
}
