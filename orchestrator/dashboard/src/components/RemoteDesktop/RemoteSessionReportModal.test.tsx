/**
 * 会话审计报表面板 + 服务层测试（设计 §11 P1「审计报表呈现」）。
 *
 * 面板：打开即查（默认最近一周）、四块视图同源渲染、维度/粒度/模式切换回到
 * 第 1 页、翻页、空态、错误态。
 * 服务层：参数拼装（空值不传）、响应缺字段兜底、失败抛错。
 */
import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import React from 'react';
import RemoteSessionReportModal from './RemoteSessionReportModal';
import {
  listRemoteSessions,
  type RemoteSessionEntry,
  type RemoteSessionReport,
} from '@/services/remote';

jest.mock('@/services/remote', () => ({
  listRemoteSessions: jest.fn(),
}));

const mockedList = listRemoteSessions as jest.MockedFunction<typeof listRemoteSessions>;

/**
 * waitFor 放宽到 5s：CPU 饱和（CI 并行跑全仓）时默认 1s 窗口偶尔不够，
 * 「请求 → setState → 渲染」会假红。只放宽等待窗口，不放宽断言。
 */
const WAIT_TIMEOUT = 5000;

/** 构造一份完整报表响应 */
function makeReport(overrides: Partial<RemoteSessionReport> = {}): RemoteSessionReport {
  return {
    data: [
      {
        id: 1,
        sessionId: 'sess-1',
        agentId: 'agent-a',
        operator: 'alice',
        protocol: 'rdp',
        host: '10.0.0.5',
        port: 3389,
        readOnly: false,
        record: true,
        recordingName: 'agent-a-sess-1.mjs',
        status: 'closed',
        startedAt: '2026-09-25T10:00:00Z',
        endedAt: '2026-09-25T10:05:00Z',
        durationMs: 300000,
      },
      {
        id: 2,
        sessionId: 'sess-2',
        agentId: 'agent-b',
        operator: 'bob',
        protocol: 'vnc',
        host: '10.0.0.6',
        port: 5901,
        readOnly: true,
        record: false,
        status: 'failed',
        failReason: 'guacd unreachable',
        startedAt: '2026-09-25T11:00:00Z',
        durationMs: 0,
      },
    ],
    total: 2,
    page: 1,
    size: 10,
    summary: {
      total: 12,
      closed: 10,
      failed: 2,
      recorded: 4,
      control: 5,
      viewOnly: 7,
      totalMsSum: 3_600_000,
    },
    groups: [
      { key: 'rdp', count: 8, msSum: 3_000_000, failed: 1 },
      { key: 'vnc', count: 4, msSum: 600_000, failed: 1 },
    ],
    buckets: [{ bucket: '2026-09-25T00:00:00Z', count: 12, failed: 2, msSum: 3_600_000 }],
    ...overrides,
  };
}

beforeEach(() => {
  jest.clearAllMocks();
  mockedList.mockResolvedValue(makeReport());
});

function renderModal(props: Partial<React.ComponentProps<typeof RemoteSessionReportModal>> = {}) {
  const onCancel = jest.fn();
  const utils = render(<RemoteSessionReportModal open onCancel={onCancel} {...props} />);
  return { ...utils, onCancel };
}

