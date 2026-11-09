import React from 'react';
import { useIntl } from '@umijs/max';
import { Alert, Button, Modal, Space, Table, Tag } from 'antd';
import type { BatchSummary } from '@/services/wingman';

// 批量操作结果：汇总 + 逐台明细（含离线/失败原因）。展示为纯受控组件，
// 结果数据与关闭动作均来自父级。
const BatchResultModal: React.FC<{
  result: BatchSummary | null;
  onClose: () => void;
}> = ({ result, onClose }) => {
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  return (
    <Modal
      open={result !== null}
      title={formatMessage('pages.agents.batchResultTitle')}
      width={620}
      footer={
        <Button type="primary" onClick={onClose}>
          {formatMessage('pages.common.close')}
        </Button>
      }
      onCancel={onClose}
    >
      {result && (
        <Space direction="vertical" style={{ width: '100%' }} size="middle">
          <Alert
            type={result.failed === 0 ? 'success' : result.succeeded === 0 ? 'error' : 'warning'}
            showIcon
            message={intl.formatMessage(
              { id: 'pages.agents.batchResultSummary' },
              {
                total: result.total,
                succeeded: result.succeeded,
                failed: result.failed,
              },
            )}
          />
          <Table
            size="small"
            rowKey="agentId"
            dataSource={result.results}
            pagination={result.results.length > 10 ? { pageSize: 10 } : false}
            columns={[
              { title: 'Agent ID', dataIndex: 'agentId', ellipsis: true },
              {
                title: formatMessage('pages.agents.resultColumn'),
                dataIndex: 'success',
                width: 90,
                render: (ok: boolean) =>
                  ok ? (
                    <Tag color="success">{formatMessage('pages.common.success')}</Tag>
                  ) : (
                    <Tag color="error">{formatMessage('pages.common.failure')}</Tag>
                  ),
              },
              {
                title: formatMessage('pages.agents.failureReason'),
                dataIndex: 'error',
                ellipsis: true,
                render: (text?: string) => text || '-',
              },
            ]}
          />
        </Space>
      )}
    </Modal>
  );
};

export default BatchResultModal;
