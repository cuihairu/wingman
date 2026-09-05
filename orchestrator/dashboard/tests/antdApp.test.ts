/**
 * antdApp.ts：App API 实例注册与获取。
 */
import { getMessage, getNotification, setAppApi } from '@/utils/antdApp';

describe('antdApp', () => {
  it('未注册时返回 undefined', () => {
    expect(getMessage()).toBeUndefined();
    expect(getNotification()).toBeUndefined();
  });

  it('setAppApi 注册后可获取 message/notification', () => {
    const message = { success: jest.fn() } as any;
    const notification = { open: jest.fn() } as any;
    setAppApi({ message, notification });

    expect(getMessage()).toBe(message);
    expect(getNotification()).toBe(notification);
  });
});
