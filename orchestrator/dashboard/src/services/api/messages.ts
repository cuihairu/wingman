/**
 * Messages API 服务存根
 * TODO: 根据实际后端 API 实现消息通知功能
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

export async function listMessages(params: ListMessagesParams): Promise<ListMessagesResponse> {
  // 存根实现：返回空消息列表
  return Promise.resolve({ items: [] });
}
