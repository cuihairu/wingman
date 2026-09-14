/**
 * TriggerFormModal 共享组件：默认值/回填、校验失败、提交成功（创建/更新文案）、
 * 提交失败（Modal.error 与兜底文案）、动作列表增删、取消关闭、自定义标题，
 * 以及 triggerToFormValues / formValuesToConfig 纯函数分支。
 */
import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import React from 'react';
import TriggerFormModal, {
  formValuesToConfig,
  triggerToFormValues,
} from '@/components/TriggerFormModal';
import type { AgentTrigger } from '@/services/wingman';

// 弹窗底部按钮（locale 无关）：footer 首个 primary 为确定，default 为取消
const okButton = () =>
  document.body.querySelector('.ant-modal-footer .ant-btn-primary') as HTMLButtonElement;
const cancelButton = () =>
  document.body.querySelector('.ant-modal-footer .ant-btn-default') as HTMLButtonElement;

const sampleTrigger = {
  id: '9',
  name: 'hp-watch',
  enabled: false,
  type: 'ColorFound',
  condition: {
    type: 'ImageFound',
    value: 'boss.png',
    region: { x: 1, y: 2, width: 3, height: 4 },
    tolerance: 20,
    interval: 500,
    enabled: true,
  },
  actions: [{ type: 'Click', value: '', x: 10, y: 20, delay: 0 }],
  oneShot: true,
  cooldown: 3000,
  lastTriggered: false,
} as AgentTrigger;

function renderModal(props: Partial<Parameters<typeof TriggerFormModal>[0]> = {}) {
  const onSubmit = props.onSubmit ?? jest.fn().mockResolvedValue(undefined);
  const onClose = props.onClose ?? jest.fn();
  const onSaved = props.onSaved ?? jest.fn();
  render(
    <TriggerFormModal
      open
      onSubmit={onSubmit as any}
      onClose={onClose}
      onSaved={onSaved}
      {...props}
    />,
  );
  return { onSubmit, onClose, onSaved };
}

const fillName = (value: string) => {
  fireEvent.change(screen.getByPlaceholderText('如 hp-watch'), { target: { value } });
};

