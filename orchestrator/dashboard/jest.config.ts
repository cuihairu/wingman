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
    setupFilesAfterEnv: ['@testing-library/jest-dom'],
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
