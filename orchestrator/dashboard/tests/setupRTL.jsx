// RTL 的 waitFor/findBy 等待窗口按 CPU 超售程度等比放大（共享机全量
// jest 13 worker + 外部负载实测：固定 1s/5s 窗口在 load 达核数 6 倍
// 以上时，对「断言条件正确、只是机器慢」的用例假红；同负载单跑
// --runInBand 全绿，纯时序非逻辑回归）。公式、封顶与依据见
// @/testSupport/rtlWindow；空闲机器与 CI runner 系数为 1，行为与
// 固定 5s 完全一致。
//
// 必须挂在 setupFilesAfterEnv（而非 setupFiles）：RTL 首次 import 时
// 以 `typeof afterEach === 'function'` 决定是否注册自动 cleanup，
// setupFiles 阶段测试框架尚未安装、afterEach 不存在，在那里提前
// import 会禁用自动 cleanup，弹窗跨用例堆积污染 document.body。
if (typeof window !== 'undefined') {
  const { configure } = require('@testing-library/react');
  const { RTL_ASYNC_UTIL_TIMEOUT } = require('@/testSupport/rtlWindow');
  configure({ asyncUtilTimeout: RTL_ASYNC_UTIL_TIMEOUT });
}
