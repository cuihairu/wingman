/**
 * 基本使用示例
 */

import { WingmanClient } from '@wingman/client';

async function main() {
  const client = new WingmanClient({
    host: 'localhost',
    port: 8080
  });

  try {
    // 连接
    await client.connect();
    console.log('已连接到服务器');

    // 获取屏幕尺寸
    const size = await client.screen.getSize();
    console.log(`屏幕尺寸: ${size.width}x${size.height}`);

    // 获取鼠标位置
    const pos = await client.input.getMousePosition();
    console.log(`鼠标位置: (${pos.x}, ${pos.y})`);

    // 获取系统信息
    const cpu = await client.system.getCpuInfo();
    console.log(`CPU: ${cpu.brand} (${cpu.cores} 核心)`);

    const mem = await client.system.getMemoryInfo();
    console.log(`内存: ${(mem.total / 1024 / 1024 / 1024).toFixed(2)} GB`);

  } finally {
    await client.disconnect();
    console.log('已断开连接');
  }
}

main().catch(console.error);
