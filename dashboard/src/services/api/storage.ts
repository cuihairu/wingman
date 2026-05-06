/**
 * Storage API 服务存根
 * TODO: 根据实际后端 API 实现文件存储上传功能
 */

export interface UploadOptions {
  path?: string;
  contentType?: string;
}

export interface UploadResult {
  URL?: string;
  key?: string;
  etag?: string;
}

/**
 * 构建头像对象键
 */
export function buildAvatarObjectKey(file: File): string {
  const timestamp = Date.now();
  const ext = file.name.split('.').pop() || 'jpg';
  return `avatars/${timestamp}.${ext}`;
}

/**
 * 上传资源文件
 * 注意：当前为存根实现，实际使用时需要对接真实存储服务
 */
export async function uploadAsset(file: File, options?: UploadOptions): Promise<UploadResult | undefined> {
  // 存根实现：转换为本地预览 URL
  if (typeof URL !== 'undefined' && typeof FileReader !== 'undefined') {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => {
        resolve({ URL: reader.result as string });
      };
      reader.onerror = reject;
      reader.readAsDataURL(file);
    });
  }
  return undefined;
}
