/**
 * TriggerFormModal 触发器新增/编辑表单（共享组件）
 * 从 Monitor 页抽取：Monitor 单 agent 透传与 Agents 页批量下发共用。
 * 组件本身不感知提交目标，表单校验后经 onSubmit 回调上送 runtime
 * TriggerConfig 载荷（trigger.add / trigger.update 语义由调用方决定）。
 */
import React, { useEffect, useState } from 'react';
import { Button, Divider, Form, Input, InputNumber, Modal, Select, Space, Switch } from 'antd';
import { DeleteOutlined, PlusOutlined } from '@ant-design/icons';
import type { AgentTrigger, AgentTriggerConfigInput } from '@/services/wingman';

// runtime TriggerType 全量 11 种（与 trigger_handler.cpp 对齐）
export const TRIGGER_CONDITION_TYPES = [
  'ColorFound',
  'ColorLost',
  'ImageFound',
  'ImageLost',
  'WindowOpened',
  'WindowClosed',
  'ProcessStarted',
  'ProcessStopped',
  'TimeElapsed',
  'HotkeyPressed',
  'PixelChanged',
];

// runtime BasicTriggerAction 全量 10 种
export const TRIGGER_ACTION_TYPES = [
  'RunScript',
  'Click',
  'KeyPress',
  'Type',
  'StopScript',
  'PauseScript',
  'ShowMessage',
  'PlayAudio',
  'Log',
  'Delay',
];

export interface TriggerFormValues {
  name: string;
  enabled: boolean;
  oneShot: boolean;
  cooldown: number;
  condition: {
    type: string;
    value: string;
    tolerance: number;
    interval: number;
    region: { x: number; y: number; width: number; height: number };
  };
  actions: Array<{ type: string; value: string; x: number; y: number; delay: number }>;
}

// trigger → 表单值（编辑模式回填）
export function triggerToFormValues(trigger: AgentTrigger): TriggerFormValues {
  const condition = trigger.condition || ({} as AgentTrigger['condition']);
  return {
    name: trigger.name || '',
    enabled: trigger.enabled !== false,
    oneShot: trigger.oneShot === true,
    cooldown: trigger.cooldown || 0,
    condition: {
      type: condition.type || trigger.type || 'ColorFound',
      value: condition.value || '',
      tolerance: condition.tolerance ?? 10,
      interval: condition.interval ?? 1000,
      region: condition.region || { x: 0, y: 0, width: 0, height: 0 },
    },
    actions: (trigger.actions || []).map((action) => ({
      type: action.type || 'Log',
      value: action.value || '',
      x: action.x || 0,
      y: action.y || 0,
      delay: action.delay || 0,
    })),
  };
}

// 表单值 → runtime TriggerConfig 载荷
export function formValuesToConfig(values: TriggerFormValues): AgentTriggerConfigInput {
  return {
    name: values.name,
    enabled: values.enabled,
    oneShot: values.oneShot,
    cooldown: values.cooldown,
    condition: {
      type: values.condition.type,
      value: values.condition.value,
      tolerance: values.condition.tolerance,
      interval: values.condition.interval,
      region: values.condition.region,
    },
    actions: values.actions.map((action) => ({
      type: action.type,
      value: action.value,
      x: action.x,
      y: action.y,
      delay: action.delay,
    })),
  };
}

export interface TriggerFormModalProps {
  open: boolean;
  /** 覆盖默认标题（"新增触发器"/"编辑触发器：name"）；批量下发等场景自定义 */
  title?: string;
  /** 编辑回填数据；null/undefined = 新增默认值 */
  initial?: AgentTrigger | null;
  /** 提交回调：resolve 即视为保存成功（随后触发 onSaved 并关闭）；reject 则弹窗提示且不关闭 */
  onSubmit: (config: AgentTriggerConfigInput) => Promise<unknown>;
  onClose: () => void;
  /** 保存成功后的提示回调 */
  onSaved?: (message: string) => void;
}

