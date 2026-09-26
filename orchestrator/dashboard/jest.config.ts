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
    // 单测预算：jest 默认 5s 在 CI 并发压满 CPU 时不够（基线实测两 flaky
    // 文件 10 run 全部超 5s 预算假红）；waitFor 窗口放宽到 5s（见
    // tests/setupTests.jsx）后，单测内多段等待叠加也需要余量，统一 15s
    testTimeout: 15000,
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
