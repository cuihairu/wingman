import { render, fireEvent, act } from '@testing-library/react';
import React from 'react';
import { BRAND } from '@/config/branding';
import { history } from '@umijs/max';
import Login from './index';

const waitTime = (time: number = 100) => {
  return new Promise((resolve) => {
    setTimeout(() => {
      resolve(true);
    }, time);
  });
};

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
      fireEvent.change(passwordInput, { target: { value: 'ant.design' } });
    });

    const submitButton = await rootContainer.findByRole('button', { name: /登\s*录/ });
    await submitButton.click();

    await waitTime(200);

    expect(localStorage.setItem).toHaveBeenCalledWith('token', 'test-token');
    expect(history.push).toHaveBeenCalledWith('/');

    rootContainer.unmount();
  });
});
