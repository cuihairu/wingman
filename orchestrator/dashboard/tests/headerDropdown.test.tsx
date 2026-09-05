/**
 * HeaderDropdown：antd Dropdown 包装与响应式样式（未被 mock 的真实渲染）。
 */
import { render, screen } from '@testing-library/react';
import React from 'react';
import HeaderDropdown from '@/components/HeaderDropdown';

describe('HeaderDropdown（真实渲染）', () => {
  it('渲染触发子元素并透传 dropdown 属性', () => {
    render(
      <HeaderDropdown menu={{ items: [{ key: 'a', label: 'A' }] }} placement="bottomRight">
        <button>trigger</button>
      </HeaderDropdown>,
    );
    expect(screen.getByText('trigger')).toBeInTheDocument();
  });
});
