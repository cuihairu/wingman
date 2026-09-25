/**
 * 错误与降级展示（公共件，设计 §7 第 3 条第 3 件）。
 *
 * 远控链路的失败分三类，处置完全不同，UI 必须区分（第二方 dashboard
 * 复刻时最容易把这三类揉成一句「连接失败」）：
 *
 *  | 类别 | 典型来源 | 处置 |
 *  |---|---|---|
 *  | 权限拒绝 | 403 desktop:control 缺失 / RBAC | 引导申请权限，**不重试** |
 *  | 能力未配置 | 501 录制未配置、guacd 不可达 | 展示后端 hint（配置指引） |
 *  | 瞬时断链 | 隧道异常断开 | 提示重新打开（票据一次性，**无法原地重连**） |
 *
 * 票据一次性是硬约束：common-js 侧无法在原连接上重握手，所以「重试」
 * 只能是重新申请票据 + 重建会话，由消费方决定是否自动做。
 */

/** 错误分类（决定 UI 语气与是否可重试） */
export type RemoteErrorKind = 'permission' | 'unconfigured' | 'disconnected' | 'unknown';

export interface ClassifiedRemoteError {
  kind: RemoteErrorKind;
  /** 面向用户的标题 */
  title: string;
  /** 面向用户的说明（保留后端原文，不二次臆造） */
  detail: string;
  /** 是否值得重试：权限与配置问题重试无意义 */
  retryable: boolean;
}

/**
 * classifyRemoteError 从错误串判定类别。
 *
 * 判定用服务端返回的原文（Go handler 的 error 字段是稳定契约，见
 * handlers/guacamole.go / recordings.go），不靠猜：命中权限码、
 * 501 hint 或 permission required 字样即判定，不匹配则落 unknown 走通用文案。
 *
 * detail 一律是原文或其兜底：命中的三类分支条件本身就要求 text 非空
 * （含关键词），故不存在「命中分支但原文为空」的情况，无需二次兜底。
 */
export function classifyRemoteError(raw: string): ClassifiedRemoteError {
  const text = (raw || '').trim();
  const lower = text.toLowerCase();

  if (lower.includes('permission') || lower.includes('forbidden') || lower.includes('required')) {
    return {
      kind: 'permission',
      title: '权限不足',
      detail: text,
      retryable: false,
    };
  }
  if (
    lower.includes('not configured') ||
    lower.includes('not implemented') ||
    lower.includes('501') ||
    lower.includes('unavailable')
  ) {
    return {
      kind: 'unconfigured',
      title: '能力未配置',
      detail: text,
      retryable: false,
    };
  }
  if (lower.includes('断开') || lower.includes('disconnect') || lower.includes('closed')) {
    return {
      kind: 'disconnected',
      title: '会话已断开',
      detail: text,
      retryable: true,
    };
  }
  return {
    kind: 'unknown',
    title: '桌面连接失败',
    detail: text || '桌面连接失败',
    retryable: true,
  };
}

export interface RemoteErrorNoticeProps {
  /** 后端 error / 异常 message 原文 */
  error: string;
  /** 重试回调（不传则不渲染重试按钮） */
  onRetry?: () => void;
}

/**
 * RemoteErrorNotice 错误提示条。权限/配置类不渲染重试（重试无意义，
 * 反而诱导用户反复点）；瞬时/未知类在给了 onRetry 时渲染。
 */
export default function RemoteErrorNotice({ error, onRetry }: RemoteErrorNoticeProps) {
  if (!error) {
    return null;
  }
  const info = classifyRemoteError(error);
  return (
    <div
      role="alert"
      data-testid="remote-error-notice"
      style={{
        padding: '8px 12px',
        borderRadius: 4,
        background: info.kind === 'permission' ? '#fff2f0' : '#fffbe6',
        border: `1px solid ${info.kind === 'permission' ? '#ffccc7' : '#ffe58f'}`,
      }}
    >
      <strong>{info.title}</strong>
      <div style={{ fontSize: 12, marginTop: 2 }}>{info.detail}</div>
      {onRetry && info.retryable && <ButtonLike onClick={onRetry}>重试</ButtonLike>}
    </div>
  );
}

// 局部按钮：避免为一个重试按钮把整个 antd Button 的样式语义带进公共件
function ButtonLike({ children, onClick }: { children: React.ReactNode; onClick: () => void }) {
  return (
    <button
      type="button"
      onClick={onClick}
      style={{
        marginTop: 6,
        padding: '2px 10px',
        fontSize: 12,
        cursor: 'pointer',
        borderRadius: 4,
        border: '1px solid #d9d9d9',
        background: '#fff',
      }}
    >
      {children}
    </button>
  );
}
