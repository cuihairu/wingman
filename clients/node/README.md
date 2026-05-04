# @wingman/client

用于远程控制 [Wingman](https://github.com/cuihairu/wingman) 自动化引擎的 Node.js/TypeScript 客户端。

## 安装

```bash
npm install @wingman/client
```

## 快速开始

```typescript
import { WingmanClient } from '@wingman/client';

const client = new WingmanClient({
  host: 'localhost',
  port: 8080
});

await client.connect();

// 获取屏幕尺寸
const size = await client.screen.getSize();
console.log(`屏幕: ${size.width}x${size.height}`);

// 截图
const screenshot = await client.screen.capture();
require('fs').writeFileSync('screenshot.png', screenshot);

await client.disconnect();
```

## API 示例

### 屏幕操作

```typescript
// 获取屏幕尺寸
const size = await client.screen.getSize();

// 截图
const full = await client.screen.capture();
const region = await client.screen.capture(0, 0, 500, 500);

// 获取像素颜色
const pixel = await client.screen.getPixel(100, 100);
console.log(`RGB: (${pixel.r}, ${pixel.g}, ${pixel.b})`);

// 查找颜色
const found = await client.screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10);
if (found) {
  console.log(`找到红色在: (${found.x}, ${found.y})`);
}

// 查找图像
const imagePos = await client.screen.findImage('target.png', 0, 0, 1920, 1080, 0.9);
```

### 输入模拟

```typescript
// 获取鼠标位置
const pos = await client.input.getMousePosition();

// 移动鼠标（带平滑动画）
await client.input.move(500, 300, 500);

// 点击
await client.input.click(500, 300, 0); // 0=左键

// 滚动
await client.input.scroll(500, 300, 120);

// 按键
await client.input.keyDown(16);  // Shift
await client.input.keyUp(16);
await client.input.keyPress(13); // Enter

// 输入文本
await client.input.typeText('Hello World!', 50);

// 检测按键
const isShift = await client.input.isKeyDown(16);

// 随机延迟
await client.input.randomDelay(100, 200);
```

### 窗口管理

```typescript
// 获取前台窗口
const hwnd = await client.window.getForeground();
const title = await client.window.getTitle(hwnd);

// 查找窗口
const notepad = await client.window.find('Notepad');

// 激活窗口
await client.window.activate(notepad);

// 获取/设置位置
const bounds = await client.window.getBounds(notepad);
await client.window.setBounds(notepad, 100, 100, 800, 600);

// 等待窗口
const hwnd = await client.window.waitFor('Notepad', 10000);
```

### 进程管理

```typescript
// 查找进程
const pid = await client.process.find('notepad.exe');

// 启动进程
const newPid = await client.process.start('notepad.exe');

// 等待退出
await client.process.wait(pid, 30000);

// 终止进程
await client.process.terminate(pid, false);

// 检查存在
const exists = await client.process.exists(pid);

// 获取路径
const path = await client.process.getPath(pid);
```

### 系统信息

```typescript
// CPU
const cpu = await client.system.getCpuInfo();
console.log(`CPU: ${cpu.brand} (${cpu.cores} 核心)`);

// 内存
const mem = await client.system.getMemoryInfo();
console.log(`内存: ${(mem.total / 1024**3).toFixed(2)} GB`);

// 磁盘
const disk = await client.system.getDiskInfo('C:\\');
console.log(`可用: ${(disk.free / 1024**3).toFixed(2)} GB`);

// GPU
const gpus = await client.system.getGpuInfo();

// 网络
const adapters = await client.system.getNetworkAdapters();

// 显示器
const displays = await client.system.getDisplayInfo();
```

### HTTP 请求

```typescript
// GET
const resp = await client.http.get('https://httpbin.org/get');
console.log(resp.body);

// POST
const postResp = await client.http.post(
  'https://httpbin.org/post',
  JSON.stringify({ key: 'value' }),
  { 'Content-Type': 'application/json' }
);
```

### KV 存储

```typescript
// 字符串
await client.kv.set('myKey', 'myValue');
const value = await client.kv.get('myKey');
await client.kv.delete('myKey');

// 计数
await client.kv.incr('counter');
await client.kv.incr('counter', 5);

// 哈希
await client.kv.hset('user:1', 'name', 'Alice');
const name = await client.kv.hget('user:1', 'name');
const all = await client.kv.hgetall('user:1');

// 列表
await client.kv.lpush('myList', 'item1', 'item2');
const item = await client.kv.lpop('myList');
const len = await client.kv.llen('myList');
```

## 事件处理

```typescript
client.on('connected', () => {
  console.log('已连接');
});

client.on('disconnected', () => {
  console.log('已断开');
});

client.on('error', (err) => {
  console.error('错误:', err);
});

client.on('reconnecting', () => {
  console.log('重连中...');
});
```

## 自动重连

```typescript
const client = new WingmanClient({
  host: 'localhost',
  port: 8080,
  autoReconnect: true,    // 自动重连
  reconnectInterval: 5000 // 重连间隔
});
```

## TypeScript 支持

此包包含完整的 TypeScript 类型定义：

```typescript
import {
  WingmanClient,
  Size,
  Point,
  Color,
  Bounds,
  CpuInfo,
  // ...
} from '@wingman/client';
```

## 许可证

Apache-2.0
