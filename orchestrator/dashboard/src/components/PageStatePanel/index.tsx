import { Alert, Result, Button, Space } from 'antd';
import React from 'react';

export interface PageStatePanelProps {
  tone?: 'info' | 'success' | 'warning' | 'error';
  badgeText?: string;
  title: string;
  description?: string;
  actions?: React.ReactNode;
  extra?: React.ReactNode;
}

const PageStatePanel: React.FC<PageStatePanelProps> = ({
  tone = 'info',
  badgeText,
  title,
  description,
  actions,
  extra,
}) => {
  const alertType = tone === 'error' ? 'error' : tone === 'warning' ? 'warning' : tone === 'success' ? 'success' : 'info';
  const resultStatus = tone === 'error' ? 'error' : tone === 'warning' ? 'warning' : tone === 'success' ? 'success' : 'info';

  return (
    <Result
      status={resultStatus}
      title={badgeText ? (
        <Space>
          <span style={{ fontSize: 48, fontWeight: 'bold', opacity: 0.3 }}>{badgeText}</span>
          <span>{title}</span>
        </Space>
      ) : title}
      subTitle={description}
      extra={actions}
    >
      {extra}
    </Result>
  );
};

export default PageStatePanel;
