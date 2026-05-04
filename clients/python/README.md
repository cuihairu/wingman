# Wingman Python Client

用于远程控制 [Wingman](https://github.com/cuihairu/wingman) 自动化引擎的 Python 客户端。

## 安装

```bash
pip install wingman-client
```

或从源码安装：

```bash
git clone https://github.com/cuihairu/wingman.git
cd wingman/clients/python
pip install -e .
```

## 快速开始

```python
from wingman import WingmanClient

# 使用上下文管理器（推荐）
with WingmanClient(host='localhost', port=8080) as client:
    # 获取屏幕尺寸
    size = client.screen.get_size()
    print(f"屏幕: {size['width']}x{size['height']}")

    # 截图
    png_data = client.screen.capture()

    # 移动鼠标
    client.input.move(500, 300, duration=500)
```

## 功能模块

### Screen - 屏幕操作

```python
# 获取屏幕尺寸
size = client.screen.get_size()

# 截取屏幕
png_data = client.screen.capture()
png_data = client.screen.capture(0, 0, 500, 500)  # 指定区域

# 获取像素颜色
pixel = client.screen.get_pixel(100, 100)
print(f"RGB: ({pixel['r']}, {pixel['g']}, {pixel['b']})")

# 查找颜色
found = client.screen.find_color(0xFF0000, 0, 0, 1920, 1080, tolerance=10)
if found:
    print(f"找到红色在: ({found['x']}, {found['y']})")

# 查找多个颜色点
points = client.screen.find_colors(0xFF0000, 0, 0, 1920, 1080)

# 查找图像
found = client.screen.find_image('target.png', 0, 0, 1920, 1080, threshold=0.9)
```

### Input - 输入模拟

```python
# 获取鼠标位置
pos = client.input.get_mouse_position()

# 移动鼠标（带平滑动画）
client.input.move(500, 300, duration=500)

# 点击鼠标
client.input.click(500, 300, button=0)  # 0=左键, 1=中键, 2=右键

# 滚动滚轮
client.input.scroll(500, 300, delta=120)

# 按键操作
client.input.key_down(16)  # Shift 按下
client.input.key_up(16)    # Shift 释放
client.input.key_press(13) # Enter 按一下

# 输入文本
client.input.type_text("Hello World!", delay=50)

# 检测按键状态
is_shift = client.input.is_key_down(16)

# 随机延迟（人性化）
client.input.random_delay(100, 200)
```

### Window - 窗口管理

```python
# 获取前台窗口
hwnd = client.window.get_foreground()
title = client.window.get_title(hwnd)

# 查找窗口
hwnd = client.window.find("Notepad")

# 激活窗口
client.window.activate(hwnd)

# 获取窗口位置
bounds = client.window.get_bounds(hwnd)
print(f"位置: ({bounds['x']}, {bounds['y']})")
print(f"大小: {bounds['width']}x{bounds['height']}")

# 设置窗口位置
client.window.set_bounds(hwnd, 100, 100, 800, 600)

# 等待窗口出现
hwnd = client.window.wait_for("Notepad", timeout=10000)
```

### Process - 进程管理

```python
# 查找进程
pid = client.process.find("notepad.exe")

# 启动进程
pid = client.process.start("notepad.exe")
pid = client.process.start("calc.exe", args="", working_dir="C:\\")

# 等待进程退出
client.process.wait(pid, timeout=30000)

# 终止进程
client.process.terminate(pid, force=False)

# 检查进程是否存在
exists = client.process.exists(pid)

# 获取进程路径
path = client.process.get_path(pid)
```

### System - 系统信息

```python
# CPU 信息
cpu = client.system.get_cpu_info()
print(f"CPU: {cpu['brand']} ({cpu['cores']} 核心)")

# 内存信息
mem = client.system.get_memory_info()
print(f"内存: {mem['total'] / 1024**3:.2f} GB")
print(f"可用: {mem['available'] / 1024**3:.2f} GB")

# 磁盘信息
disk = client.system.get_disk_info("C:\\")
print(f"C盘: {disk['free'] / 1024**3:.2f} GB 可用")

# GPU 信息
gpus = client.system.get_gpu_info()
for gpu in gpus:
    print(f"GPU: {gpu['name']}")

# 系统信息
os_info = client.system.get_os_info()
print(f"系统: {os_info['platform']} {os_info['version']}")

# 运行时间
uptime = client.system.get_uptime()
print(f"运行时间: {uptime // 3600} 小时")
```

### HTTP - HTTP 请求

```python
# GET 请求
resp = client.http.get("https://httpbin.org/get")
print(resp['body'])

# POST 请求
resp = client.http.post(
    "https://httpbin.org/post",
    body='{"key": "value"}',
    headers={"Content-Type": "application/json"}
)
```

### KV - 键值存储

```python
# 字符串操作
client.kv.set("my_key", "my_value")
value = client.kv.get("my_key")
client.kv.delete("my_key")

# 自增/自减
client.kv.incr("counter")
client.kv.incr("counter", by=5)
client.kv.decr("counter")

# 哈希操作
client.kv.hset("user:1", "name", "Alice")
client.kv.hset("user:1", "age", "25")
name = client.kv.hget("user:1", "name")
all_data = client.kv.hgetall("user:1")

# 列表操作
client.kv.lpush("my_list", "item1", "item2")
item = client.kv.lpop("my_list")
length = client.kv.llen("my_list")
```

## 事件处理

```python
def on_error(data):
    print(f"错误: {data['error']}")

def on_screenshot(data):
    print(f"收到截图: {data['width']}x{data['height']}")

with WingmanClient() as client:
    # 注册事件处理器
    client.on('error', on_error)
    client.on('screenshot', on_screenshot)

    # 运行你的代码...
```

## 异步操作

```python
import threading

def background_task(client):
    while client.is_connected():
        # 执行后台任务
        pos = client.input.get_mouse_position()
        print(f"鼠标: {pos['x']}, {pos['y']}")
        time.sleep(1)

with WingmanClient() as client:
    # 启动后台线程
    thread = threading.Thread(target=background_task, args=(client,))
    thread.start()

    # 主线程继续执行
    for i in range(10):
        print(f"主循环 {i}")
        time.sleep(0.5)

    thread.join()
```

## 命令行工具

安装后可以使用命令行工具：

```bash
# 获取屏幕尺寸
wingman-client screen size

# 截图
wingman-client screen capture screenshot.png

# 查找颜色
wingman-client screen find-color 0xFF0000

# 移动鼠标
wingman-client input move 500 300
```

## 错误处理

```python
from wingman import WingmanClient, ConnectionError, RequestTimeout, WingmanError

try:
    with WingmanClient(timeout=60) as client:
        result = client.screen.find_color(0xFF0000, 0, 0, 1920, 1080)
except ConnectionError as e:
    print(f"连接失败: {e}")
except RequestTimeout as e:
    print(f"请求超时: {e}")
except WingmanError as e:
    print(f"请求失败: {e}")
```

## 许可证

Apache-2.0
