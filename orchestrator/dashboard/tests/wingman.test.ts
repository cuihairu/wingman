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

describe('Agent 触发器 API（getAgentTriggers / toggleAgentTrigger）', () => {
  it('getAgentTriggers 归一化 runtime trigger.list 条目', async () => {
    mockResponse([
      {
        id: 1,
        name: 'hp-watch',
        enabled: true,
        type: 'ColorFound',
        condition: {
          type: 'ColorFound',
          value: '#ff0000',
          region: { x: '10', y: 20, width: 100, height: 50 },
          tolerance: '15',
          interval: 1000,
        },
        actions: [{ type: 'RunScript', value: 'heal.lua', x: 1, y: 2, delay: 0 }],
        oneShot: false,
        cooldown: 3000,
        lastTriggered: false,
      },
      null,
      'garbage',
    ]);

    const resp = await wingman.getAgentTriggers('agent-1');
    expect(mockedRequest).toHaveBeenCalledWith(
      '/api/agents/agent-1/triggers',
      expect.objectContaining({ method: 'GET' }),
    );
    expect(resp.success).toBe(true);
    expect(resp.data).toHaveLength(3);

    const trigger = resp.data![0];
    expect(trigger).toMatchObject({
      id: '1',
      name: 'hp-watch',
      enabled: true,
      type: 'ColorFound',
      oneShot: false,
      cooldown: 3000,
      lastTriggered: false,
    });
    // 字符串数字回退为数值
    expect(trigger.condition.region).toEqual({ x: 10, y: 20, width: 100, height: 50 });
    expect(trigger.condition.tolerance).toBe(15);
    expect(trigger.actions[0]).toMatchObject({ type: 'RunScript', value: 'heal.lua' });

    // 非法条目安全回退
    const fallback = resp.data![1];
    expect(fallback.id).toBe('');
    expect(fallback.name).toBe('未命名触发器');
    expect(fallback.enabled).toBe(false);
    expect(fallback.actions).toEqual([]);
  });

  it('toggleAgentTrigger 携带触发器 id POST', async () => {
    mockResponse({ id: '1', enabled: false });

    const resp = await wingman.toggleAgentTrigger('agent-1', '1');
    expect(mockedRequest).toHaveBeenCalledWith(
      '/api/agents/agent-1/triggers/toggle',
      expect.objectContaining({ method: 'POST', data: { id: '1' } }),
    );
    expect(resp.data).toEqual({ id: '1', enabled: false });
  });
});

describe('Agent 触发器 CRUD API（create / update / remove）', () => {
  const config: wingman.AgentTriggerConfigInput = {
    name: 'boss-alert',
    enabled: true,
    oneShot: false,
    cooldown: 5000,
    condition: {
      type: 'ImageFound',
      value: 'boss.png',
      tolerance: 10,
      interval: 1000,
      region: { x: 0, y: 0, width: 100, height: 100 },
    },
    actions: [{ type: 'Click', value: '', x: 100, y: 200, delay: 0 }],
  };

  it('createAgentTrigger POST 配置原样上送，返回 runtime 分配 id', async () => {
    mockResponse({ id: '42', name: 'boss-alert' });

    const resp = await wingman.createAgentTrigger('agent-1', config);
    expect(mockedRequest).toHaveBeenCalledWith('/api/agents/agent-1/triggers', {
      method: 'POST',
      data: config,
    });
    expect(resp.data).toEqual({ id: '42', name: 'boss-alert' });
  });

  it('updateAgentTrigger PUT 到 /:triggerId 并携带配置', async () => {
    mockResponse({ id: '42' });

    const resp = await wingman.updateAgentTrigger('agent-1', '42', { cooldown: 9000 });
    expect(mockedRequest).toHaveBeenCalledWith('/api/agents/agent-1/triggers/42', {
      method: 'PUT',
      data: { cooldown: 9000 },
    });
    expect(resp.data).toEqual({ id: '42' });
  });

  it('removeAgentTrigger DELETE 到 /:triggerId', async () => {
    mockResponse({ id: '42' });

    await wingman.removeAgentTrigger('agent-1', '42');
    expect(mockedRequest).toHaveBeenCalledWith(
      '/api/agents/agent-1/triggers/42',
      expect.objectContaining({ method: 'DELETE' }),
    );
  });

  it('toStrictNumber：非有限数值与非数字字符串回退 0，action 缺 type/value 回退空串', async () => {
    mockResponse([
      {
        id: 't-nan',
        name: 'nan-trigger',
        condition: {
          type: 'ColorFound',
          value: '#fff',
          region: { x: NaN, y: 1, width: 10, height: 10 },
          tolerance: NaN,
          interval: 'abc',
        },
        actions: [{}],
      },
    ]);

    const resp = await wingman.getAgentTriggers('agent-2');
    const trigger = resp.data![0];
    expect(trigger.condition.region).toEqual({ x: 0, y: 1, width: 10, height: 10 });
    expect(trigger.condition.tolerance).toBe(0);
    expect(trigger.condition.interval).toBe(0);
    expect(trigger.actions[0]).toEqual({ type: '', value: '', x: 0, y: 0, delay: 0 });
  });
});

