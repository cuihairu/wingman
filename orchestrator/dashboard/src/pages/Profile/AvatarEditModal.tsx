import React, { useState } from 'react';
import { useIntl } from '@umijs/max';
import { Avatar, Form, Input, message, Modal, Upload } from 'antd';
import { InboxOutlined, UserOutlined } from '@ant-design/icons';
import { updateMyProfile } from '@/services/api/me';
import { buildAvatarObjectKey, uploadAsset } from '@/services/api/storage';

// 头像编辑弹窗：表单/上传态组件内部持有，父级只持开关与当前头像值；
// 保存成功后经 onSaved 回调通知父级刷新资料。
const AvatarEditModal: React.FC<{
  open: boolean;
  avatar?: string;
  onClose: () => void;
  onSaved: () => void | Promise<void>;
}> = ({ open, avatar, onClose, onSaved }) => {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const [form] = Form.useForm();
  const [uploading, setUploading] = useState(false);

  const persistAvatar = async (avatarValue: string, closeModal = true) => {
    await updateMyProfile({ avatar: avatarValue });
    message.success(formatMessage('profile.avatar.success'));
    if (closeModal) {
      onClose();
    }
    await onSaved();
  };

  const handleSubmit = async () => {
    const values = await form.validateFields();
    try {
      await persistAvatar(values.avatar);
    } catch (error) {
      message.error(formatMessage('profile.update.error'));
    }
  };

  const handleUpload = async (options: any) => {
    const file = options?.file as File | undefined;
    if (!file) {
      options?.onError?.(new Error('missing file'));
      return;
    }

    setUploading(true);
    try {
      const uploaded = await uploadAsset(file, { path: buildAvatarObjectKey(file) });
      const avatarUrl = uploaded?.URL || '';
      if (!avatarUrl) {
        throw new Error('missing avatar url');
      }
      form.setFieldsValue({ avatar: avatarUrl });
      await persistAvatar(avatarUrl);
      options?.onSuccess?.(uploaded);
    } catch (error) {
      message.error(formatMessage('profile.update.error'));
      options?.onError?.(error);
    } finally {
      setUploading(false);
    }
  };

  return (
    <Modal
      open={open}
      forceRender
      title={formatMessage('profile.avatar.modal.title')}
      onCancel={onClose}
      onOk={() => handleSubmit().catch(() => {})}
      okText={formatMessage('profile.avatar.modal.submit')}
      confirmLoading={uploading}
    >
      <Form form={form} layout="vertical" initialValues={{ avatar }}>
        <Form.Item noStyle dependencies={['avatar']}>
          {({ getFieldValue }) => {
            const avatarValue = getFieldValue('avatar') || avatar;
            return (
              <div className="avatar-upload-preview">
                <Avatar
                  size={72}
                  src={avatarValue}
                  icon={!avatarValue ? <UserOutlined /> : undefined}
                />
                <div>
                  <div className="avatar-upload-preview__title">
                    {formatMessage('profile.avatar.modal.title')}
                  </div>
                  <div className="avatar-upload-preview__hint">
                    {formatMessage('profile.avatar.modal.previewHint')}
                  </div>
                </div>
              </div>
            );
          }}
        </Form.Item>
        <Form.Item label={formatMessage('profile.avatar.modal.uploadLabel')}>
          <Upload.Dragger
            accept="image/*"
            maxCount={1}
            showUploadList={false}
            customRequest={handleUpload}
            disabled={uploading}
          >
            <p className="ant-upload-drag-icon">
              <InboxOutlined />
            </p>
            <p className="ant-upload-text">{formatMessage('profile.avatar.modal.dragText')}</p>
            <p className="ant-upload-hint">{formatMessage('profile.avatar.modal.uploadHint')}</p>
          </Upload.Dragger>
        </Form.Item>
        <Form.Item
          name="avatar"
          label={formatMessage('profile.avatar.modal.label')}
          rules={[
            { required: true, message: formatMessage('profile.avatar.modal.required') },
            {
              validator(_, value) {
                if (!value) {
                  return Promise.reject(new Error(formatMessage('profile.avatar.modal.required')));
                }
                try {
                  new URL(value);
                  return Promise.resolve();
                } catch (error) {
                  return Promise.reject(new Error(formatMessage('profile.avatar.modal.invalid')));
                }
              },
            },
          ]}
        >
          <Input placeholder={formatMessage('profile.avatar.modal.placeholder')} />
        </Form.Item>
      </Form>
    </Modal>
  );
};

export default AvatarEditModal;
