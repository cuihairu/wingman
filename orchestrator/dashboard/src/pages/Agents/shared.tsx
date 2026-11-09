import { AndroidOutlined, AppleOutlined, DesktopOutlined, MobileOutlined } from '@ant-design/icons';
import { Space, Tag, Typography } from 'antd';
import type { AgentInfo } from '@/services/wingman';

const { Text } = Typography;

// PlatformTag 平台标识：desktop（含缺省，兼容未上报的旧 agent）→ 桌面、
// android → 安卓、ios → Apple、其余 → 移动设备（docs/android-agent-design.md §3.3）
export const PlatformTag: React.FC<{ platform?: string; count?: number }> = ({
  platform,
  count,
}) => {
  const value = platform || 'desktop';
  const icon =
    value === 'android' ? (
      <AndroidOutlined />
    ) : value === 'ios' ? (
      <AppleOutlined />
    ) : value === 'desktop' ? (
      <DesktopOutlined />
    ) : (
      <MobileOutlined />
    );
  const color = value === 'android' ? 'green' : value === 'desktop' ? 'geekblue' : 'default';
  return (
    <Tag icon={icon} color={color}>
      {value.toUpperCase()}
      {count !== undefined ? ` ${count}` : ''}
    </Tag>
  );
};

// CapabilityTags 能力词汇标签（ADR: Capability System）：agent.register 上报的
// 能力集逐项展示；词汇表外项（server 侧 unknownCapabilities，标 unverified）
// 以警示色区分，不做客户端过滤。
export const CapabilityTags: React.FC<{ record: AgentInfo }> = ({ record }) => {
  const caps = record.capabilities || [];
  if (caps.length === 0) {
    return <Text type="secondary">—</Text>;
  }
  const unknown = new Set(record.unknownCapabilities || []);
  return (
    <Space size={4} wrap>
      {caps.map((c) => (
        <Tag key={c} color={unknown.has(c) ? 'warning' : 'default'} style={{ fontSize: 11 }}>
          {c}
        </Tag>
      ))}
    </Space>
  );
};

// extractErrorMessage 从 umi request 抛出的错误中提取可读文本；
// 模块级纯函数拿不到 intl，兜底文案由调用方传入
export function extractErrorMessage(error: unknown, fallback: string): string {
  const candidate = error as
    | { data?: { error?: string; message?: string }; message?: string }
    | undefined;
  return candidate?.data?.error || candidate?.data?.message || candidate?.message || fallback;
}
