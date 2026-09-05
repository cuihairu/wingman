/**
 * transform.ts 分支补全：applyTransform 透传、getByPath 边界、
 * 谓词算子（eq/ne/gt/lt/match）、directive 算子（mul/div/add/sub/iso*）、
 * aggregate 空数组/NaN 过滤、resolveValue 三种取值形式。
 */
import { applyTransform, getByPath } from '@/plugin/transform';

describe('applyTransform 透传分支', () => {
  const root = { a: 1 };

  it('无 transform 时原样返回', () => {
    expect(applyTransform(root, undefined)).toBe(root);
  });

  it('expr 为空白字符串且无 template 时原样返回', () => {
    expect(applyTransform(root, { expr: '   ' })).toBe(root);
  });

  it('template 为原始值（数字）时直接返回', () => {
    expect(applyTransform(root, { template: 42 })).toBe(42);
  });

  it('template 数组逐项映射', () => {
    expect(applyTransform({ x: 5 }, { template: ['$.x', 'lit'] })).toEqual([5, 'lit']);
  });

  it('template 字符串 $$. 前缀从 root 取值，普通字符串透传', () => {
    expect(applyTransform(root, { template: '$$.a' })).toBe(1);
    expect(applyTransform(root, { template: '$.a' })).toBe(1);
    expect(applyTransform(root, { template: 'plain' })).toBe('plain');
  });
});

describe('getByPath 边界', () => {
  it('空表达式返回 undefined，$ 与 $. 返回根对象', () => {
    expect(getByPath({ a: 1 }, '')).toBeUndefined();
    expect(getByPath({ a: 1 }, '$')).toEqual({ a: 1 });
    expect(getByPath({ a: 1 }, '$.')).toEqual({ a: 1 });
  });

  it('无 $. 前缀时自动补齐', () => {
    expect(getByPath({ a: { b: 2 } }, 'a.b')).toBe(2);
  });

  it('数组下标越界/非数组下标返回 undefined', () => {
    expect(getByPath({ arr: [1, 2] }, '$.arr[1]')).toBe(2);
    expect(getByPath({ arr: [1] }, '$.arr[5]')).toBeUndefined();
    expect(getByPath({ notArr: 3 }, '$.notArr[0]')).toBeUndefined();
  });

  it('中间节点为 null 时返回 undefined', () => {
    expect(getByPath({ a: null }, '$.a.b')).toBeUndefined();
  });

  it('非法标识符键走裸取值分支', () => {
    expect(getByPath({ a: 7 }, '$.a-b')).toBeUndefined();
    expect(getByPath({ arr: [{ v: 9 }] }, '$.arr[0].v')).toBe(9);
  });
});

describe('谓词算子', () => {
  const root = {};
  const data = { items: [{ n: 1, s: 'alpha' }, { n: 2, s: 'beta' }] };
  const forEach = (where: any) =>
    applyTransform(data, {
      template: { forEach: { path: '$.items', where, template: { n: '$.n' } } },
    });

  it('eq / ne', () => {
    expect(forEach({ eq: ['$.n', 1] })).toEqual([{ n: 1 }]);
    expect(forEach({ ne: ['$.n', 1] })).toEqual([{ n: 2 }]);
  });

  it('gt / lt', () => {
    expect(forEach({ gt: ['$.n', 1] })).toEqual([{ n: 2 }]);
    expect(forEach({ lt: ['$.n', 2] })).toEqual([{ n: 1 }]);
  });

  it('contains / match（字符串正则与 RegExp 实例）', () => {
    expect(forEach({ contains: ['$.s', 'alp'] })).toEqual([{ n: 1 }]);
    expect(forEach({ match: ['$.s', '^be'] })).toEqual([{ n: 2 }]);
    expect(forEach({ match: ['$.s', new RegExp('^al')] })).toEqual([{ n: 1 }]);
  });

  it('where 参数为非数组标量时与 undefined 比较，$$. 前缀取 root 值', () => {
    // 非数组参数只取 args[0]，与 args[1]=undefined 比较
    expect(forEach({ eq: 1 })).toEqual([]);
    const withRoot = { threshold: 1, items: [{ n: 1 }, { n: 2 }] };
    expect(
      applyTransform(withRoot, {
        template: {
          forEach: {
            path: '$.items',
            where: { gt: ['$.n', '$$.threshold'] },
            template: { n: '$.n' },
          },
        },
      }),
    ).toEqual([{ n: 2 }]);
  });

  it('match 对 null 值回退空串', () => {
    const d = { items: [{ s: null }] };
    expect(
      applyTransform(d, {
        template: { forEach: { path: '$.items', where: { match: ['$.s', ''] }, template: 1 } },
      }),
    ).toEqual([1]);
  });

  it('contains 对 null 值回退空串比较', () => {
    const d = { items: [{ s: null }, { s: 'ab' }] };
    expect(
      applyTransform(d, {
        template: { forEach: { path: '$.items', where: { contains: ['$.s', ''] }, template: 1 } },
      }),
    ).toEqual([1, 1]);
  });

  it('where 缺失/为空对象/未知算子时不过滤', () => {
    expect(forEach(undefined)).toEqual([{ n: 1 }, { n: 2 }]);
    expect(forEach({})).toEqual([{ n: 1 }, { n: 2 }]);
    expect(forEach({ bogus: ['$.n', 1] })).toEqual([{ n: 1 }, { n: 2 }]);
  });
});