// TriggerFormModal 触发器新增/编辑表单（经 Go server 透传 runtime trigger.add/update）
function TriggerFormModal({
  open,
  title,
  initial,
  onSubmit,
  onClose,
  onSaved,
}: TriggerFormModalProps) {
  const [form] = Form.useForm<TriggerFormValues>();
  const [submitting, setSubmitting] = useState(false);

  useEffect(() => {
    if (open) {
      form.resetFields();
      form.setFieldsValue(
        initial
          ? triggerToFormValues(initial)
          : {
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
            },
      );
    }
  }, [open, initial, form]);

  const handleOk = async () => {
    let values: TriggerFormValues;
    try {
      values = await form.validateFields();
    } catch {
      return;
    }
    setSubmitting(true);
    try {
      const config = formValuesToConfig(values);
      await onSubmit(config);
      onSaved?.(`触发器 ${values.name} 已${initial ? '更新' : '创建'}`);
      onClose();
    } catch (error) {
      // 交给外层事件流提示（onError 简化为 Modal 内提示）
      const message = (error as { message?: string })?.message || '保存失败';
      Modal.error({ title: '触发器保存失败', content: message });
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <Modal
      open={open}
      title={title ?? (initial ? `编辑触发器：${initial.name}` : '新增触发器')}
      width={680}
      confirmLoading={submitting}
      onOk={handleOk}
      onCancel={onClose}
      destroyOnClose
    >
      <Form form={form} layout="vertical" initialValues={{ enabled: true }}>
        <Space size={16} style={{ display: 'flex' }} align="start">
          <Form.Item
            name="name"
            label="名称"
            rules={[{ required: true, message: '请输入触发器名称' }]}
            style={{ flex: 1, minWidth: 240 }}
          >
            <Input placeholder="如 hp-watch" />
          </Form.Item>
          <Form.Item name="cooldown" label="冷却 (ms)">
            <InputNumber min={0} step={500} />
          </Form.Item>
          <Form.Item name="enabled" label="启用" valuePropName="checked">
            <Switch size="small" />
          </Form.Item>
          <Form.Item name="oneShot" label="一次性" valuePropName="checked">
            <Switch size="small" />
          </Form.Item>
        </Space>

        <Divider orientation="left" plain>
          触发条件
        </Divider>
        <Space size={16} style={{ display: 'flex' }} align="start" wrap>
          <Form.Item name={['condition', 'type']} label="类型" style={{ minWidth: 160 }}>
            <Select
              options={TRIGGER_CONDITION_TYPES.map((type) => ({ value: type, label: type }))}
            />
          </Form.Item>
          <Form.Item
            name={['condition', 'value']}
            label="条件值（颜色 #rrggbb / 图片路径 / 窗口标题 / 进程名 / 毫秒 / 键名）"
            style={{ flex: 1, minWidth: 260 }}
          >
            <Input placeholder="#ff0000" />
          </Form.Item>
          <Form.Item name={['condition', 'tolerance']} label="容差">
            <InputNumber min={0} max={255} />
          </Form.Item>
          <Form.Item name={['condition', 'interval']} label="检测间隔 (ms)">
            <InputNumber min={0} step={100} />
          </Form.Item>
        </Space>
        <Space size={16} style={{ display: 'flex' }} wrap>
          {(['x', 'y', 'width', 'height'] as const).map((key) => (
            <Form.Item
              key={key}
              name={['condition', 'region', key]}
              label={`区域 ${key}`}
              initialValue={0}
            >
              <InputNumber min={0} />
            </Form.Item>
          ))}
        </Space>

        <Divider orientation="left" plain>
          触发动作
        </Divider>
        <Form.List name="actions">
          {(fields, { add, remove }) => (
            <>
              {fields.map((field) => (
                <Space key={field.key} size={8} style={{ display: 'flex' }} align="baseline" wrap>
                  <Form.Item name={[field.name, 'type']} initialValue="Log" noStyle>
                    <Select
                      style={{ width: 140 }}
                      options={TRIGGER_ACTION_TYPES.map((type) => ({ value: type, label: type }))}
                    />
                  </Form.Item>
                  <Form.Item name={[field.name, 'value']} noStyle>
                    <Input placeholder="脚本路径 / 按键 / 文本 / 消息" style={{ width: 220 }} />
                  </Form.Item>
                  <Form.Item name={[field.name, 'x']} noStyle>
                    <InputNumber placeholder="x" style={{ width: 80 }} />
                  </Form.Item>
                  <Form.Item name={[field.name, 'y']} noStyle>
                    <InputNumber placeholder="y" style={{ width: 80 }} />
                  </Form.Item>
                  <Form.Item name={[field.name, 'delay']} noStyle>
                    <InputNumber placeholder="延迟" style={{ width: 90 }} />
                  </Form.Item>
                  <Button
                    type="text"
                    danger
                    icon={<DeleteOutlined />}
                    onClick={() => remove(field.name)}
                  />
                </Space>
              ))}
              <Form.Item>
                <Button
                  type="dashed"
                  block
                  icon={<PlusOutlined />}
                  onClick={() => add({ type: 'Log' })}
                >
                  添加动作
                </Button>
              </Form.Item>
            </>
          )}
        </Form.List>
      </Form>
    </Modal>
  );
}

export default TriggerFormModal;
