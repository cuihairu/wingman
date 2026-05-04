"""
Wingman Python Client 基本使用示例
"""

from wingman import WingmanClient

# 使用上下文管理器（推荐）
with WingmanClient(host='localhost', port=8080) as client:
    # 获取屏幕尺寸
    screen = client.screen.get_size()
    print(f"屏幕尺寸: {screen['width']}x{screen['height']}")

    # 获取鼠标位置
    mouse = client.input.get_mouse_position()
    print(f"鼠标位置: ({mouse['x']}, {mouse['y']})")

    # 获取系统信息
    cpu = client.system.get_cpu_info()
    print(f"CPU: {cpu['brand']} ({cpu['cores']} 核心)")

    mem = client.system.get_memory_info()
    print(f"内存: {mem['total'] / 1024 / 1024 / 1024:.2f} GB")

print("\n示例完成！")
