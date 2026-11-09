/**
 * @name Wingman API 服务
 * @description 连接到 Wingman 后端服务器
 *
 * 按域拆分的实现位于 ./wingman/（types/agents/batch/teams/workflows/scripts/format），
 * 本文件是兼容门面：具名导出与默认导出的聚合保持原有 import 路径不变。
 */

import { getAgents, shutdownAgent } from './wingman/agents';
import { batchRunScript, batchStopScript, batchCreateTrigger } from './wingman/batch';
import { getTeams, getTeam, pushTeamMessage } from './wingman/teams';
import { getWorkflows, getWorkflow, submitWorkflow, cancelWorkflow } from './wingman/workflows';
import {
  getScripts,
  getScriptContent,
  saveScriptContent,
  createScript,
  deleteScript,
  runScript,
  stopScript,
  getScriptLogs,
} from './wingman/scripts';
import {
  formatBytes,
  formatDuration,
  getAgentStatusColor,
  getWorkflowStatusColor,
  getStepStatusColor,
} from './wingman/format';

export * from './wingman/types';
export * from './wingman/agents';
export * from './wingman/batch';
export * from './wingman/teams';
export * from './wingman/workflows';
export * from './wingman/scripts';
export * from './wingman/format';

export default {
  // Agent
  getAgents,
  shutdownAgent,
  // Team
  getTeams,
  getTeam,
  pushTeamMessage,
  // Workflow
  getWorkflows,
  getWorkflow,
  submitWorkflow,
  cancelWorkflow,
  // Script
  getScripts,
  getScriptContent,
  saveScriptContent,
  createScript,
  deleteScript,
  runScript,
  stopScript,
  getScriptLogs,
  // Batch
  batchRunScript,
  batchStopScript,
  batchCreateTrigger,
  // Utils
  formatBytes,
  formatDuration,
  getAgentStatusColor,
  getWorkflowStatusColor,
  getStepStatusColor,
};
