import React, { useEffect, useState } from 'react';
import { useIntl, useNavigate } from '@umijs/max';
import { Alert, Button, Form, Input, message, Modal, Space } from 'antd';
import { CopyOutlined } from '@ant-design/icons';
import { createFeedback } from '@/services/api/support';
import type { PermissionApplyItem } from './shared';

// 权限申请弹窗：生成申请文案（复制兜底）+ 经反馈渠道提交；表单/提交态
// 组件内部持有，父级只持开关与目标权限项。
const PermissionApplyModal: React.FC<{
  open: boolean;
  target: PermissionApplyItem | null;
  username?: string;
  onClose: () => void;
}> = ({ open, target, username, onClose }) => {
  const intl = useIntl();
  const navigate = useNavigate();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const [form] = Form.useForm();
  const [submitting, setSubmitting] = useState(false);

  // 打开即重置（拆分前由 handleOpenApplyPermission 在父级 resetFields）
  useEffect(() => {
    if (open) {
      form.resetFields();
    }
  }, [open, form]);

  const buildApplyPermissionContent = (reason: string) => {
    if (!target) return '';
    return [
      `${formatMessage('profile.permissions.apply.content.applicant')}: ${username || '-'}`,
      `${formatMessage('profile.permissions.apply.content.permission')}: ${target.name}`,
      `${formatMessage('profile.permissions.apply.content.permission.key')}: ${target.resource}:${target.action}`,
      `${formatMessage('profile.permissions.apply.content.permission.id')}: ${target.id}`,
      `${formatMessage('profile.permissions.apply.content.reason')}: ${reason || '-'}`,
    ].join('\n');
  };

  const handleCopyPermissionApplyContent = async () => {
    const values = await form.validateFields();
    const content = buildApplyPermissionContent(values.reason);
    await navigator.clipboard.writeText(content);
    message.success(formatMessage('profile.permissions.apply.copy.success'));
  };

  const handleSubmitPermissionApply = async () => {
    const values = await form.validateFields();
    const content = buildApplyPermissionContent(values.reason);
    setSubmitting(true);
    try {
      await createFeedback({
        category: 'permission_request',
        content,
        priority: 'normal',
        source: 'profile_permission_apply',
      });
      message.success(formatMessage('profile.permissions.apply.submit.success'));
      onClose();
    } catch (error) {
      // 部分环境可能未开放反馈写入，降级为复制文案+人工提交流程
      await navigator.clipboard.writeText(content);
      message.warning(formatMessage('profile.permissions.apply.submit.fallback'));
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <Modal
      open={open}
      forceRender
      title={formatMessage('profile.permissions.apply.modal.title')}
      onCancel={() => {
        onClose();
        form.resetFields();
      }}
      onOk={() => handleSubmitPermissionApply().catch(() => {})}
      okText={formatMessage('profile.permissions.apply.modal.submit')}
      confirmLoading={submitting}
    >
      <Space direction="vertical" style={{ width: '100%' }} size={12}>
        {target && (
          <Alert
            showIcon
            type="info"
            message={target.name}
            description={`${target.resource}:${target.action}`}
          />
        )}
        <Form form={form} layout="vertical">
          <Form.Item
            name="reason"
            label={formatMessage('profile.permissions.apply.reason')}
            rules={[
              {
                required: true,
                message: formatMessage('profile.permissions.apply.reason.required'),
              },
            ]}
          >
            <Input.TextArea
              rows={4}
              placeholder={formatMessage('profile.permissions.apply.reason.placeholder')}
            />
          </Form.Item>
        </Form>
        <Space>
          <Button
            icon={<CopyOutlined />}
            onClick={() => handleCopyPermissionApplyContent().catch(() => {})}
          >
            {formatMessage('profile.permissions.apply.copy')}
          </Button>
          <Button
            onClick={() => {
              navigate('/support/feedback');
            }}
          >
            {formatMessage('profile.permissions.apply.goto.feedback')}
          </Button>
        </Space>
      </Space>
    </Modal>
  );
};

export default PermissionApplyModal;
