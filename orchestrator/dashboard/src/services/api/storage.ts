/**
 * Storage API
 * 当前没有独立对象存储服务。头像上传转换为 Data URL 后由 Profile API 持久化。
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
 * 返回可直接保存到 profile.avatar 的 Data URL。
 */
export async function uploadAsset(file: File, options?: UploadOptions): Promise<UploadResult | undefined> {
  void options;
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
