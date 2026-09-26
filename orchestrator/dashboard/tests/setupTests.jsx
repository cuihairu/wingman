const localStorageMock = {
  getItem: jest.fn(),
  setItem: jest.fn(),
  removeItem: jest.fn(),
  clear: jest.fn(),
};

// node 环境下的测试文件（@jest-environment node）没有 window，
// 这里统一守卫，保证两类环境都能复用同一份 setup。
if (typeof window !== 'undefined') {
  Object.defineProperty(window, 'localStorage', {
    value: localStorageMock,
    writable: true,
  });
}
global.localStorage = localStorageMock;

// jsdom 不带 TextEncoder/TextDecoder（RemoteDesktopModal 的 base64 流
// 载荷编解码用），从 node:util 补齐
if (typeof window !== 'undefined' && !window.TextEncoder) {
  const util = require('util');
  window.TextEncoder = util.TextEncoder;
  window.TextDecoder = util.TextDecoder;
}

// jsdom 的 Blob 没有 text()（浏览器现代标准方法；录像回放里
// guacamole-common-js 内部 clientState.text() 也会调到），
// 用 FileReader 补齐（与库内部读法一致）
if (typeof Blob !== 'undefined' && !Blob.prototype.text) {
  Blob.prototype.text = function blobText() {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => resolve(String(reader.result));
      reader.onerror = () => reject(reader.error ?? new Error('Blob read failed'));
      reader.readAsText(this);
    });
  };
}

Object.defineProperty(URL, 'createObjectURL', {
  writable: true,
  value: jest.fn(),
});

class Worker {
  constructor(stringUrl) {
    this.url = stringUrl;
    this.onmessage = () => {};
  }

  postMessage(msg) {
    this.onmessage(msg);
  }
}
if (typeof window !== 'undefined') {
  window.Worker = Worker;
}

if (typeof window !== 'undefined') {
  // ref: https://github.com/ant-design/ant-design/issues/18774
  if (!window.matchMedia) {
    Object.defineProperty(global.window, 'matchMedia', {
      writable: true,
      configurable: true,
      value: jest.fn(() => ({
        matches: false,
        addListener: jest.fn(),
        removeListener: jest.fn(),
      })),
    });
  }
  if (!window.matchMedia) {
    Object.defineProperty(global.window, 'matchMedia', {
      writable: true,
      configurable: true,
      value: jest.fn((query) => ({
        matches: query.includes('max-width'),
        addListener: jest.fn(),
        removeListener: jest.fn(),
      })),
    });
  }
}
const errorLog = console.error;
if (typeof window !== 'undefined') {
  Object.defineProperty(global.window.console, 'error', {
    writable: true,
    configurable: true,
    value: (...rest) => {
      const logStr = rest.join('');
      if (logStr.includes('Warning: An update to %s inside a test was not wrapped in act(...)')) {
        return;
      }
      if (logStr.includes('ReactDOMTestUtils.act')) {
        return;
      }
      errorLog(...rest);
    },
  });
} else {
  console.error = (...rest) => {
    const logStr = rest.join('');
    if (logStr.includes('Warning: An update to %s inside a test was not wrapped in act(...)')) {
      return;
    }
    if (logStr.includes('ReactDOMTestUtils.act')) {
      return;
    }
    errorLog(...rest);
  };
}

jest.mock(
  '@umijs/max',
  () => {
    const React = require('react');
    const request = jest.fn(async (url) => {
      if (typeof url === 'string' && url.includes('/api/v1/auth/login')) {
        return { token: 'test-token', user: { username: 'admin', roles: ['admin'] } };
      }
      if (typeof url === 'string' && url.includes('/api/v1/profile')) {
        return { username: 'admin', roles: ['admin'] };
      }
      return {};
    });

    return {
      __esModule: true,
      history: {
        push: jest.fn(),
        location: { pathname: '/user/login' },
      },
      request,
      useIntl: () => ({
        formatMessage: ({ defaultMessage }) => defaultMessage,
      }),
      FormattedMessage: ({ defaultMessage }) =>
        React.createElement(React.Fragment, null, defaultMessage),
      SelectLang: () => null,
      Helmet: ({ children }) => React.createElement(React.Fragment, null, children),
      useModel: () => ({
        initialState: {
          fetchUserInfo: async () => ({ username: 'admin', roles: ['admin'] }),
        },
        setInitialState: jest.fn(),
      }),
    };
  },
  { virtual: true },
);
