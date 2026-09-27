import os from 'os';

// 单测预算 = 3 × RTL 异步等待窗口（单测内多段 waitFor 叠加 + 组件内部
// 退避的余量），下限 15s / 封顶 180s：系数为 1（空闲/CI）时与固定
// 15s 完全一致。窗口公式与实测依据见 ./src/testSupport/rtlWindow——
// 这里内联同款公式（jest.config.ts 由 jest 以原生 ESM 加载，不能按
// moduleNameMapper/无扩展名 import 共享模块）
const STARVATION_FACTOR = Math.max(1, Math.ceil(os.loadavg()[0] / Math.max(1, os.cpus().length)));
const RTL_ASYNC_UTIL_TIMEOUT = Math.min(5000 * STARVATION_FACTOR, 60000);
const TEST_TIMEOUT_MS = Math.min(Math.max(15000, RTL_ASYNC_UTIL_TIMEOUT * 3), 180000);

export default async () => {
  return {
    rootDir: '.',
    testEnvironment: 'jsdom',
    testMatch: ['**/?(*.)+(spec|test).[jt]s?(x)'],
    transform: {
      '^.+\\.(t|j)sx?$': [
        'ts-jest',
        {
          tsconfig: '<rootDir>/tsconfig.jest.json',
        },
      ],
    },
    moduleNameMapper: {
      '^@/(.*)$': '<rootDir>/src/$1',
      '\\.(css|less|scss|sass)$': '<rootDir>/tests/mocks/styleMock.js',
    },
    testEnvironmentOptions: {
      url: 'http://localhost:8000',
    },
    setupFiles: ['./tests/setupTests.jsx'],
    // setupRTL 必须在本数组（框架已安装、RTL 自动 cleanup 正常注册）：
    // 详见 tests/setupRTL.jsx 头注释
    setupFilesAfterEnv: ['@testing-library/jest-dom', './tests/setupRTL.jsx'],
    // 单测预算：见文件头 TEST_TIMEOUT_MS 注释（随 CPU 超售系数等比放大，
    // 空闲/CI 环境为 15s，与历史口径一致）
    testTimeout: TEST_TIMEOUT_MS,
    globals: {
      localStorage: null,
    },
    // 覆盖率门禁：--coverage 时任何一项低于阈值即失败
    coverageThreshold: {
      global: {
        statements: 95,
        branches: 95,
        functions: 95,
        lines: 95,
      },
    },
  };
};
