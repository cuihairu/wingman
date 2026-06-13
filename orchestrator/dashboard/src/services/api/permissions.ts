/**
 * Permissions 目录 API 存根
 * 后端仅提供 /profile/permissions（当前用户授权），未提供权限目录列表接口；
 * 返回 rejected 以触发 UI 的"功能未实现"降级分支。
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

export async function listPermissions(
  _params: ListPermissionsParams,
): Promise<ListPermissionsResponse> {
  return Promise.reject(new Error('permissions catalog API not implemented'));
}
