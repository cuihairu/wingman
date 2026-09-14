import { describe, it, expect, beforeEach } from 'vitest';
import { get } from 'svelte/store';
import { logs } from '$lib/stores/logs';
import { settings } from '$lib/stores/settings';

describe('logs store', () => {
	beforeEach(() => {
		logs.clear();
		settings.update({ logLevel: 'info' });
	});

	it('add 默认 info 级别，来源为 GUI，时间为 [hh:mm:ss] 格式', () => {
		logs.add('hello');
		const entries = get(logs);
		expect(entries).toHaveLength(1);
		expect(entries[0]).toMatchObject({ message: 'hello', type: 'info', source: 'gui' });
		expect(entries[0].time).toMatch(/^\[\d{2}:\d{2}:\d{2}\]$/);
	});

	it('add 可指定级别', () => {
		logs.add('boom', 'error');
		expect(get(logs)[0].type).toBe('error');
	});

	it('addRuntime 受 settings.logLevel 过滤：info 级别下 debug 在入口被丢弃', () => {
		logs.addRuntime('debug line', 'debug');
		expect(get(logs)).toHaveLength(0);

		logs.addRuntime('warn line', 'warning');
		expect(get(logs)).toHaveLength(1);
		expect(get(logs)[0]).toMatchObject({ message: 'warn line', type: 'warning', source: 'runtime' });
	});

	it('settings.logLevel=debug 时所有级别放行', () => {
		settings.update({ logLevel: 'debug' });
		logs.addRuntime('d', 'debug');
		logs.addRuntime('i', 'info');
		logs.addRuntime('e', 'error');
		expect(get(logs)).toHaveLength(3);
	});

	it('settings.logLevel=error 时仅 error 放行', () => {
		settings.update({ logLevel: 'error' });
		logs.addRuntime('w', 'warning');
		expect(get(logs)).toHaveLength(0);
		logs.addRuntime('e', 'error');
		expect(get(logs)).toHaveLength(1);
	});

	it('addRuntime 优先使用事件时间戳，缺失或非法时回退当前时间', () => {
		const ts = new Date('2024-01-01T10:20:30').getTime();
		logs.addRuntime('with ts', 'info', ts);
		logs.addRuntime('bad ts', 'info', Number.NaN);
		logs.addRuntime('no ts', 'info');
		const entries = get(logs);
		const expected = `[${new Date(ts).toTimeString().split(' ')[0]}]`;
		expect(entries[0].time).toBe(expected);
		expect(entries[1].time).toMatch(/^\[\d{2}:\d{2}:\d{2}\]$/);
		expect(entries[2].time).toMatch(/^\[\d{2}:\d{2}:\d{2}\]$/);
	});

	it('notifyDropped 只增不减，非数字忽略', () => {
		logs.notifyDropped(5);
		expect(get(logs.dropped)).toBe(5);
		logs.notifyDropped(3);
		expect(get(logs.dropped)).toBe(5);
		logs.notifyDropped(Number.NaN);
		expect(get(logs.dropped)).toBe(5);
		logs.notifyDropped(9);
		expect(get(logs.dropped)).toBe(9);
	});

	it('超过 1000 条时裁剪最旧日志', () => {
		settings.update({ logLevel: 'debug' });
		for (let i = 0; i < 1002; i++) logs.addRuntime(`m${i}`, 'info');
		const entries = get(logs);
		expect(entries).toHaveLength(1000);
		expect(entries[0].message).toBe('m2');
		expect(entries[999].message).toBe('m1001');
	});

	it('clear 同时清空列表与 dropped 计数', () => {
		logs.notifyDropped(4);
		logs.add('x');
		logs.clear();
		expect(get(logs)).toHaveLength(0);
		expect(get(logs.dropped)).toBe(0);
	});
});
