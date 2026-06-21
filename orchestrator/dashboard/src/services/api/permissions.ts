import { fetchJSON } from '@/services/core/http';

/**
 * Permissions 目录 API
 * 后端权限目录使用 code 字段（resource:action），这里规范化为 Profile 页申请列表需要的结构。
 */

export interface PermissionRecord {
  id: string;
  name: string;
  description?: string;
  resource: string;
  action: string;
  category?: string;
}

export interface ListPermissionsParams {
  page?: number;
  pageSize?: number;
  resource?: string;
  category?: string;
}

export interface ListPermissionsResponse {
  items: PermissionRecord[];
  total?: number;
}

interface BackendPermission {
  id?: number;
  ID?: number;
  code: string;
  name?: string;
  description?: string;
  category?: string;
}

interface BackendListPermissionsResponse {
  items?: BackendPermission[];
  total?: number;
}

function splitPermissionCode(code: string) {
  const [resource, ...rest] = code.split(':');
  return {
    resource: resource || code,
    action: rest.join(':') || '*',
  };
}

function normalizePermission(item: BackendPermission): PermissionRecord {
  const code = String(item.code || item.id || item.ID || '');
  const parsed = splitPermissionCode(code);
  return {
    id: code,
    name: item.name || code,
    description: item.description,
    resource: parsed.resource,
    action: parsed.action,
    category: item.category,
  };
}

export async function listPermissions(
  params: ListPermissionsParams = {},
): Promise<ListPermissionsResponse> {
  const search = new URLSearchParams();
  if (params.category) search.set('category', params.category);

  const suffix = search.toString();
  const response = await fetchJSON<BackendListPermissionsResponse>(
    `/api/admin/permissions${suffix ? `?${suffix}` : ''}`,
  );

  let items = (response.items || []).map(normalizePermission);
  if (params.resource) {
    items = items.filter((item) => item.resource === params.resource);
  }

  const total = items.length;
  if (params.pageSize && params.pageSize > 0) {
    const page = Math.max(params.page || 1, 1);
    const start = (page - 1) * params.pageSize;
    items = items.slice(start, start + params.pageSize);
  }

  return { items, total };
}
