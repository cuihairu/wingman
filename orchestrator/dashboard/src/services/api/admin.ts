import { fetchJSON } from '@/services/core/http';

// ========== 用户管理 ==========

export interface AdminUser {
  id: number;
  username: string;
  role: string;
  active: boolean;
  createdAt?: string;
  updatedAt?: string;
}

export interface ListUsersParams {
  page?: number;
  size?: number;
  role?: string;
  keyword?: string;
}

export interface ListUsersResponse {
  items: AdminUser[];
  total: number;
}

export interface CreateUserPayload {
  username: string;
  password: string;
  role?: string;
  active?: boolean;
}

export interface UpdateUserPayload {
  role?: string;
  active?: boolean;
}

export async function listUsers(params: ListUsersParams = {}): Promise<ListUsersResponse> {
  const search = new URLSearchParams();
  if (params.page) search.set('page', String(params.page));
  if (params.size) search.set('size', String(params.size));
  if (params.role) search.set('role', params.role);
  if (params.keyword) search.set('keyword', params.keyword);
  const suffix = search.toString();
  return fetchJSON<ListUsersResponse>(`/api/admin/users${suffix ? `?${suffix}` : ''}`);
}

export async function getUser(id: number): Promise<AdminUser> {
  return fetchJSON<AdminUser>(`/api/admin/users/${id}`);
}

export async function createUser(payload: CreateUserPayload): Promise<{ user: AdminUser }> {
  return fetchJSON(`/api/admin/users`, {
    method: 'POST',
    body: JSON.stringify(payload),
  });
}

export async function updateUser(
  id: number,
  payload: UpdateUserPayload,
): Promise<{ user: AdminUser }> {
  return fetchJSON(`/api/admin/users/${id}`, {
    method: 'PUT',
    body: JSON.stringify(payload),
  });
}

export async function deleteUser(id: number): Promise<void> {
  await fetchJSON(`/api/admin/users/${id}`, { method: 'DELETE' });
}

export async function resetUserPassword(id: number, newPassword: string): Promise<void> {
  await fetchJSON(`/api/admin/users/${id}/reset-password`, {
    method: 'POST',
    body: JSON.stringify({ newPassword }),
  });
}

// ========== 角色管理 ==========

export interface AdminPermission {
  id: number;
  code: string;
  name?: string;
  description?: string;
  category?: string;
  builtin?: boolean;
}

export interface AdminRole {
  id: number;
  code: string;
  name?: string;
  description?: string;
  builtin?: boolean;
  permissions: AdminPermission[];
  createdAt?: string;
}

export interface ListRolesResponse {
  items: AdminRole[];
  total: number;
}

export interface CreateRolePayload {
  code: string;
  name?: string;
  description?: string;
  permissions?: string[];
}

export interface UpdateRolePayload {
  name?: string;
  description?: string;
  permissions?: string[];
}

export async function listRoles(): Promise<ListRolesResponse> {
  return fetchJSON<ListRolesResponse>(`/api/admin/roles`);
}

export async function getRole(code: string): Promise<AdminRole> {
  return fetchJSON<AdminRole>(`/api/admin/roles/${encodeURIComponent(code)}`);
}

export async function createRole(payload: CreateRolePayload): Promise<{ role: AdminRole }> {
  return fetchJSON(`/api/admin/roles`, {
    method: 'POST',
    body: JSON.stringify(payload),
  });
}

export async function updateRole(
  code: string,
  payload: UpdateRolePayload,
): Promise<{ role: AdminRole }> {
  return fetchJSON(`/api/admin/roles/${encodeURIComponent(code)}`, {
    method: 'PUT',
    body: JSON.stringify(payload),
  });
}

export async function deleteRole(code: string): Promise<void> {
  await fetchJSON(`/api/admin/roles/${encodeURIComponent(code)}`, { method: 'DELETE' });
}

// ========== 权限目录 ==========
//
// 注意：本函数返回后端原始权限目录（AdminPermission，仅含 code 字段），
// 供 Admin/Roles 页面做权限分配多选用。Profile 页申请列表需要的
// resource/action 拆分结构请改用 `@/services/api/permissions` 的 listPermissions。
// 两者刻意分别命名以避免 `export *` 聚合时产生导出歧义。

export interface ListPermissionCatalogResponse {
  items: AdminPermission[];
  total: number;
}

export async function listPermissionCatalog(category?: string): Promise<ListPermissionCatalogResponse> {
  const suffix = category ? `?category=${encodeURIComponent(category)}` : '';
  return fetchJSON<ListPermissionCatalogResponse>(`/api/admin/permissions${suffix}`);
}
