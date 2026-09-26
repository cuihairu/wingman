// RTL 的 waitFor/findBy 默认 1s 等待窗口在 CI 并发压满 CPU 时不够
// （共享机 8 份全量 jest 并跑 + 外部负载实测：loginPage /
// triggerFormModal 两文件 10 run 中 4 run 在 1s 窗口内等不到渲染
// 而假红，纯时序非逻辑回归）。全局放宽到 5s：等待条件不变，只给
// CPU 饥饿下的时序余量，同时覆盖 tests/ 与 src/ 全部 waitFor/findBy
// 站点，无需逐文件改写。
//
// 必须挂在 setupFilesAfterEach（而非 setupFiles）：RTL 首次 import 时
// 以 `typeof afterEach === 'function'` 决定是否注册自动 cleanup，
// setupFiles 阶段测试框架尚未安装、afterEach 不存在，在那里提前
// import 会禁用自动 cleanup，弹窗跨用例堆积污染 document.body。
if (typeof window !== 'undefined') {
  const { configure } = require('@testing-library/react');
  configure({ asyncUtilTimeout: 5000 });
}
