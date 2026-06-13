/**
 * Messages API 服务存根
 * 后端尚未提供消息通知接口；返回 rejected 以触发 UI 的"功能未实现"降级分支。
 */

export interface MessageItem {
  id?: string;
  title?: string;
  content?: string;
  status?: 'read' | 'unread';
  createdAt?: string;
}

export interface ListMessagesParams {
  status?: string;
  pageSize?: number;
  page?: number;
}

export interface ListMessagesResponse {
  items: MessageItem[];
  total?: number;
}

export async function listMessages(_params: ListMessagesParams): Promise<ListMessagesResponse> {
  return Promise.reject(new Error('messages API not implemented'));
}
