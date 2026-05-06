/**
 * Support API 服务存根
 * TODO: 根据实际后端 API 实现支持反馈功能
 */

export interface CreateFeedbackParams {
  category: string;
  content: string;
  priority?: string;
  source?: string;
}

export async function createFeedback(params: CreateFeedbackParams): Promise<void> {
  // 存根实现：模拟提交成功
  console.log('[Support Stub] Feedback submitted:', params);
  return Promise.resolve();
}
