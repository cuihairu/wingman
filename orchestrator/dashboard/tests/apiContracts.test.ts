/**
 * 前后端 API 路径契约测试。
 *
 * server（orchestrator/server/main.go）路由分两组：
 *   /api/*       —— wingman 兼容组（agents/workflows/scripts 写操作/messages/feedback/audit/admin）
 *   /api/v1/*    —— 版本化组（auth/profile/status/health/settings/scripts admin 组）
 *
 * 历史事故：src/utils/api.ts 曾把所有 /api/* 全局改写为 /api/v1/*，导致 agents、
 * workflows、messages、audit、admin 等页面请求全部 404。本测试逐函数锁定
 * "service 函数 → 实际请求 URL" 的映射，防止前缀漂移回归。
 */

import { request } from '@umijs/max';
import { fetchJSON } from '@/services/core/http';
import * as wingman from '@/services/wingman';
import * as auth from '@/services/api/auth';
import * as me from '@/services/api/me';
import * as admin from '@/services/api/admin';
import * as audit from '@/services/api/audit';
import * as messages from '@/services/api/messages';
import * as support from '@/services/api/support';

const mockedRequest = request as unknown as jest.Mock;

const fetchMock = jest.fn(async () => ({
  ok: true,
  status: 200,
  json: async () => ({}),
}));

beforeEach(() => {
  mockedRequest.mockClear();
  fetchMock.mockClear();
  (global as any).fetch = fetchMock;
});

afterEach(() => {
  delete (global as any).fetch;
});

function lastRequestUrl(): string {
  const calls = mockedRequest.mock.calls;
  expect(calls.length).toBeGreaterThan(0);
  return String(calls[calls.length - 1][0]);
}

function lastRequestMethod(): string {
  const calls = mockedRequest.mock.calls;
  return String(calls[calls.length - 1][1]?.method ?? 'GET');
}

function lastFetchUrl(): string {
  expect(fetchMock.mock.calls.length).toBeGreaterThan(0);
  return String(fetchMock.mock.calls[fetchMock.mock.calls.length - 1][0]);
}

describe('wingman.ts 走 /api 兼容组（禁止 /api/v1 前缀）', () => {
  it('getAgents → GET /api/agents', async () => {
    await wingman.getAgents();
    expect(lastRequestUrl()).toBe('/api/agents');
    expect(lastRequestMethod()).toBe('GET');
  });

  it('shutdownAgent → POST /api/agents/:id/shutdown', async () => {
    await wingman.shutdownAgent('agent-1');
    expect(lastRequestUrl()).toBe('/api/agents/agent-1/shutdown');
    expect(lastRequestMethod()).toBe('POST');
  });

  it('setAgentTags → PUT /api/agents/:id/tags', async () => {
    await wingman.setAgentTags('agent-1', ['prod']);
    expect(lastRequestUrl()).toBe('/api/agents/agent-1/tags');
    expect(lastRequestMethod()).toBe('PUT');
  });

  it('getWorkflows → GET /api/workflows', async () => {
    await wingman.getWorkflows();
    expect(lastRequestUrl()).toBe('/api/workflows');
  });

  it('getWorkflow → GET /api/workflows/:id', async () => {
    await wingman.getWorkflow('wf-9');
    expect(lastRequestUrl()).toBe('/api/workflows/wf-9');
  });

  it('getWorkflowTemplates → GET /api/workflow-templates', async () => {
    await wingman.getWorkflowTemplates();
    expect(lastRequestUrl()).toBe('/api/workflow-templates');
  });

  it('submitWorkflow → POST /api/workflows', async () => {
    await wingman.submitWorkflow({
      name: 'demo',
      steps: [],
    } as any);
    expect(lastRequestUrl()).toBe('/api/workflows');
    expect(lastRequestMethod()).toBe('POST');
  });

  it('cancelWorkflow → POST /api/workflows/:id/cancel', async () => {
    await wingman.cancelWorkflow('wf-9');
    expect(lastRequestUrl()).toBe('/api/workflows/wf-9/cancel');
    expect(lastRequestMethod()).toBe('POST');
  });

  it('getScripts → GET /api/scripts', async () => {
    await wingman.getScripts();
    expect(lastRequestUrl()).toBe('/api/scripts');
  });

  it('getScriptContent → POST /api/scripts/content', async () => {
    await wingman.getScriptContent('a.lua');
    expect(lastRequestUrl()).toBe('/api/scripts/content');
    expect(lastRequestMethod()).toBe('POST');
  });

  it('saveScriptContent → POST /api/scripts/save', async () => {
    await wingman.saveScriptContent('a.lua', 'print(1)');
    expect(lastRequestUrl()).toBe('/api/scripts/save');
  });

  it('createScript → POST /api/scripts', async () => {
    await wingman.createScript('new-script');
    expect(lastRequestUrl()).toBe('/api/scripts');
    expect(lastRequestMethod()).toBe('POST');
  });

  it('deleteScript → POST /api/scripts/delete（回归：该路径曾在 v1 组缺失）', async () => {
    await wingman.deleteScript('a.lua');
    expect(lastRequestUrl()).toBe('/api/scripts/delete');
    expect(lastRequestMethod()).toBe('POST');
  });

  it('runScript → POST /api/scripts/run', async () => {
    await wingman.runScript('a.lua');
    expect(lastRequestUrl()).toBe('/api/scripts/run');
  });

  it('stopScript → POST /api/scripts/stop', async () => {
    await wingman.stopScript('exec-1');
    expect(lastRequestUrl()).toBe('/api/scripts/stop');
  });

  it('getScriptLogs → POST /api/scripts/logs', async () => {
    await wingman.getScriptLogs('exec-1');
    expect(lastRequestUrl()).toBe('/api/scripts/logs');
  });

  it('全部 wingman.ts 请求均不得以 /api/v1 开头', async () => {
    await wingman.getAgents();
    await wingman.getWorkflows();
    await wingman.getScripts();
    await wingman.getWorkflowTemplates();
    for (const call of mockedRequest.mock.calls) {
      expect(String(call[0]).startsWith('/api/v1')).toBe(false);
    }
  });
});

