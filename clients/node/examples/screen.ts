/**
 * 屏幕操作示例
 */

import { WingmanClient } from '@wingman/client';
import * as fs from 'fs';

async function main() {
  const client = new WingmanClient();

  client.on('connected', () => console.log('已连接'));
  client.on('disconnected', () => console.log('已断开'));

  await client.connect();

  // 获取屏幕尺寸
  const size = await client.screen.getSize();
  console.log(`屏幕: ${size.width}x${size.height}`);

  // 截取全屏
  console.log('正在截图...');
  const screenshot = await client.screen.capture();
  fs.writeFileSync('screenshot.png', screenshot);
  console.log('已保存到 screenshot.png');

  // 截取区域
  const region = await client.screen.capture(0, 0, 500, 500);
  fs.writeFileSync('screenshot_region.png', region);
  console.log('已保存区域截图');

  // 获取像素颜色
  const pixel = await client.screen.getPixel(100, 100);
  console.log(`像素(100,100): RGB(${pixel.r}, ${pixel.g}, ${pixel.b})`);

  // 查找颜色
  const found = await client.screen.findColor(
    0xFF0000,  // 红色
    0, 0, size.width, size.height,
    10  // 容差
  );
  if (found) {
    console.log(`找到红色在: (${found.x}, ${found.y})`);
  } else {
    console.log('未找到红色');
  }

  // 查找多个颜色点
  const points = await client.screen.findColors(
    0xFF0000,
    0, 0, size.width, size.height,
    10, 100
  );
  console.log(`找到 ${points.length} 个红色点`);

  // 查找图像
  try {
    const imagePos = await client.screen.findImage(
      'target.png',
      0, 0, size.width, size.height,
      0.9
    );
    if (imagePos) {
      console.log(`找到图像在: (${imagePos.x}, ${imagePos.y})`);
    }
  } catch (e) {
    console.log('请准备 target.png 进行图像匹配测试');
  }

  await client.disconnect();
}

main().catch(console.error);
