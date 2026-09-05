/**
 * wingman.ts 补全：normalize* 各分支（camel/snake 双写、JSON 字符串字段、
 * 非法枚举回退）、format 工具、状态→颜色映射、脚本执行日志归一化。
 */
import { request } from '@umijs/max';
import * as wingman from '@/services/wingman';

const mockedRequest = request as unknown as jest.Mock;

function mockResponse(data: unknown) {
  mockedRequest.mockResolvedValueOnce({ success: true, data });
}

beforeEach(() => {
  mockedRequest.mockReset();
});

describe('normalizeWorkflow / normalizeWorkflowInstance', () => {
  it('steps 为 JSON 字符串 + snake_case 字段 + 非法 status 回退 pending', async () => {
    mockResponse([
      {
        ID: 'w1',
        Name: '日常任务',
        description: 'd',
        StepsJSON: JSON.stringify([
          {
            id: 's1',
            name: '第一步',
            script: 'main.lua',
            workers: 'a, b ,',
            depends_on: 's0',
            timeout_seconds: 45,
            parameters: { k: 1 },
          },
        ]),
        contextJson: '{"hp":99}',
        Status: 'NO_SUCH_STATUS',
        createdAt: '2024-01-02T03:04:05.000Z',
      },
    ]);

    const resp = await wingman.getWorkflows();
    expect(resp.success).toBe(true);
    const wf = resp.data![0];
    expect(wf).toMatchObject({
      id: 'w1',
      name: '日常任务',
      status: wingman.WorkflowStatus.Pending,
      sharedContext: { hp: 99 },
    });
    expect(wf.createdTime).toBe(Date.parse('2024-01-02T03:04:05.000Z'));
    expect(wf.steps[0]).toMatchObject({
      id: 's1',
      workers: ['a', 'b'],
      dependsOn: ['s0'],
      timeoutSeconds: 45,
      parameters: { k: 1 },
    });
  });

  it('steps/context 为非法 JSON 字符串时回退空值，workers 为数组时 trim 过滤', async () => {
    mockResponse([
      {
        id: 'w2',
        steps: '{bad json',
        sharedContext: '{also bad',
        status: 'running',
        stepsFallback: undefined,
        Steps: [{ id: 's', workers: [' x ', '', 3] }],
      },
    ]);
    const resp = await wingman.getWorkflows();
    const wf = resp.data![0];
    expect(wf.steps).toEqual([]);
    expect(wf.sharedContext).toEqual({});
  });

  it('getWorkflow 归一化实例：stepStatus 数组形式 + workerStatus 记录形式', async () => {
    mockResponse({
      id: 'wf1',
      status: 'running',
      steps: [{ id: 's1', name: 'S1', script: 'a.lua' }],
      stepStatus: [
        { stepId: 's1', status: 'COMPLETED' },
        { StepID: 's2', Status: 'failed' },
        { step_id: 's3', status: 'bogus' },
        { status: 'pending' },
      ],
      workerStatus: {
        w1: { status: 'running', message: 'hi', startTime: 1, endTime: 2 },
        '': { status: 'pending' },
      },
      current_step_id: 's1',
    });

    const resp = await wingman.getWorkflow('wf1');
    const inst = resp.data!;
    expect(inst.stepStatus).toEqual({
      s1: wingman.StepStatus.Completed,
      s2: wingman.StepStatus.Failed,
      s3: wingman.StepStatus.Pending,
    });
    expect(Object.keys(inst.workerStatus)).toEqual(['w1']);
    expect(inst.workerStatus.w1).toMatchObject({
      workerId: 'w1',
      status: wingman.StepStatus.Running,
      message: 'hi',
    });
    expect(inst.currentStepId).toBe('s1');
  });

  it('workerStatus 数组形式 + 非法成员被跳过', async () => {
    mockResponse({
      id: 'wf2',
      stepStatus: { s1: { Status: 'SKIPPED' } },
      workerStatus: [{ workerId: 'w9', status: 'skipped' }, null],
    });
    const inst = (await wingman.getWorkflow('wf2')).data!;
    expect(inst.stepStatus.s1).toBe(wingman.StepStatus.Skipped);
    expect(inst.workerStatus.w9.status).toBe(wingman.StepStatus.Skipped);
  });

  it('unwrapResponseData：响应无 success 字段时按裸数据解包', async () => {
    mockedRequest.mockResolvedValueOnce([{ id: 'raw' }]);
    const resp = await wingman.getWorkflows();
    expect(resp.data![0].id).toBe('raw');
  });
});

