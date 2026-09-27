import { render, fireEvent, act, waitFor } from '@testing-library/react';
import React from 'react';
import { BRAND } from '@/config/branding';
import { history } from '@umijs/max';
import Login from './index';

describe('Login Page', () => {
  it('should show login form', async () => {
    const rootContainer = render(<Login />);

    await rootContainer.findAllByText(BRAND.title);

    expect(rootContainer.baseElement?.querySelector('.ant-pro-form-login-desc')?.textContent).toBe(
      BRAND.subTitle,
    );

    rootContainer.unmount();
  });

  it('should login success', async () => {
    const rootContainer = render(<Login />);

    await rootContainer.findAllByText(BRAND.title);

    const userNameInput = await rootContainer.findByPlaceholderText('用户名');

    act(() => {
      fireEvent.change(userNameInput, { target: { value: 'admin' } });
    });

    const passwordInput = await rootContainer.findByPlaceholderText('密码');

    act(() => {
      fireEvent.change(passwordInput, { target: { value: 'admin123456' } });
    });

    const submitButton = await rootContainer.findByRole('button', { name: /登\s*录/ });
    await submitButton.click();

    // 等条件而非固定 sleep：登录链（request mock → token 写入 → 跳转）
    // 全是微任务，空闲机器毫秒级完成，但共享机 CPU 超售下固定 200ms
    // 不够 wall-clock，旧写法在 load 高时假红
    await waitFor(() => {
      expect(localStorage.setItem).toHaveBeenCalledWith('token', 'test-token');
      expect(history.push).toHaveBeenCalledWith('/');
    });

    rootContainer.unmount();
  });
});
