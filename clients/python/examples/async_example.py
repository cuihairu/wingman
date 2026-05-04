"""
异步示例和事件处理
"""

import threading
import time
from wingman import WingmanClient

def on_error(data):
    """错误事件处理"""
    print(f"[错误] {data.get('error', 'Unknown error')}")

def on_screenshot(data):
    """截图事件处理"""
    print(f"[事件] 收到截图: {data.get('width', 0)}x{data.get('height', 0)}")

def monitor_keys(client, duration=10):
    """监控按键"""
    print(f"监控按键 {duration} 秒...")
    end_time = time.time() + duration

    while time.time() < end_time and client.is_connected():
        # 检测 Shift 键
        is_shift = client.input.is_key_down(16)
        if is_shift:
            print("[检测到] Shift 键被按下")
        time.sleep(0.5)

    print("监控结束")

with WingmanClient() as client:
    # 注册事件处理器
    client.on('error', on_error)
    client.on('screenshot', on_screenshot)

    # 在后台线程监控按键
    monitor_thread = threading.Thread(
        target=monitor_keys,
        args=(client, 5),
        daemon=True
    )
    monitor_thread.start()

    # 主线程执行其他操作
    print("主线程执行操作...")

    for i in range(5):
        print(f"[主线程] 循环 {i+1}/5")
        time.sleep(1)

    # 等待监控线程结束
    monitor_thread.join()

print("\n异步示例完成！")
