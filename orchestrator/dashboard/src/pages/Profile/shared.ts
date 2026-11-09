/** Profile 页内共享类型（权限申请目标项，目录项与兜底模板同形） */
export type PermissionApplyItem = {
  id: string;
  name: string;
  description?: string;
  resource: string;
  action: string;
  category?: string;
};