describe('TriggerFormModal 组件', () => {
  it('open=false 不渲染弹窗', () => {
    render(<TriggerFormModal open={false} onSubmit={jest.fn()} onClose={jest.fn()} />);
    expect(document.querySelector('.ant-modal')).toBeNull();
  });

  it('新增默认值：提交载荷为安全默认 config，onSaved 报“已创建”并关闭', async () => {
    const { onSubmit, onClose, onSaved } = renderModal({});
    expect(screen.getByText('新增触发器')).toBeInTheDocument();

    fillName('t1');
    fireEvent.click(okButton());

    await waitFor(() => expect(onSubmit).toHaveBeenCalledTimes(1));
    expect(onSubmit.mock.calls[0][0]).toMatchObject({
      name: 't1',
      enabled: true,
      oneShot: false,
      cooldown: 0,
      condition: {
        type: 'ColorFound',
        value: '',
        tolerance: 10,
        interval: 1000,
        region: { x: 0, y: 0, width: 0, height: 0 },
      },
      actions: [],
    });
    await waitFor(() => expect(onSaved).toHaveBeenCalledWith('触发器 t1 已创建'));
    expect(onClose).toHaveBeenCalled();
  });

  it('编辑回填：标题带触发器名，提交载荷与 triggerToFormValues 一致，文案“已更新”', async () => {
    const { onSubmit, onSaved, onClose } = renderModal({ initial: sampleTrigger });
    expect(screen.getByText('编辑触发器：hp-watch')).toBeInTheDocument();

    fireEvent.click(okButton());

    await waitFor(() => expect(onSubmit).toHaveBeenCalledTimes(1));
    expect(onSubmit.mock.calls[0][0]).toEqual(
      formValuesToConfig(triggerToFormValues(sampleTrigger)),
    );
    await waitFor(() => expect(onSaved).toHaveBeenCalledWith('触发器 hp-watch 已更新'));
    expect(onClose).toHaveBeenCalled();
  });

  it('名称必填：校验失败不提交也不关闭', async () => {
    const { onSubmit, onClose } = renderModal({});

    fireEvent.click(okButton());
    await waitFor(() => expect(screen.getByText('请输入触发器名称')).toBeInTheDocument());

    expect(onSubmit).not.toHaveBeenCalled();
    expect(onClose).not.toHaveBeenCalled();
  });

  it('onSubmit reject：弹错误提示不关闭；message 缺失回退“保存失败”', async () => {
    const onSubmit = jest
      .fn<Promise<unknown>, []>()
      .mockRejectedValueOnce(new Error('boom'))
      .mockRejectedValueOnce({});
    const { onClose } = renderModal({ onSubmit });

    fillName('t2');
    fireEvent.click(okButton());
    // antd confirm 标题渲染两份（.ant-modal-title 与 .ant-confirm-title），用 getAllByText
    await waitFor(() =>
      expect(screen.getAllByText('触发器保存失败').length).toBeGreaterThanOrEqual(1),
    );
    expect(screen.getByText('boom')).toBeInTheDocument();
    expect(onClose).not.toHaveBeenCalled();

    // 第二次提交：无 message 的拒绝 → 兜底文案
    fireEvent.click(okButton());
    await waitFor(() => expect(screen.getAllByText('保存失败').length).toBeGreaterThanOrEqual(1));
    expect(onClose).not.toHaveBeenCalled();
  });

  it('自定义标题覆盖默认标题，取消按钮回调 onClose', () => {
    const { onClose } = renderModal({ title: '批量下发触发器' });
    expect(screen.getByText('批量下发触发器')).toBeInTheDocument();

    fireEvent.click(cancelButton());
    expect(onClose).toHaveBeenCalled();
  });

  it('动作列表：添加动作出现输入行，提交携带动作，删除后消失', async () => {
    const { onSubmit } = renderModal({});

    fireEvent.click(screen.getByText('添加动作'));
    const actionValue = screen.getByPlaceholderText('脚本路径 / 按键 / 文本 / 消息');
    fireEvent.change(actionValue, { target: { value: 'heal.lua' } });

    fillName('t3');
    fireEvent.click(okButton());
    await waitFor(() => expect(onSubmit).toHaveBeenCalledTimes(1));
    expect(onSubmit.mock.calls[0][0].actions).toEqual([
      { type: 'Log', value: 'heal.lua', x: undefined, y: undefined, delay: undefined },
    ]);

    // 删除动作行
    const removeBtn = document.body.querySelector(
      '.ant-modal button.ant-btn-dangerous',
    ) as HTMLButtonElement;
    fireEvent.click(removeBtn);
    await waitFor(() =>
      expect(
        screen.queryByPlaceholderText('脚本路径 / 按键 / 文本 / 消息'),
      ).not.toBeInTheDocument(),
    );
  });
});

describe('triggerToFormValues / formValuesToConfig', () => {
  it('完整触发器逐字段回填', () => {
    expect(triggerToFormValues(sampleTrigger)).toEqual({
      name: 'hp-watch',
      enabled: false,
      oneShot: true,
      cooldown: 3000,
      condition: {
        type: 'ImageFound',
        value: 'boss.png',
        tolerance: 20,
        interval: 500,
        region: { x: 1, y: 2, width: 3, height: 4 },
      },
      actions: [{ type: 'Click', value: '', x: 10, y: 20, delay: 0 }],
    });
  });

  it('空触发器安全回退默认值', () => {
    expect(triggerToFormValues({} as AgentTrigger)).toEqual({
      name: '',
      enabled: true,
      oneShot: false,
      cooldown: 0,
      condition: {
        type: 'ColorFound',
        value: '',
        tolerance: 10,
        interval: 1000,
        region: { x: 0, y: 0, width: 0, height: 0 },
      },
      actions: [],
    });
  });

  it('condition 缺失时 type 回退 trigger.type，动作空字段回退 Log', () => {
    const values = triggerToFormValues({
      name: 'hot',
      type: 'HotkeyPressed',
      actions: [{ value: 'F1' }],
    } as unknown as AgentTrigger);
    expect(values.condition.type).toBe('HotkeyPressed');
    expect(values.actions[0]).toEqual({ type: 'Log', value: 'F1', x: 0, y: 0, delay: 0 });
  });

  it('formValuesToConfig 原样映射为 TriggerConfig 载荷', () => {
    const config = formValuesToConfig(triggerToFormValues(sampleTrigger));
    expect(config.name).toBe('hp-watch');
    expect(config.enabled).toBe(false);
    expect(config.oneShot).toBe(true);
    expect(config.cooldown).toBe(3000);
    expect(config.condition!.type).toBe('ImageFound');
    expect(config.condition!.region).toEqual({ x: 1, y: 2, width: 3, height: 4 });
    expect(config.actions![0]).toEqual({ type: 'Click', value: '', x: 10, y: 20, delay: 0 });
  });
});
