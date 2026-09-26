/**
 * 远端文件浏览器（公共件，设计 §15 SSH/SFTP 树第一版）。
 *
 * 纯受控组件：文件系统对象由 useGuacamoleSession 上报后经 props 注入，
 * 这里不含任何连接逻辑。能力边界即协议边界（1.5.x 线协议只有 get/put）：
 * 列目录 / 下载 / 上传；没有删除/重命名——不是 UI 取舍，是协议层不存在。
 *
 * 权限沿用 §14/§15 先例（收发不对称）：监看模式可浏览与下载（只读动作），
 * 上传入口仅接管模式渲染（网关不解析指令，UI 层不给入口是无歧义的约束）。
 */
import { useCallback, useEffect, useRef, useState } from 'react';
import { Button, Space, Table, Tooltip, Typography } from 'antd';
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
} from './filesystem';
import type { RemoteFileEntry, RemoteFileSystemObject } from './types';

const { Text } = Typography;

export interface RemoteFileBrowserProps {
  /** 会话上报的文件系统对象（SSH=SFTP；RDP 驱动器同协议） */
  fs: RemoteFileSystemObject;
  /** true 监看：隐藏上传入口（下载/浏览仍可用） */
  readOnly: boolean;
  /** 瞬时反馈（上传完成/失败、列目录失败等） */
  notify: (kind: 'success' | 'error', text: string) => void;
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
  height = 240,
}: RemoteFileBrowserProps) {
  // 路径以段表示（根 = []），面包屑与导航共用
  const [segments, setSegments] = useState<string[]>([]);
  const [entries, setEntries] = useState<RemoteFileEntry[]>([]);
  const [loading, setLoading] = useState(false);
  const fileInputRef = useRef<HTMLInputElement>(null);

  const path = segments.length === 0 ? '/' : `/${segments.join('/')}`;

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
    try {
      const { filename, blob } = await downloadRemoteFile(fs, joinRemotePath(path, entry.name));
      saveBlobDownload(filename, blob);
      notify('success', `已下载 ${filename}`);
    } catch (e) {
      notify('error', e instanceof Error ? e.message : `${entry.name} 下载失败`);
    }
  };

  const onFileChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = e.target.files;
    e.target.value = ''; // 复位以允许连续选同一文件（与工具栏上传一致）
    if (!files || files.length === 0) {
      return;
    }
    for (const file of Array.from(files)) {
      try {
        await uploadRemoteFile(fs, file, path);
        notify('success', `${file.name} 上传完成`);
      } catch (err) {
        notify('error', err instanceof Error ? err.message : `${file.name} 上传失败`);
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
      <Table<RemoteFileEntry>
        size="small"
        rowKey={(r) => r.name}
        columns={columns}
        dataSource={entries}
        loading={loading}
        pagination={false}
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
