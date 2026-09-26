import { history, useIntl } from '@umijs/max';
import { Button } from 'antd';
import React from 'react';
import { PageStatePanel } from '@/components';

const ForbiddenPage: React.FC = () => {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  return (
    <div style={{ padding: 24 }}>
      <PageStatePanel
        tone="error"
        badgeText="403"
        title={formatMessage('pages.403.title')}
        description={formatMessage('pages.403.subTitle')}
        actions={
          <Button type="primary" onClick={() => history.push('/')}>
            {formatMessage('pages.403.buttonText')}
          </Button>
        }
      />
    </div>
  );
};

export default ForbiddenPage;
