"""
输入模拟示例
"""

import time
from wingman import WingmanClient

with WingmanClient() as client:
    # 获取当前鼠标位置
    pos = client.input.get_mouse_position()
    print(f"当前鼠标位置: ({pos['x']}, {pos['y']})")

    # 保存原位置
    original_x, original_y = pos['x'], pos['y']

    # 平滑移动鼠标到 (500, 300)
    print("移动鼠标...")
    client.input.move(500, 300, duration=500)
    time.sleep(0.6)

    # 点击左键
    print("点击...")
    client.input.click(500, 300, button=0)
    time.sleep(0.3)

    # 检测按键状态
    is_shift_down = client.input.is_key_down(16)  # VK_SHIFT
    print(f"Shift 键状态: {'按下' if is_shift_down else '释放'}")

    # 随机延迟 (人性化)
    print("随机延迟 100-200ms...")
    client.input.random_delay(100, 200)

    # 输入文本
    print("输入文本...")
    client.input.type_text("Hello from Wingman Python Client!", delay=50)
    time.sleep(1)

    # 按 Enter 键
    print("按 Enter 键...")
    client.input.key_press(13)  # VK_RETURN
    time.sleep(0.5)

    # 移回原位置
    print("移回原位置...")
    client.input.move(original_x, original_y, duration=300)

    print("\n输入示例完成！")
    print("提示: 请确保在文本编辑器中运行此示例")
