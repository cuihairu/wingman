/**
 * 事件处理和自动重连示例
 */

import { WingmanClient } from '@wingman/client';

async function main() {
  const client = new WingmanClient({
    autoReconnect: true,
    reconnectInterval: 5000
  });

  // 监听事件
  client.on('connected', () => {
    console.log('✓ 已连接到服务器');
  });

  client.on('disconnected', () => {
    console.log('✗ 已断开连接');
  });

  client.on('error', (err) => {
    console.error(`错误: ${err.message}`);
  });

  client.on('reconnecting', () => {
    console.log('↻ 正在重连...');
  });

  await client.connect();

  // 执行操作
  const size = await client.screen.getSize();
  console.log(`屏幕: ${size.width}x${size.height}`);

  // 模拟连接断开...
  console.log('5 秒后断开连接（测试自动重连）');
  setTimeout(() => {
    client['socket']?.destroy(); // 模拟断开
  }, 5000);

  // 保持运行以观察重连
  await new Promise(resolve => setTimeout(resolve, 20000));

  await client.disconnect();
}

main().catch(console.error);
