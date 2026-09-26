/**
 * 远端文件浏览器（公共件，设计 §15 SSH/SFTP 树）。
 *
 * 纯受控组件：文件系统对象由 useGuacamoleSession 上报后经 props 注入，
 * 这里不含任何连接逻辑。能力边界即协议边界（1.5.x 线协议只有 get/put）：
 * 列目录 / 下载 / 上传；没有删除/重命名——不是 UI 取舍，是协议层不存在。
 *
 * 第二版（§15.1）：大目录浏览器侧分页（协议一次 get 返回整个目录体，无
 * 服务端分页）、传输进度与有限重试、下载/上传结果经 audit 回调上报
 * （列表高频不审计）。权限沿用 §14/§15 先例（收发不对称）：监看模式可
 * 浏览与下载（只读动作），上传入口仅接管模式渲染。
 */
import { useCallback, useEffect, useRef, useState } from 'react';
import { Button, Progress, Space, Table, Tooltip, Typography } from 'antd';
import {
  ArrowUpOutlined,
  DownloadOutlined,
  FileOutlined,
  FolderFilled,
  ReloadOutlined,
  UploadOutlined,
} from '@ant-design/icons';
import type { ColumnsType } from 'antd/es/table';
import {
  downloadRemoteFile,
  formatRemoteSize,
  joinRemotePath,
  listRemoteDirectory,
  uploadRemoteFile,
  withRetry,
} from './filesystem';
import type { RemoteFileEntry, RemoteFileOpAudit, RemoteFileSystemObject } from './types';

const { Text } = Typography;

/** 进行中的传输（进度条渲染；key = kind:name） */
interface ActiveTransfer {
  kind: 'download' | 'upload';
  current: number;
  total?: number;
}

export interface RemoteFileBrowserProps {
  /** 会话上报的文件系统对象（SSH=SFTP；RDP 驱动器同协议） */
  fs: RemoteFileSystemObject;
  /** true 监看：隐藏上传入口（下载/浏览仍可用） */
  readOnly: boolean;
  /** 瞬时反馈（上传完成/失败、列目录失败等） */
  notify: (kind: 'success' | 'error', text: string) => void;
  /**
   * 文件操作审计回调（下载/上传的最终结果；list 不上报）。可选：无消费者
   * （cockpit 二方未接审计端点）时不上报。票据由容器层注入。
   */
  audit?: (op: RemoteFileOpAudit) => void;
  /** 每页行数（浏览器侧分页；协议无服务端分页），默认 50 */
  pageSize?: number;
  /** 列表滚动区高度（px），默认 240 */
  height?: number;
}

/** 触发浏览器下载（与面板 onfile 下发同款实现）。 */
function saveBlobDownload(filename: string, blob: Blob) {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}

