import { fetchJSON } from '@/services/core/http';

/**
 * Messages API 服务
 */

export interface MessageItem {
  id?: string;
  title?: string;
  content?: string;
  status?: 'read' | 'unread';
  category?: string;
  source?: string;
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

function normalizeMessage(item: any): MessageItem {
  return {
    id: item?.id !== undefined ? String(item.id) : undefined,
    title: item?.title,
    content: item?.content,
    status: item?.status === 'read' ? 'read' : 'unread',
    category: item?.category,
    source: item?.source,
    createdAt: item?.createdAt ?? item?.created_at,
  };
}

export async function listMessages(params: ListMessagesParams = {}): Promise<ListMessagesResponse> {
  const search = new URLSearchParams();
  if (params.status) search.set('status', params.status);
  if (params.pageSize) search.set('pageSize', String(params.pageSize));
  if (params.page) search.set('page', String(params.page));
  const suffix = search.toString();
  const response = await fetchJSON<ListMessagesResponse>(`/api/messages${suffix ? `?${suffix}` : ''}`);
  return {
    items: (response.items || []).map(normalizeMessage),
    total: response.total,
  };
}

export async function unreadCount(): Promise<{ count: number }> {
  return fetchJSON<{ count: number }>('/api/messages/unread-count');
}

export async function markMessageRead(id: string | number): Promise<void> {
  await fetchJSON(`/api/messages/${encodeURIComponent(String(id))}/read`, { method: 'POST' });
}

export async function markAllMessagesRead(): Promise<void> {
  await fetchJSON('/api/messages/read-all', { method: 'POST' });
}
