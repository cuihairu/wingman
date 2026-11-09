/**
 * guacamole-common-js 的 jest.mock 工厂内容（jsdom 无真实 WS）。
 *
 * 拆分前 guac mock 只在 index.test.tsx 里有一份；拆分后各测试文件统一以
 *   jest.mock('guacamole-common-js', () => {
 *     const m = require('./index.testMocks');
 *     return { __esModule: true, default: m.mocks, ...m.mocks };
 *   })
 * 复用本模块，mock 形状与拆分前逐字一致（named + default 双出口：
 * 组件源码 default 导入、测试读 named 句柄）。
 */

// 每个 Client 一个稳定 display 实例：测试要读 scale 调用断言缩放
const makeDisplay = () => ({
  getElement: () => document.createElement('div'),
  getWidth: jest.fn(() => 1280),
  getHeight: jest.fn(() => 800),
  // 1.5.0 的 scale 是方法（this.scale = function(scale)），赋值不生效
  scale: jest.fn(),
  showCursor: jest.fn(),
  onresize: undefined as undefined | (() => void),
});
const mocks = {
  WebSocketTunnel: jest.fn().mockImplementation((url: string) => ({ url })),
  Client: jest.fn().mockImplementation(() => {
    const display = makeDisplay();
    return {
      display,
      getDisplay: () => display,
      connect: jest.fn(),
      disconnect: jest.fn(),
      sendMouseState: jest.fn(),
      sendKeyEvent: jest.fn(),
      createClipboardStream: jest.fn().mockImplementation(() => ({
        sendBlob: jest.fn(),
        sendEnd: jest.fn(),
        onack: null,
      })),
      createFileStream: jest.fn().mockImplementation((_m: string, filename: string) => ({
        sendBlob: jest.fn(),
        sendEnd: jest.fn(),
        filename,
        onack: null,
      })),
    };
  }),
  BlobWriter: jest.fn().mockImplementation((stream: unknown) => ({
    sendBlob: jest.fn(),
    oncomplete: null,
    onerror: null,
    onprogress: null,
    stream,
  })),
  Mouse: jest.fn().mockImplementation(() => ({})),
  Keyboard: jest.fn().mockImplementation(() => ({})),
  Touchscreen: jest.fn().mockImplementation(() => ({})),
  Status: { Code: { SUCCESS: 0x0000, UNSUPPORTED: 0x0100, SERVER_ERROR: 0x0200 } },
};

export default mocks;
export { mocks };
