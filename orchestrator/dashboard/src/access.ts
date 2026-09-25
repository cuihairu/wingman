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
    // 批量运行/停止脚本与单 agent run 同权限码
    canScriptRun: has('scripts:run') || has('admin'),
    // 远程桌面（Guacamole 像素面）：view=监看+录像检索；control=接管+删录像
    canDesktopView: has('desktop:view') || has('admin'),
    canDesktopControl: has('desktop:control') || has('admin'),
    // 任一管理区权限即可看到 Admin 菜单；具体子页由各自 access 守卫细控
    canAccessAdmin: has('admin') || has('users:manage') || has('roles:manage'),
  };
}