describe('RemoteSessionReportModal', () => {
  it('打开即查询：默认最近一周 + 默认维度/粒度', async () => {
    renderModal();
    await waitFor(() => expect(mockedList).toHaveBeenCalled());
    const arg = mockedList.mock.calls[0][0] ?? {};
    expect(arg).toMatchObject({ page: 1, size: 10, groupBy: 'protocol', bucket: 'day' });
    // 默认窗口 168h：一周前的时间戳（宽松断言避免时钟抖动）
    const since = Date.parse(String(arg.start));
    const deltaHours = (Date.now() - since) / 3600_000;
    expect(deltaHours).toBeGreaterThan(167);
    expect(deltaHours).toBeLessThan(169);
  });

  it('关闭时不查询（受控开关）', () => {
    render(<RemoteSessionReportModal open={false} onCancel={jest.fn()} />);
    expect(mockedList).not.toHaveBeenCalled();
  });

  it('渲染汇总六项 + 列表两行 + 维度分布 + 时间趋势', async () => {
    renderModal();
    await waitFor(() => expect(screen.getByText('alice')).toBeTruthy());

    // 汇总：会话总数/正常断开/建连失败/接管/监看/累计时长
    expect(screen.getByText('会话总数')).toBeTruthy();
    expect(screen.getByText('12')).toBeTruthy();
    // 「正常断开」既是汇总标题也是状态标签文案 → getAllByText
    expect(screen.getAllByText('正常断开').length).toBeGreaterThanOrEqual(2);
    expect(screen.getByText('10')).toBeTruthy();
    expect(screen.getAllByText('建连失败').length).toBeGreaterThanOrEqual(2);
    expect(screen.getByText('接管次数')).toBeTruthy();
    expect(screen.getByText('5')).toBeTruthy();
    expect(screen.getByText('监看次数')).toBeTruthy();
    expect(screen.getByText('7')).toBeTruthy();
    // 累计时长格式化：3_600_000ms = 1时0分
    expect(screen.getByText('累计时长')).toBeTruthy();
    expect(screen.getByText('1时0分')).toBeTruthy();

    // 列表：协议/模式/状态/录像
    // 「RDP/VNC」在表格与维度分布里各出现一次 → getAllByText
    expect(screen.getAllByText('RDP').length).toBeGreaterThanOrEqual(1);
    expect(screen.getAllByText('VNC').length).toBeGreaterThanOrEqual(1);
    expect(screen.getByText('接管')).toBeTruthy();
    expect(screen.getByText('监看')).toBeTruthy();
    // 时长：5分 / 0ms（0 显示为 0ms 以区分「瞬间」与「无时长」）
    expect(screen.getByText('5分0秒')).toBeTruthy();
    expect(screen.getByText('0ms')).toBeTruthy();

    // 维度分布 + 时间趋势
    expect(screen.getByText('维度分布（按协议）')).toBeTruthy();
    expect(screen.getByText('8 次 · 50分0秒 · 失败 1')).toBeTruthy();
    expect(screen.getByText('时间趋势（按天）')).toBeTruthy();
    expect(screen.getByText('12 次 · 1时0分 · 失败 2')).toBeTruthy();
  });

  it('切换维度/粒度/模式都回到第 1 页并重查', async () => {
    renderModal();
    await waitFor(() => expect(mockedList).toHaveBeenCalledTimes(1));

    // 维度：按操作者
    fireEvent.mouseDown(document.querySelectorAll('.ant-select-selector')[0]);
    fireEvent.click(await screen.findByTitle('按操作者'));
    await waitFor(() => expect(mockedList).toHaveBeenCalledTimes(2));
    expect(mockedList.mock.calls[1][0]).toMatchObject({ groupBy: 'operator', page: 1 });
    expect(screen.getByText('维度分布（按操作者）')).toBeTruthy();

    // 粒度：按月
    fireEvent.mouseDown(document.querySelectorAll('.ant-select-selector')[1]);
    fireEvent.click(await screen.findByTitle('按月'));
    await waitFor(() => expect(mockedList).toHaveBeenCalledTimes(3));
    expect(mockedList.mock.calls[2][0]).toMatchObject({ bucket: 'month', page: 1 });
    expect(screen.getByText('时间趋势（按月）')).toBeTruthy();

    // 模式：仅接管
    fireEvent.mouseDown(document.querySelectorAll('.ant-select-selector')[2]);
    fireEvent.click(await screen.findByTitle('仅接管'));
    await waitFor(() => expect(mockedList).toHaveBeenCalledTimes(4));
    expect(mockedList.mock.calls[3][0]).toMatchObject({ mode: 'control', page: 1 });
  });

  it('模式选回「监看+接管」即清除筛选', async () => {
    renderModal();
    await waitFor(() => expect(mockedList).toHaveBeenCalledTimes(1));
    fireEvent.mouseDown(document.querySelectorAll('.ant-select-selector')[2]);
    fireEvent.click(await screen.findByTitle('仅监看'));
    await waitFor(() => expect(mockedList).toHaveBeenCalledTimes(2));
    expect(mockedList.mock.calls[1][0]?.mode).toBe('view');

    fireEvent.mouseDown(document.querySelectorAll('.ant-select-selector')[2]);
    fireEvent.click(await screen.findByTitle('监看+接管'));
    await waitFor(() => expect(mockedList).toHaveBeenCalledTimes(3));
    expect(mockedList.mock.calls[2][0]?.mode).toBeUndefined();
  });

  it('翻页带目标页码重查', async () => {
    mockedList.mockResolvedValue(makeReport({ total: 30, page: 1, size: 10 }));
    renderModal();
    await waitFor(() => expect(mockedList).toHaveBeenCalledTimes(1));

    fireEvent.click(screen.getByTitle('2'));
    await waitFor(() => expect(mockedList).toHaveBeenCalledTimes(2));
    expect(mockedList.mock.calls[1][0]).toMatchObject({ page: 2 });
  });

  it('刷新按钮静默重查当前页（不闪 loading）', async () => {
    renderModal();
    await waitFor(() => expect(mockedList).toHaveBeenCalledTimes(1));
    await waitFor(() => expect(screen.queryByText(/会话总数/)).toBeTruthy());

    // 挂起响应：静默路径下表格不进入 loading 态
    let release: (v: RemoteSessionReport) => void = () => {};
    mockedList.mockImplementationOnce(
      () =>
        new Promise<RemoteSessionReport>((resolve) => {
          release = resolve;
        }),
    );
    fireEvent.click(screen.getByText(/^刷\s?新$/));
    await waitFor(() => expect(mockedList).toHaveBeenCalledTimes(2));
    // 旧数据仍在、没有 loading 遮罩 → 静默生效
    expect(screen.queryByText(/会话总数/)).toBeTruthy();
    expect(document.querySelector('.ant-table-loading')).toBeNull();

    release(makeReport());
    await waitFor(() => expect(document.querySelector('.ant-table-loading')).toBeNull());
  });

  it('空态：列表与两处聚合各给空文案', async () => {
    mockedList.mockResolvedValue({
      data: [],
      total: 0,
      page: 1,
      size: 10,
      summary: {
        total: 0,
        closed: 0,
        failed: 0,
        recorded: 0,
        control: 0,
        viewOnly: 0,
        totalMsSum: 0,
      },
      groups: [],
      buckets: [],
    });
    renderModal();
    await waitFor(() => expect(screen.getAllByText('该区间无会话').length).toBe(2));
    expect(screen.getByText('该区间无会话记录')).toBeTruthy();
  });

  it('查询失败展示错误条且不带旧数据', async () => {
    mockedList.mockRejectedValue(new Error('desktop:view permission required'));
    renderModal();
    await waitFor(() => expect(screen.getByText('会话审计报表不可用')).toBeTruthy());
    expect(screen.getByText('desktop:view permission required')).toBeTruthy();
    // 错误态下不渲染表格与汇总（避免把上一次的结果当本次结果展示）
    expect(screen.queryByText('会话总数')).toBeNull();
  });

  it('非 Error 异常走兜底文案', async () => {
    mockedList.mockRejectedValue('boom');
    renderModal();
    await waitFor(() => expect(screen.getByText('获取会话审计报表失败')).toBeTruthy());
  });

  it('关闭按钮透传 onCancel', async () => {
    const { onCancel } = renderModal();
    await waitFor(() => expect(mockedList).toHaveBeenCalled());
    // antd 对双字中文按钮文案自动插空格（关 闭）
    fireEvent.click(screen.getByText(/^关\s?闭$/));
    expect(onCancel).toHaveBeenCalled();
  });

  it('渲染只读声明：不含凭证与画面内容', async () => {
    renderModal();
    await waitFor(() => expect(screen.getByText(/不含任何凭证与画面内容/)).toBeTruthy());
  });

  it('时长格式化：秒级 / 无失败不追加后缀 / 未知协议键原样显示', async () => {
    mockedList.mockResolvedValue(
      makeReport({
        data: [
          {
            id: 1,
            sessionId: 'quick',
            agentId: 'agent-a',
            operator: 'alice',
            protocol: 'rdp',
            host: '10.0.0.5',
            port: 3389,
            readOnly: true,
            record: false,
            status: 'closed',
            startedAt: '2026-09-25T10:00:00Z',
            durationMs: 45000, // 45 秒 → 秒级分支
          },
        ],
        total: 1,
        // 零失败 → 不追加「失败 N」后缀；协议维度给出后端未知的键
        groups: [
          { key: 'rdp', count: 3, msSum: 45000, failed: 0 },
          { key: 'gnx', count: 1, msSum: 0, failed: 0 },
        ],
        buckets: [{ bucket: '2026-09-25T00:00:00Z', count: 4, failed: 0, msSum: 45000 }],
      }),
    );
    renderModal();
    await waitFor(() => expect(screen.getByText('45秒')).toBeTruthy());
    // 零失败不追加后缀
    expect(screen.getByText('3 次 · 45秒')).toBeTruthy();
    expect(screen.getByText('4 次 · 45秒')).toBeTruthy();
    // protocol 维度下未知协议键原样显示（不显示成 undefined）
    expect(screen.getByText('gnx')).toBeTruthy();
    // 无录像行显示「-」而非 Tag
    expect(screen.getAllByText('-').length).toBeGreaterThan(0);
    expect(screen.queryByText('有')).toBeNull();
  });

  it('维度键展示：protocol 维度映射友好名，其余维度原样显示', async () => {
    // groupBy=protocol 时 'alice-ops' 不是协议名 → 原样显示（不误映射）
    mockedList.mockResolvedValue(
      makeReport({ groups: [{ key: 'alice-ops', count: 2, msSum: 2000, failed: 1 }] }),
    );
    renderModal();
    await waitFor(() => expect(screen.getByText('维度分布（按协议）')).toBeTruthy());
    expect(screen.getByText('alice-ops')).toBeTruthy();
    expect(screen.getByText('2 次 · 2秒 · 失败 1')).toBeTruthy();

    // 切到「按操作者」：键原样显示
    fireEvent.mouseDown(document.querySelectorAll('.ant-select-selector')[0]);
    fireEvent.click(await screen.findByTitle('按操作者'));
    await waitFor(() => expect(screen.getByText('维度分布（按操作者）')).toBeTruthy());
    expect(screen.getByText('alice-ops')).toBeTruthy();

    // 切到「按机器」维度
    fireEvent.mouseDown(document.querySelectorAll('.ant-select-selector')[0]);
    fireEvent.click(await screen.findByTitle('按机器'));
    await waitFor(() => expect(screen.getByText('维度分布（按机器）')).toBeTruthy());
  });

  it('表格遇到后端未知协议时不显示 undefined（回退原值）', async () => {
    mockedList.mockResolvedValue(
      makeReport({
        data: [
          {
            id: 9,
            sessionId: 'odd',
            agentId: 'agent-z',
            operator: 'erin',
            // 后端协议白名单外的值（灰度/回滚期间可能出现）
            protocol: 'spice' as RemoteSessionEntry['protocol'],
            host: '10.0.0.9',
            port: 5900,
            readOnly: true,
            record: false,
            status: 'closed',
            startedAt: '2026-09-25T10:00:00Z',
            durationMs: 1000,
          },
        ],
        total: 1,
      }),
    );
    renderModal();
    await waitFor(() => expect(screen.getByText('erin')).toBeTruthy());
    expect(screen.getByText('spice')).toBeTruthy();
  });
});
