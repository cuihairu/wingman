/**
 * @see https://umijs.org/docs/max/access#access
 * Wingman 简化版权限控制
 * */
type AccessCurrentUser = {
  access?: string;
};

export default function access(initialState: { currentUser?: AccessCurrentUser } | undefined) {
  const acc = ((initialState?.currentUser as any)?.access as string | undefined) || '';
  const perms = new Set(
    acc
      .split(',')
      .map((token) => token.trim().toLowerCase())
      .filter(Boolean),
  );
  const has = (p: string) => {
    const key = (p || '').toLowerCase();
    return perms.has('*') || perms.has(key);
  };
  return {
    canAdmin: has('admin'),
    canAgentManage: has('agents:manage') || has('admin'),
    canUserManage: has('users:manage') || has('admin'),
    canRoleManage: has('roles:manage') || has('admin'),
    // 任一管理区权限即可看到 Admin 菜单；具体子页由各自 access 守卫细控
    canAccessAdmin: has('admin') || has('users:manage') || has('roles:manage'),
  };
}