describe('脚本 API 与 normalizeScript / normalizeScriptLog', () => {
  it('createScript 归一化脚本：running 状态、executionId、Date/字符串时间', async () => {
    mockResponse({
      id: 's1',
      name: 'x.lua',
      status: 'running',
      executionId: 42,
      size: 128,
      modifiedTime: new Date('2024-01-01T00:00:00Z'),
    });
    const resp = await wingman.createScript('x.lua');
    expect(resp.data).toMatchObject({
      id: 's1',
      path: 'x.lua',
      isRunning: true,
      executionId: '42',
      size: 128,
      modifiedTime: Date.parse('2024-01-01T00:00:00Z'),
    });
  });

  it('normalizeScript 缺省：path 兜底 name/id，非法时间回退 0，is_running 蛇形字段', async () => {
    mockResponse([{ path: '/scripts/a.lua', is_running: true, modifiedTime: 'not-a-date' }]);
    const resp = await wingman.getScripts();
    const script = resp.data![0];
    expect(script.id).toBe('/scripts/a.lua');
    expect(script.isRunning).toBe(true);
    expect(script.modifiedTime).toBe(0);
    expect(script.executionId).toBeUndefined();
  });

  it('getScriptLogs 归一化日志：时间/级别/消息多字段回退', async () => {
    mockResponse([
      { timestamp: '2024-01-01T00:00:00Z', level: 'WARN', output: '警告内容' },
      { level: 'BOGUS', message: '普通消息', createdAt: 1700000000000 },
      { CreatedAt: '2024-06-01T00:00:00.000Z', Level: 'ERROR', Output: 'E' },
      {},
    ]);
    const resp = await wingman.getScriptLogs('exec-1', 10, 50);
    expect(mockedRequest).toHaveBeenLastCalledWith('/api/scripts/logs', {
      method: 'POST',
      data: { executionId: 'exec-1', offset: 10, limit: 50 },
    });
    const logs = resp.data!;
    expect(logs[0]).toMatchObject({ level: 'warn', message: '警告内容' });
    expect(logs[0].timestamp).toBe(Date.parse('2024-01-01T00:00:00Z'));
    expect(logs[1]).toMatchObject({ level: 'info', message: '普通消息', timestamp: 1700000000000 });
    expect(logs[2]).toMatchObject({ level: 'error', message: 'E' });
    expect(logs[3].level).toBe('info');
  });

  it('runScript 归一化 executionId（scriptId 兜底）', async () => {
    mockResponse({ scriptId: 'exec-9' });
    const resp = await wingman.runScript('a.lua', ['--v']);
    expect(resp.data).toEqual({ executionId: 'exec-9' });
  });

  it('getScriptContent / saveScriptContent / deleteScript / stopScript 直通', async () => {
    mockResponse('print(1)');
    expect((await wingman.getScriptContent('a.lua')).data).toBe('print(1)');
    mockResponse(undefined);
    await wingman.saveScriptContent('a.lua', 'print(2)');
    expect(mockedRequest).toHaveBeenLastCalledWith('/api/scripts/save', {
      method: 'POST',
      data: { path: 'a.lua', content: 'print(2)' },
    });
    mockResponse(undefined);
    await wingman.deleteScript('a.lua');
    mockResponse(undefined);
    await wingman.stopScript('exec-1');
    expect(mockedRequest).toHaveBeenLastCalledWith('/api/scripts/stop', {
      method: 'POST',
      data: { executionId: 'exec-1' },
    });
  });
});

describe('格式化工具', () => {
  it('formatBytes 各单位', () => {
    expect(wingman.formatBytes(0)).toBe('0 B');
    expect(wingman.formatBytes(1)).toBe('1.00 B');
    expect(wingman.formatBytes(1024)).toBe('1.00 KB');
    expect(wingman.formatBytes(1024 * 1024)).toBe('1.00 MB');
    expect(wingman.formatBytes(1024 ** 3)).toBe('1.00 GB');
    expect(wingman.formatBytes(1024 ** 4)).toBe('1.00 TB');
  });

  it('formatDuration 各区间', () => {
    expect(wingman.formatDuration(500)).toBe('500ms');
    expect(wingman.formatDuration(1500)).toBe('1.5s');
    expect(wingman.formatDuration(90000)).toBe('1.5m');
    expect(wingman.formatDuration(7200000)).toBe('2.0h');
  });
});

describe('状态颜色映射', () => {
  it('AgentStatus 全枚举 + 默认', () => {
    expect(wingman.getAgentStatusColor(wingman.AgentStatus.Online)).toBe('green');
    expect(wingman.getAgentStatusColor(wingman.AgentStatus.Idle)).toBe('blue');
    expect(wingman.getAgentStatusColor(wingman.AgentStatus.Busy)).toBe('orange');
    expect(wingman.getAgentStatusColor(wingman.AgentStatus.Error)).toBe('red');
    expect(wingman.getAgentStatusColor(wingman.AgentStatus.Offline)).toBe('default');
    expect(wingman.getAgentStatusColor('whatever' as wingman.AgentStatus)).toBe('default');
  });

  it('WorkflowStatus 全枚举 + 默认', () => {
    expect(wingman.getWorkflowStatusColor(wingman.WorkflowStatus.Completed)).toBe('success');
    expect(wingman.getWorkflowStatusColor(wingman.WorkflowStatus.Running)).toBe('processing');
    expect(wingman.getWorkflowStatusColor(wingman.WorkflowStatus.Failed)).toBe('error');
    expect(wingman.getWorkflowStatusColor(wingman.WorkflowStatus.Cancelled)).toBe('default');
    expect(wingman.getWorkflowStatusColor(wingman.WorkflowStatus.Pending)).toBe('default');
    expect(wingman.getWorkflowStatusColor('nope' as wingman.WorkflowStatus)).toBe('default');
  });

  it('StepStatus 全枚举 + 默认', () => {
    expect(wingman.getStepStatusColor(wingman.StepStatus.Completed)).toBe('success');
    expect(wingman.getStepStatusColor(wingman.StepStatus.Running)).toBe('processing');
    expect(wingman.getStepStatusColor(wingman.StepStatus.Failed)).toBe('error');
    expect(wingman.getStepStatusColor(wingman.StepStatus.Skipped)).toBe('default');
    expect(wingman.getStepStatusColor(wingman.StepStatus.Pending)).toBe('default');
    expect(wingman.getStepStatusColor('nope' as wingman.StepStatus)).toBe('default');
  });
});
