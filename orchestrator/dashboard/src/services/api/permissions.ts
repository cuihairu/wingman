/**
 * Permissions API 服务存根
 * TODO: 根据实际后端 API 实现权限目录功能
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

export async function listPermissions(params: ListPermissionsParams): Promise<ListPermissionsResponse> {
  // 存根实现：返回空权限目录
  return Promise.resolve({ items: [] });
}
