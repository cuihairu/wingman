import React, { useEffect, useState } from 'react';
import { useAccess, useIntl } from '@umijs/max';
import { Alert, Button, message, Modal, Popconfirm, Space, Table } from 'antd';
import { DownloadOutlined, DeleteOutlined, PlayCircleOutlined } from '@ant-design/icons';
import RemoteRecordingPlayer from '@/components/RemoteDesktop/RemoteRecordingPlayer';
import {
  deleteRecording,
  downloadRecording,
  fetchRecordingBlob,
  listRecordings,
  type RecordingEntry,
} from '@/services/remote';
import { formatBytes } from '@/services/wingman';
import { extractErrorMessage } from './shared';

// 会话录像面板（设计 §16）：guacd 录制目录检索；.mjs 下载后可用 guacenc
// 离线转 mp4；删除需 desktop:control；浏览器内回放本地解析 .mjs 不经网关。
// 列表/错误/回放均为组件内部状态，父级只持有面板开关。
const RecordingsModal: React.FC<{ open: boolean; onClose: () => void }> = ({ open, onClose }) => {
  const access = useAccess();
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });
  const canDesktopControl = Boolean(access.canDesktopControl);

  // 浏览器内回放（§16「回放」）：点回放拉取 Blob 本地解析，不经网关
  const [playingRecording, setPlayingRecording] = useState<string | null>(null);
  const [recordings, setRecordings] = useState<RecordingEntry[]>([]);
  const [recordingsError, setRecordingsError] = useState('');
  const [recordingsLoading, setRecordingsLoading] = useState(false);
  const [deletingRecording, setDeletingRecording] = useState<string | null>(null);

  const fetchRecordings = async (silent = false) => {
    if (!silent) {
      setRecordingsLoading(true);
    }
    setRecordingsError('');
    try {
      setRecordings(await listRecordings());
    } catch (e) {
      // 501（未配置）与其他错误都转为面板文案：错误串本身带配置指引
      setRecordingsError(
        e instanceof Error ? e.message : formatMessage('pages.agents.recordings.listFailed'),
      );
    } finally {
      setRecordingsLoading(false);
    }
  };

  // 面板打开即拉取列表（父级按钮只负责开门）
  useEffect(() => {
    if (open) {
      fetchRecordings();
    }
  }, [open]);

  const handleDownloadRecording = async (name: string) => {
    try {
      await downloadRecording(name);
    } catch (e) {
      message.error(
        extractErrorMessage(e, formatMessage('pages.agents.recordings.downloadFailed')),
      );
    }
  };
  const handleDeleteRecording = async (name: string) => {
    setDeletingRecording(name);
    try {
      await deleteRecording(name);
      message.success(intl.formatMessage({ id: 'pages.agents.recordings.deleted' }, { name }));
      await fetchRecordings(true);
    } catch (e) {
      message.error(extractErrorMessage(e, formatMessage('pages.agents.recordings.deleteFailed')));
    } finally {
      setDeletingRecording(null);
    }
  };

  return (
    <>
      <Modal
        open={open}
        title={formatMessage('pages.agents.recordings.title')}
        width={680}
        footer={
          <Button type="primary" onClick={onClose}>
            {formatMessage('pages.agents.recordings.close')}
          </Button>
        }
        onCancel={onClose}
      >
        {recordingsError ? (
          <Alert
            type="warning"
            showIcon
            message={formatMessage('pages.agents.recordings.unavailable')}
            description={recordingsError}
            action={
              <Button size="small" onClick={() => fetchRecordings()}>
                {formatMessage('pages.agents.recordings.retry')}
              </Button>
            }
          />
        ) : (
          <Table
            rowKey="name"
            size="small"
            loading={recordingsLoading}
            dataSource={recordings}
            pagination={{ pageSize: 8, hideOnSinglePage: true }}
            locale={{
              emptyText: formatMessage('pages.agents.recordings.empty'),
            }}
            columns={[
              {
                title: formatMessage('pages.agents.recordings.colName'),
                dataIndex: 'name',
                ellipsis: true,
              },
              {
                title: formatMessage('pages.agents.recordings.colSize'),
                dataIndex: 'sizeBytes',
                width: 90,
                render: (v: number) => formatBytes(v),
              },
              {
                title: formatMessage('pages.agents.recordings.colTime'),
                dataIndex: 'modifiedAt',
                width: 170,
                render: (v: string) => (v ? new Date(v).toLocaleString() : '-'),
              },
              {
                title: formatMessage('pages.agents.recordings.colActions'),
                key: 'actions',
                width: 210,
                render: (_: unknown, r: RecordingEntry) => (
                  <Space>
                    <Button
                      type="link"
                      size="small"
                      icon={<PlayCircleOutlined />}
                      onClick={() => setPlayingRecording(r.name)}
                    >
                      {formatMessage('pages.agents.recordings.play')}
                    </Button>
                    <Button
                      type="link"
                      size="small"
                      icon={<DownloadOutlined />}
                      onClick={() => handleDownloadRecording(r.name)}
                    >
                      {formatMessage('pages.agents.recordings.download')}
                    </Button>
                    {canDesktopControl && (
                      <Popconfirm
                        title={intl.formatMessage(
                          { id: 'pages.agents.recordings.deleteConfirm' },
                          { name: r.name },
                        )}
                        description={formatMessage('pages.agents.recordings.deleteIrreversible')}
                        okText={formatMessage('pages.agents.recordings.delete')}
                        okButtonProps={{ danger: true }}
                        onConfirm={() => handleDeleteRecording(r.name)}
                      >
                        <Button
                          type="link"
                          size="small"
                          danger
                          icon={<DeleteOutlined />}
                          loading={deletingRecording === r.name}
                        >
                          {formatMessage('pages.agents.recordings.delete')}
                        </Button>
                      </Popconfirm>
                    )}
                  </Space>
                ),
              },
            ]}
          />
        )}
      </Modal>

      <RemoteRecordingPlayer
        open={playingRecording !== null}
        name={playingRecording ?? ''}
        load={() => fetchRecordingBlob(playingRecording as string)}
        onClose={() => setPlayingRecording(null)}
      />
    </>
  );
};

export default RecordingsModal;
