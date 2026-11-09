import React, { useEffect, useState } from 'react';
import { useIntl } from '@umijs/max';
import { Alert, Form, Input, message, Modal } from 'antd';
import { changeMyPassword } from '@/services/api/me';

// 修改密码弹窗：表单/提交态组件内部持有，父级只持开关。
const PasswordChangeModal: React.FC<{ open: boolean; onClose: () => void }> = ({
  open,
  onClose,
}) => {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const [form] = Form.useForm();
  const [submitting, setSubmitting] = useState(false);

  // 打开即重置（拆分前由 showPasswordModal 在父级 resetFields）
  useEffect(() => {
    if (open) {
      form.resetFields();
    }
  }, [open, form]);

  const handleFinish = async (values: any) => {
    setSubmitting(true);
    try {
      await changeMyPassword({
        current: values.current,
        password: values.password,
      });
      message.success(formatMessage('profile.password.success'));
      onClose();
      form.resetFields();
    } catch (error) {
      message.error(formatMessage('profile.password.error'));
      throw error;
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <Modal
      open={open}
      forceRender
      title={formatMessage('profile.password.modal.title')}
      onCancel={() => {
        onClose();
        form.resetFields();
      }}
      onOk={() => form.submit()}
      okText={formatMessage('profile.password.modal.submit')}
      confirmLoading={submitting}
    >
      <Alert
        message={formatMessage('profile.password.modal.warning')}
        type="warning"
        showIcon
        style={{ marginBottom: 16 }}
      />
      <Form layout="vertical" form={form} onFinish={handleFinish}>
        <Form.Item
          name="current"
          label={formatMessage('profile.password.current')}
          rules={[{ required: true }]}
        >
          <Input.Password placeholder={formatMessage('profile.password.current.placeholder')} />
        </Form.Item>
        <Form.Item
          name="password"
          label={formatMessage('profile.password.new')}
          rules={[
            { required: true },
            { min: 6, message: formatMessage('profile.password.min.length') },
          ]}
        >
          <Input.Password placeholder={formatMessage('profile.password.new.placeholder')} />
        </Form.Item>
        <Form.Item
          name="confirm"
          label={formatMessage('profile.password.confirm')}
          rules={[
            { required: true },
            ({ getFieldValue }) => ({
              validator(_, value) {
                if (!value || getFieldValue('password') === value) {
                  return Promise.resolve();
                }
                return Promise.reject(new Error(formatMessage('profile.password.mismatch')));
              },
            }),
          ]}
        >
          <Input.Password placeholder={formatMessage('profile.password.confirm.placeholder')} />
        </Form.Item>
      </Form>
    </Modal>
  );
};

export default PasswordChangeModal;