describe('normalize 补全：snake/大写字段回退与缺省兜底', () => {
  it('steps 数组直传：workers 数组 trim 过滤、ID/Name/Script 大写字段回退、空对象兜底', async () => {
    mockResponse([
      {
        id: 'w3',
        steps: [
          {
            ID: 'S9',
            Name: 'N9',
            Script: 'c9.lua',
            workers: [' x ', '', 3, 'y'],
            dependsOn: [],
          },
          {},
        ],
      },
    ]);

    const resp = await wingman.getWorkflows();
    const steps = resp.data![0].steps;
    expect(steps[0]).toMatchObject({
      id: 'S9',
      name: 'N9',
      script: 'c9.lua',
      workers: ['x', '3', 'y'],
      dependsOn: [],
    });
    expect(steps[1]).toMatchObject({ id: '', name: '', script: '' });
  });

  it('stepStatus 为记录且值为裸字符串时按原始值归一化', async () => {
    mockResponse({
      id: 'wf3',
      stepStatus: { s5: 'running', s6: 'bogus-status' },
    });
    const inst = (await wingman.getWorkflow('wf3')).data!;
    expect(inst.stepStatus.s5).toBe(wingman.StepStatus.Running);
    expect(inst.stepStatus.s6).toBe(wingman.StepStatus.Pending);
  });

  it('getWorkflowTemplates：响应缺 data 字段时回退空数组', async () => {
    mockedRequest.mockResolvedValueOnce({ success: true });
    const templates = await wingman.getWorkflowTemplates();
    expect(templates).toEqual([]);
  });

  it('normalizeScript：仅有 name 时 id/path 均回退 name', async () => {
    mockResponse([{ name: 'only-name.lua' }]);
    const resp = await wingman.getScripts();
    const script = resp.data![0];
    expect(script.id).toBe('only-name.lua');
    expect(script.path).toBe('only-name.lua');
  });
});

describe('批量操作 API（batch run / stop / trigger）', () => {
  it('batchRunScript POST 选择器 + path，响应归一化 BatchSummary', async () => {
    mockResponse({
      total: 2,
      succeeded: 1,
      failed: 1,
      results: [
        { agentId: 'a1', success: true },
        { agentId: 'a2', success: false, error: 'agent offline' },
      ],
    });

    const resp = await wingman.batchRunScript({ agentIds: ['a1', 'a2'] }, 'demo.lua');
    expect(mockedRequest).toHaveBeenCalledWith('/api/agents/batch/run-script', {
      method: 'POST',
      data: { agentIds: ['a1', 'a2'], path: 'demo.lua' },
    });
    expect(resp.data).toMatchObject({ total: 2, succeeded: 1, failed: 1 });
    expect(resp.data!.results[0]).toEqual({ agentId: 'a1', success: true, error: undefined });
    expect(resp.data!.results[1]).toEqual({
      agentId: 'a2',
      success: false,
      error: 'agent offline',
    });
  });

  it('batchStopScript POST 选择器 + executionId，空 results 归一化为数组', async () => {
    mockResponse({ total: 0, succeeded: 0, failed: 0 });

    const resp = await wingman.batchStopScript({ tags: ['prod', 'edge'] }, 'demo');
    expect(mockedRequest).toHaveBeenCalledWith('/api/agents/batch/stop-script', {
      method: 'POST',
      data: { tags: ['prod', 'edge'], executionId: 'demo' },
    });
    expect(resp.data!.results).toEqual([]);
  });

  it('batchCreateTrigger POST 选择器 + 平铺触发器配置', async () => {
    const config: wingman.AgentTriggerConfigInput = {
      name: 'batch-trigger',
      enabled: true,
      condition: { type: 'TimeElapsed', value: '5000' },
      actions: [{ type: 'Log', value: 'hi' }],
    };
    mockResponse({ total: 3, succeeded: 3, failed: 0, results: [] });

    await wingman.batchCreateTrigger({ tags: ['prod'] }, config);
    expect(mockedRequest).toHaveBeenCalledWith('/api/agents/batch/trigger', {
      method: 'POST',
      data: { tags: ['prod'], ...config },
    });
  });

  it('normalizeBatchSummary 宽容回退：非法 total 与缺失/畸形 results', async () => {
    mockResponse({ total: 'bad', results: [null, { agentId: 'a1' }, 42] });

    const resp = await wingman.batchRunScript({ agentIds: ['a1'] }, 'x.lua');
    expect(resp.data).toEqual({
      total: 0,
      succeeded: 0,
      failed: 0,
      results: [
        { agentId: '', success: false, error: undefined },
        { agentId: 'a1', success: false, error: undefined },
        { agentId: '', success: false, error: undefined },
      ],
    });
  });
});
