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
    canUserManage: has('users:manage') || has('admin'),
    canRoleManage: has('roles:manage') || has('admin'),
  };
}