describe('directive 算子补全', () => {
  const o = { v: 6, sec: 3, ms: 1000 };

  it('mul / div（含除零与非法乘数回退）', () => {
    expect(applyTransform(o, { template: { mul: { value: '$.v', by: 10 } } })).toBe(60);
    expect(applyTransform(o, { template: { mul: { value: '$.v', by: 'x' } } })).toBeUndefined();
    expect(applyTransform(o, { template: { div: { value: '$.v', by: 4 } } })).toBe(1.5);
    expect(applyTransform(o, { template: { div: { value: '$.v', by: 0 } } })).toBeUndefined();
  });

  it('add / sub（缺省按 0 处理）', () => {
    expect(applyTransform(o, { template: { add: { a: '$.v', b: 1 } } })).toBe(7);
    expect(applyTransform(o, { template: { add: {} } })).toBe(0);
    expect(applyTransform(o, { template: { sub: { a: '$.v', b: 1 } } })).toBe(5);
    expect(applyTransform(o, { template: { sub: {} } })).toBe(0);
  });

  it('number / msFromSec 对非法输入返回 undefined', () => {
    expect(applyTransform(o, { template: { number: '$.v' } })).toBe(6);
    expect(applyTransform(o, { template: { number: 'nope' } })).toBeUndefined();
    expect(applyTransform(o, { template: { msFromSec: '$.sec' } })).toBe(3000);
    expect(applyTransform(o, { template: { msFromSec: 'nope' } })).toBeUndefined();
  });

  it('toFixed 无值返回 undefined，digits 非法按 0', () => {
    expect(applyTransform(o, { template: { toFixed: { value: 'nope' } } })).toBeUndefined();
    expect(applyTransform(o, { template: { toFixed: { value: '$.v', digits: 'x' } } })).toBe(6);
  });

  it('isoFromMs / isoFromSec 非法输入返回 undefined', () => {
    expect(applyTransform(o, { template: { isoFromMs: 'nope' } })).toBeUndefined();
    expect(applyTransform(o, { template: { isoFromSec: 'nope' } })).toBeUndefined();
    expect(typeof applyTransform(o, { template: { isoFromMs: '$.ms' } })).toBe('string');
  });

  it('pluck 的 value 为普通对象（非指令）时原样返回', () => {
    const out = applyTransform(
      { arr: [{ v: 1 }] },
      { template: { pluck: { path: '$.arr', value: { nested: true } } } },
    );
    expect(out).toEqual([{ nested: true }]);
  });

  it('pluck 的 value 为 $$. 前缀时取 root 值', () => {
    const out = applyTransform(
      { tag: 'T', arr: [{ v: 1 }] },
      { template: { pluck: { path: '$.arr', value: '$$.tag' } } },
    );
    expect(out).toEqual(['T']);
  });

  it('未知单键指令按普通对象处理', () => {
    expect(applyTransform(o, { template: { bogus: 1 } })).toEqual({ bogus: 1 });
  });

  it('多键对象不是 directive，按普通模板映射', () => {
    expect(applyTransform(o, { template: { k: '$.v', j: '$.sec' } })).toEqual({ k: 6, j: 3 });
  });
});

describe('pluck 值指令与 aggregate 边界', () => {
  it('pluck 的 value 支持 directive', () => {
    const out = applyTransform(
      { arr: [{ v: '1' }, { v: '2' }] },
      { template: { pluck: { path: '$.arr', value: { number: '$.v' } } } },
    );
    expect(out).toEqual([1, 2]);
  });

  it('sum/avg 过滤 NaN，avg 空数组返回 0', () => {
    const base = { arr: [{ v: '1' }, { v: 'x' }] };
    expect(applyTransform(base, { template: { sum: { path: '$.arr', value: '$.v' } } })).toBe(1);
    expect(
      applyTransform({ arr: [] }, { template: { avg: { path: '$.arr', value: '$.v' } } }),
    ).toBe(0);
    expect(
      applyTransform(
        { arr: [{ v: 'x' }] },
        { template: { avg: { path: '$.arr', value: '$.v' } } },
      ),
    ).toBe(0);
  });

  it('path 缺失时（undefined 走 || [] 兜底）返回空结果', () => {
    expect(
      applyTransform({ n: 1 }, { template: { forEach: { path: '$.missing', template: '$' } } }),
    ).toEqual([]);
    expect(
      applyTransform({ n: 1 }, { template: { map: { path: '$.missing', template: '$' } } }),
    ).toEqual([]);
    expect(
      applyTransform({ n: 1 }, { template: { pluck: { path: '$.missing', value: '$' } } }),
    ).toEqual([]);
    expect(applyTransform({ n: 1 }, { template: { sum: { path: '$.missing', value: '$' } } })).toBe(
      0,
    );
  });

  it('path 非数组时聚合返回 0，forEach/map/pluck 返回 []', () => {
    expect(applyTransform({ n: 1 }, { template: { sum: { path: '$.n', value: '$' } } })).toBe(0);
    expect(applyTransform({ n: 1 }, { template: { forEach: { path: '$.n', template: '$' } } })).toEqual([]);
    expect(applyTransform({ n: 1 }, { template: { map: { path: '$.n', template: '$' } } })).toEqual([]);
    expect(applyTransform({ n: 1 }, { template: { pluck: { path: '$.n', value: '$' } } })).toEqual([]);
  });
});