/** RemoteFileBrowser 远端文件树浏览器（列目录/下载/上传）。 */
export default function RemoteFileBrowser({
  fs,
  readOnly,
  notify,
  audit,
  pageSize = 50,
  height = 240,
}: RemoteFileBrowserProps) {
  // 路径以段表示（根 = []），面包屑与导航共用
  const [segments, setSegments] = useState<string[]>([]);
  const [entries, setEntries] = useState<RemoteFileEntry[]>([]);
  const [loading, setLoading] = useState(false);
  const [transfers, setTransfers] = useState<Record<string, ActiveTransfer>>({});
  const fileInputRef = useRef<HTMLInputElement>(null);

  const path = segments.length === 0 ? '/' : `/${segments.join('/')}`;

  const setTransfer = (key: string, t: ActiveTransfer | null) => {
    setTransfers((prev) => {
      const next = { ...prev };
      if (t === null) {
        delete next[key];
      } else {
        next[key] = t;
      }
      return next;
    });
  };

  const load = useCallback(
    async (dir: string) => {
      setLoading(true);
      try {
        const list = await listRemoteDirectory(fs, dir);
        // 目录在前、名称升序（guacd listing 的顺序不保证）
        list.sort(
          (a, b) => Number(b.directory) - Number(a.directory) || a.name.localeCompare(b.name),
        );
        setEntries(list);
      } catch (e) {
        notify('error', e instanceof Error ? e.message : '目录读取失败');
      } finally {
        setLoading(false);
      }
    },
    [fs, notify],
  );

  useEffect(() => {
    load(path);
  }, [load, path]);

  const enter = (name: string) => setSegments((prev) => [...prev, name]);
  const jump = (depth: number) => setSegments((prev) => prev.slice(0, depth));

  const handleDownload = async (entry: RemoteFileEntry) => {
    const key = `download:${entry.name}`;
    const fullPath = joinRemotePath(path, entry.name);
    setTransfer(key, { kind: 'download', current: 0, total: entry.size });
    try {
      const { value, attempts } = await withRetry(
        () =>
          downloadRemoteFile(fs, fullPath, {
            onProgress: (current) =>
              setTransfer(key, { kind: 'download', current, total: entry.size }),
          }),
        { attempts: 2 },
      );
      saveBlobDownload(value.filename, value.blob);
      notify('success', `已下载 ${value.filename}`);
      audit?.({
        action: 'download',
        path: fullPath,
        result: 'ok',
        sizeBytes: value.blob.size,
        attempts,
      });
    } catch (e) {
      const message = e instanceof Error ? e.message : `${entry.name} 下载失败`;
      notify('error', message);
      audit?.({
        action: 'download',
        path: fullPath,
        result: 'fail',
        sizeBytes: entry.size,
        error: message,
      });
    } finally {
      setTransfer(key, null);
    }
  };

  const onFileChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = e.target.files;
    e.target.value = ''; // 复位以允许连续选同一文件（与工具栏上传一致）
    if (!files || files.length === 0) {
      return;
    }
    for (const file of Array.from(files)) {
      const key = `upload:${file.name}`;
      const fullPath = joinRemotePath(path, file.name);
      setTransfer(key, { kind: 'upload', current: 0, total: file.size });
      try {
        const { attempts } = await withRetry(
          () =>
            uploadRemoteFile(fs, file, path, {
              onProgress: (current, total) =>
                setTransfer(key, { kind: 'upload', current, total: total ?? file.size }),
            }),
          { attempts: 2 },
        );
        notify('success', `${file.name} 上传完成`);
        audit?.({
          action: 'upload',
          path: fullPath,
          result: 'ok',
          sizeBytes: file.size,
          attempts,
        });
      } catch (err) {
        const message = err instanceof Error ? err.message : `${file.name} 上传失败`;
        notify('error', message);
        audit?.({
          action: 'upload',
          path: fullPath,
          result: 'fail',
          sizeBytes: file.size,
          error: message,
        });
      } finally {
        setTransfer(key, null);
      }
    }
    load(path);
  };

  const columns: ColumnsType<RemoteFileEntry> = [
    {
      title: '名称',
      dataIndex: 'name',
      render: (name: string, entry) =>
        entry.directory ? (
          <Button
            type="link"
            size="small"
            icon={<FolderFilled />}
            onClick={() => enter(name)}
            style={{ padding: 0 }}
          >
            {name}
          </Button>
        ) : (
          <Space size="small">
            <FileOutlined />
            <Text style={{ fontSize: 13 }}>{name}</Text>
          </Space>
        ),
    },
    {
      title: '大小',
      dataIndex: 'size',
      width: 90,
      render: (size: number, entry) => (
        <Text type="secondary" style={{ fontSize: 12 }}>
          {entry.directory ? '' : formatRemoteSize(size)}
        </Text>
      ),
    },
    {
      title: '操作',
      key: 'actions',
      width: 90,
      render: (_, entry) =>
        entry.directory ? null : (
          <Tooltip title="下载到本机">
            <Button
              type="text"
              size="small"
              icon={<DownloadOutlined />}
              data-testid={`remote-fs-download-${entry.name}`}
              onClick={() => handleDownload(entry)}
            />
          </Tooltip>
        ),
    },
  ];

  return (
    <div data-testid="remote-file-browser">
      {/* 路径导航 + 刷新 + 上传 */}
      <Space
        style={{ width: '100%', justifyContent: 'space-between', marginBottom: 8 }}
        align="center"
      >
        <Space size="small" wrap>
          {segments.length > 0 && (
            <Button
              size="small"
              icon={<ArrowUpOutlined />}
              onClick={() => jump(segments.length - 1)}
              title="上一级"
            />
          )}
          <BreadcrumbNav segments={segments} onJump={jump} />
        </Space>
        <Space size="small">
          <Button size="small" icon={<ReloadOutlined />} onClick={() => load(path)}>
            刷新
          </Button>
          {!readOnly && (
            <>
              <Button
                size="small"
                icon={<UploadOutlined />}
                onClick={() => fileInputRef.current?.click()}
              >
                上传到当前目录
              </Button>
              <input
                ref={fileInputRef}
                type="file"
                multiple
                hidden
                onChange={onFileChange}
                data-testid="remote-fs-upload-input"
              />
            </>
          )}
        </Space>
      </Space>
      {Object.entries(transfers).length > 0 && (
        <div data-testid="remote-fs-transfers" style={{ marginBottom: 8 }}>
          {Object.entries(transfers).map(([key, t]) => {
            const label = `${t.kind === 'download' ? '下载' : '上传'} ${key.slice(key.indexOf(':') + 1)}`;
            const pct = t.total
              ? Math.min(100, Math.floor((t.current / t.total) * 100))
              : undefined;
            return (
              <div key={key} style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                <Text style={{ fontSize: 12, flexShrink: 0 }} type="secondary">
                  {label}
                </Text>
                <div style={{ flex: 1 }}>
                  <Progress
                    percent={pct ?? 100}
                    showInfo={pct !== undefined}
                    status={pct === undefined ? 'active' : 'normal'}
                    size="small"
                    data-testid={`remote-fs-progress-${key}`}
                  />
                </div>
                {pct === undefined && (
                  <Text style={{ fontSize: 12 }} type="secondary">
                    {formatRemoteSize(t.current)}
                  </Text>
                )}
              </div>
            );
          })}
        </div>
      )}
      <Table<RemoteFileEntry>
        size="small"
        rowKey={(r) => r.name}
        columns={columns}
        dataSource={entries}
        loading={loading}
        pagination={{
          pageSize,
          hideOnSinglePage: true,
          showSizeChanger: false,
          showTotal: (total) => `共 ${total} 项`,
        }}
        scroll={{ y: height }}
        locale={{ emptyText: '（空目录）' }}
      />
    </div>
  );
}

/** 面包屑：根 + 各级目录，点任意级跳转。 */
function BreadcrumbNav({
  segments,
  onJump,
}: {
  segments: string[];
  onJump: (depth: number) => void;
}) {
  return (
    <Text style={{ fontSize: 13 }}>
      <Button type="link" size="small" style={{ padding: 0 }} onClick={() => onJump(0)}>
        根目录
      </Button>
      {segments.map((seg, i) => (
        <span key={`${i}-${seg}`}>
          {' / '}
          {i < segments.length - 1 ? (
            <Button type="link" size="small" style={{ padding: 0 }} onClick={() => onJump(i + 1)}>
              {seg}
            </Button>
          ) : (
            seg
          )}
        </span>
      ))}
    </Text>
  );
}
