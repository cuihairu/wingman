"""
屏幕操作示例
"""

from wingman import WingmanClient
from PIL import Image
import io

with WingmanClient() as client:
    # 获取屏幕尺寸
    size = client.screen.get_size()
    w, h = size['width'], size['height']
    print(f"屏幕: {w}x{h}")

    # 截取全屏
    print("正在截图...")
    png_data = client.screen.capture()
    image = Image.open(io.BytesIO(png_data))
    image.save('screenshot.png')
    print("截图已保存到 screenshot.png")

    # 截取指定区域
    region_data = client.screen.capture(0, 0, 500, 500)
    region_image = Image.open(io.BytesIO(region_data))
    region_image.save('screenshot_region.png')
    print("区域截图已保存到 screenshot_region.png")

    # 获取像素颜色
    pixel = client.screen.get_pixel(100, 100)
    print(f"像素 (100, 100) 颜色: RGB({pixel['r']}, {pixel['g']}, {pixel['b']})")

    # 查找颜色 (红色: 0xFF0000)
    found = client.screen.find_color(0xFF0000, 0, 0, w, h, tolerance=10)
    if found:
        print(f"找到红色位置: ({found['x']}, {found['y']})")
    else:
        print("未找到红色")

    # 查找多个颜色点
    points = client.screen.find_colors(0xFF0000, 0, 0, w, h, tolerance=10, count=10)
    print(f"找到 {len(points)} 个红色点")
    for i, p in enumerate(points[:5]):
        print(f"  [{i+1}] ({p['x']}, {p['y']})")

    # 查找图像
    try:
        found = client.screen.find_image('target.png', 0, 0, w, h, threshold=0.9)
        if found:
            print(f"找到图像位置: ({found['x']}, {found['y']})")
        else:
            print("未找到图像")
    except FileNotFoundError:
        print("请准备 target.png 文件进行图像匹配测试")