describe('services/api 走显式前缀（v1 白名单 + /api 组）', () => {
  it('auth.createSession → POST /api/v1/auth/login', async () => {
    await auth.createSession({ username: 'admin', password: 'x' });
    expect(lastRequestUrl()).toBe('/api/v1/auth/login');
    expect(lastRequestMethod()).toBe('POST');
  });

  it('auth.fetchCurrentUserGames → /api/v1/profile/games', async () => {
    await auth.fetchCurrentUserGames();
    expect(lastRequestUrl()).toBe('/api/v1/profile/games');
  });

  it('me.getMyProfile → /api/v1/profile', async () => {
    await me.getMyProfile();
    expect(lastRequestUrl()).toBe('/api/v1/profile');
  });

  it('me.getMyGames → /api/v1/profile/games', async () => {
    await me.getMyGames();
    expect(lastRequestUrl()).toBe('/api/v1/profile/games');
  });

  it('me.getMyPermissions → /api/v1/profile/permissions', async () => {
    await me.getMyPermissions();
    expect(lastRequestUrl()).toBe('/api/v1/profile/permissions');
  });

  it('admin.listUsers → /api/admin/users', async () => {
    await admin.listUsers({ page: 1, size: 20 });
    expect(lastFetchUrl()).toBe('/api/admin/users?page=1&size=20');
  });

  it('admin.createUser → POST /api/admin/users', async () => {
    await admin.createUser({ username: 'u', password: 'p', role: 'viewer' } as any);
    expect(lastFetchUrl()).toBe('/api/admin/users');
  });

  it('admin.resetUserPassword → POST /api/admin/users/:id/reset-password', async () => {
    await admin.resetUserPassword(7, { password: 'new' } as any);
    expect(lastFetchUrl()).toBe('/api/admin/users/7/reset-password');
  });

  it('admin.listRoles → /api/admin/roles', async () => {
    await admin.listRoles();
    expect(lastFetchUrl()).toBe('/api/admin/roles');
  });

  it('admin.listPermissionCatalog → /api/admin/permissions', async () => {
    await admin.listPermissionCatalog();
    expect(lastFetchUrl()).toBe('/api/admin/permissions');
  });

  it('audit.listAudit → /api/audit', async () => {
    await audit.listAudit({ page: 2, size: 20, kinds: 'login' });
    expect(lastFetchUrl()).toBe('/api/audit?kinds=login&size=20&page=2');
  });

  it('audit.listAudit 无参数调用安全（默认空查询）', async () => {
    await audit.listAudit();
    expect(lastFetchUrl()).toBe('/api/audit');
  });

  it('messages.listMessages → /api/messages', async () => {
    await messages.listMessages();
    expect(lastFetchUrl()).toBe('/api/messages');
  });

  it('messages.unreadCount → /api/messages/unread-count', async () => {
    await messages.unreadCount();
    expect(lastFetchUrl()).toBe('/api/messages/unread-count');
  });

  it('messages.markAllMessagesRead → POST /api/messages/read-all', async () => {
    await messages.markAllMessagesRead();
    expect(lastFetchUrl()).toBe('/api/messages/read-all');
  });

  it('support.createFeedback → POST /api/feedback', async () => {
    await support.createFeedback({ type: 'bug', content: 'x' } as any);
    expect(lastFetchUrl()).toBe('/api/feedback');
  });

  it('fetchJSON 路径除 v1 白名单外不得出现 /api/v1 前缀', async () => {
    await admin.listUsers();
    await admin.listRoles();
    await audit.listAudit();
    await messages.listMessages();
    await support.createFeedback({ type: 'bug', content: 'x' } as any);
    for (const call of fetchMock.mock.calls) {
      const url = String(call[0]);
      expect(url.startsWith('/api/v1')).toBe(false);
    }
  });
});
