/**
 * 组件 barrel（src/components/index.ts）：re-export 编译后以 getter 形式存在，
 * 逐一访问确保导出映射完整；并经 barrel 渲染 Footer 验证可用性。
 */
import { render, screen } from '@testing-library/react';
import React from 'react';

import * as components from '@/components';

describe('components barrel', () => {
  it('导出的全部组件均可访问（re-export getter 生效）', () => {
    expect(components.Footer).toBeDefined();
    expect(components.Question).toBeDefined();
    expect(components.SelectLang).toBeDefined();
    expect(components.AvatarDropdown).toBeDefined();
    expect(components.AvatarName).toBeDefined();
    expect(components.PageStatePanel).toBeDefined();
    expect(components.ScreenshotView).toBeDefined();
  });

  it('经 barrel 渲染 Footer 页脚', () => {
    const { container } = render(<components.Footer />);
    expect(container.querySelector('footer, .ant-pro-global-footer')).not.toBeNull();
    expect(screen.getAllByText(/Wingman/).length).toBeGreaterThan(0);
  });
});
