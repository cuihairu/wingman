"""
窗口和进程管理示例
"""

import time
from wingman import WingmanClient

with WingmanClient() as client:
    # ========== 窗口操作 ==========
    print("=== 窗口操作 ===")

    # 获取前台窗口
    hwnd = client.window.get_foreground()
    if hwnd:
        title = client.window.get_title(hwnd)
        bounds = client.window.get_bounds(hwnd)
        print(f"前台窗口: {title}")
        print(f"位置: ({bounds['x']}, {bounds['y']})")
        print(f"大小: {bounds['width']}x{bounds['height']}")

    # 查找记事本窗口
    notepad_hwnd = client.window.find("Notepad")
    if not notepad_hwnd:
        print("记事本未运行，尝试启动...")
        pid = client.process.start("notepad.exe")
        print(f"记事本已启动 (PID: {pid})")
        time.sleep(1)

        # 等待窗口出现
        notepad_hwnd = client.window.wait_for("Notepad", timeout=5000)

    if notepad_hwnd:
        print(f"找到记事本窗口 (HWND: {notepad_hwnd})")

        # 激活窗口
        client.window.activate(notepad_hwnd)
        print("已激活记事本")
        time.sleep(0.5)

        # 获取当前位置
        bounds = client.window.get_bounds(notepad_hwnd)
        print(f"当前位置: ({bounds['x']}, {bounds['y']})")

        # 移动窗口
        client.window.set_bounds(notepad_hwnd, 100, 100, 600, 400)
        print("已移动窗口到 (100, 100)")
        time.sleep(1)

        # 恢复原位置
        client.window.set_bounds(notepad_hwnd,
                                  bounds['x'], bounds['y'],
                                  bounds['width'], bounds['height'])

    # ========== 进程操作 ==========
    print("\n=== 进程操作 ===")

    # 查找 Chrome 进程
    chrome_pid = client.process.find("chrome.exe")
    if chrome_pid:
        print(f"Chrome 正在运行 (PID: {chrome_pid})")
        path = client.process.get_path(chrome_pid)
        print(f"路径: {path}")
    else:
        print("Chrome 未运行")

    # 检查记事本进程
    notepad_pid = client.process.find("notepad.exe")
    if notepad_pid:
        print(f"记事本进程 (PID: {notepad_pid})")

        # 检查进程是否存在
        exists = client.process.exists(notepad_pid)
        print(f"进程存在: {exists}")

print("\n窗口和进程示例完成！")
