"""
Wingman Python Client
用于远程控制 Wingman 自动化引擎
"""

import socket
import json
import struct
import threading
import time
from typing import Any, Optional, Callable, Dict, List
from dataclasses import dataclass
from enum import Enum


class MessageType(Enum):
    """消息类型枚举"""
    REQUEST = 1
    RESPONSE = 2
    EVENT = 3
    HEARTBEAT = 4


@dataclass
class Response:
    """响应数据结构"""
    success: bool
    data: Any = None
    error: Optional[str] = None
    request_id: Optional[int] = None


class WingmanError(Exception):
    """Wingman 错误基类"""
    pass


class ConnectionError(WingmanError):
    """连接错误"""
    pass


class RequestTimeout(WingmanError):
    """请求超时"""
    pass


class WingmanClient:
    """
    Wingman Python 客户端

    示例:
        client = WingmanClient(host='localhost', port=8080)
        client.connect()

        # 获取屏幕尺寸
        screen = client.screen.get_size()
        print(f"屏幕: {screen['width']}x{screen['height']}")

        # 截图
        screenshot = client.screen.capture()

        client.disconnect()
    """

    def __init__(self, host: str = 'localhost', port: int = 8080,
                 timeout: int = 30, auto_reconnect: bool = True):
        """
        初始化客户端

        Args:
            host: 服务器地址
            port: 服务器端口
            timeout: 请求超时时间（秒）
            auto_reconnect: 是否自动重连
        """
        self.host = host
        self.port = port
        self.timeout = timeout
        self.auto_reconnect = auto_reconnect

        self._socket: Optional[socket.socket] = None
        self._connected = False
        self._request_id = 0
        self._pending_requests: Dict[int, threading.Event] = {}
        self._responses: Dict[int, Response] = {}
        self._event_handlers: Dict[str, List[Callable]] = {}
        self._lock = threading.Lock()
        self._recv_thread: Optional[threading.Thread] = None
        self._heartbeat_thread: Optional[threading.Thread] = None

        # 模块访问器
        self.screen = ScreenModule(self)
        self.input = InputModule(self)
        self.window = WindowModule(self)
        self.process = ProcessModule(self)
        self.system = SystemModule(self)
        self.http = HttpModule(self)
        self.json = JsonModule(self)
        self.kv = KvModule(self)

    def connect(self) -> bool:
        """
        连接到服务器

        Returns:
            连接是否成功
        """
        try:
            self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self._socket.connect((self.host, self.port))
            self._connected = True

            # 启动接收线程
            self._recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
            self._recv_thread.start()

            # 启动心跳线程
            self._heartbeat_thread = threading.Thread(target=self._heartbeat_loop, daemon=True)
            self._heartbeat_thread.start()

            return True
        except socket.error as e:
            raise ConnectionError(f"连接失败: {e}")

    def disconnect(self) -> None:
        """断开连接"""
        self._connected = False
        if self._socket:
            try:
                self._socket.close()
            except:
                pass
            self._socket = None

    def is_connected(self) -> bool:
        """检查是否已连接"""
        return self._connected

    def _send_request(self, method: str, params: Dict = None) -> Response:
        """
        发送请求到服务器

        Args:
            method: API 方法名
            params: 参数字典

        Returns:
            响应对象
        """
        if not self._connected:
            if self.auto_reconnect:
                self.connect()
            else:
                raise ConnectionError("未连接到服务器")

        with self._lock:
            self._request_id += 1
            request_id = self._request_id

        request = {
            'id': request_id,
            'method': method,
            'params': params or {}
        }

        # 准备事件和响应
        event = threading.Event()
        self._pending_requests[request_id] = event

        try:
            # 发送请求
            self._send_message(request)

            # 等待响应
            if not event.wait(timeout=self.timeout):
                del self._pending_requests[request_id]
                raise RequestTimeout(f"请求超时: {method}")

            # 获取响应
            response = self._responses.pop(request_id)
            return response

        except Exception as e:
            if request_id in self._pending_requests:
                del self._pending_requests[request_id]
            raise WingmanError(f"请求失败: {e}")

    def _send_message(self, message: Dict) -> None:
        """发送消息"""
        data = json.dumps(message).encode('utf-8')
        # 发送长度头 (4 字节大端序)
        self._socket.sendall(struct.pack('>I', len(data)))
        # 发送数据
        self._socket.sendall(data)

    def _recv_loop(self) -> None:
        """接收消息循环"""
        while self._connected:
            try:
                # 接收长度头
                header = self._recv_exact(4)
                if not header:
                    break

                length = struct.unpack('>I', header)[0]

                # 接收数据
                data = self._recv_exact(length)
                if not data:
                    break

                # 解析消息
                message = json.loads(data.decode('utf-8'))
                self._handle_message(message)

            except Exception as e:
                if self._connected:
                    self._emit_event('error', {'error': str(e)})
                break

        self._connected = False

    def _recv_exact(self, n: int) -> Optional[bytes]:
        """接收精确 n 字节"""
        if not self._socket:
            return None

        data = b''
        while len(data) < n:
            chunk = self._socket.recv(n - len(data))
            if not chunk:
                return None
            data += chunk
        return data

    def _handle_message(self, message: Dict) -> None:
        """处理收到的消息"""
        if 'id' in message:
            # 响应消息
            request_id = message['id']
            if request_id in self._pending_requests:
                response = Response(
                    success=message.get('success', False),
                    data=message.get('data'),
                    error=message.get('error'),
                    request_id=request_id
                )
                self._responses[request_id] = response
                self._pending_requests[request_id].set()

        elif 'event' in message:
            # 事件消息
            self._emit_event(message['event'], message.get('data', {}))

    def _heartbeat_loop(self) -> None:
        """心跳循环"""
        while self._connected:
            try:
                time.sleep(30)
                if self._connected:
                    self._send_message({'type': 'heartbeat'})
            except:
                break

    def on(self, event: str, handler: Callable) -> None:
        """注册事件处理器"""
        if event not in self._event_handlers:
            self._event_handlers[event] = []
        self._event_handlers[event].append(handler)

    def off(self, event: str, handler: Callable = None) -> None:
        """取消事件处理器"""
        if event in self._event_handlers:
            if handler:
                self._event_handlers[event].remove(handler)
            else:
                self._event_handlers[event] = []

    def _emit_event(self, event: str, data: Dict) -> None:
        """触发事件"""
        if event in self._event_handlers:
            for handler in self._event_handlers[event]:
                try:
                    handler(data)
                except:
                    pass

    def __enter__(self):
        """上下文管理器入口"""
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """上下文管理器退出"""
        self.disconnect()


class Module:
    """API 模块基类"""

    def __init__(self, client: WingmanClient):
        self._client = client

    def _call(self, method: str, **params) -> Any:
        """调用 API"""
        response = self._client._send_request(method, params)
        if not response.success:
            raise WingmanError(response.error or "请求失败")
        return response.data


class ScreenModule(Module):
    """屏幕操作模块"""

    def get_size(self) -> Dict[str, int]:
        """获取屏幕尺寸"""
        return self._call('screen.get_size')

    def capture(self, x: int = 0, y: int = 0, width: int = 0, height: int = 0) -> bytes:
        """
        截取屏幕

        Args:
            x: 起始 X 坐标
            y: 起始 Y 坐标
            width: 宽度 (0 表示全屏)
            height: 高度 (0 表示全屏)

        Returns:
            PNG 格式图片数据
        """
        result = self._call('screen.capture', x=x, y=y, width=width, height=height)
        # 返回 base64 解码后的数据
        import base64
        return base64.b64decode(result.get('data', ''))

    def get_pixel(self, x: int, y: int) -> Dict[str, int]:
        """获取像素颜色"""
        return self._call('screen.get_pixel', x=x, y=y)

    def find_color(self, color: int, x1: int, y1: int, x2: int, y2: int,
                   tolerance: int = 0) -> Optional[Dict[str, int]]:
        """查找颜色"""
        return self._call('screen.find_color',
                         color=color, x1=x1, y1=y1, x2=x2, y2=y2, tolerance=tolerance)

    def find_colors(self, color: int, x1: int, y1: int, x2: int, y2: int,
                    tolerance: int = 0, count: int = 100) -> List[Dict[str, int]]:
        """查找多个颜色点"""
        return self._call('screen.find_colors',
                         color=color, x1=x1, y1=y1, x2=x2, y2=y2,
                         tolerance=tolerance, count=count) or []

    def find_image(self, image_path: str, x1: int, y1: int, x2: int, y2: int,
                   threshold: float = 0.9) -> Optional[Dict[str, int]]:
        """查找图像"""
        with open(image_path, 'rb') as f:
            image_data = f.read()
        import base64
        return self._call('screen.find_image',
                         image=base64.b64encode(image_data).decode(),
                         x1=x1, y1=y1, x2=x2, y2=y2, threshold=threshold)


class InputModule(Module):
    """输入模拟模块"""

    def get_mouse_position(self) -> Dict[str, int]:
        """获取鼠标位置"""
        return self._call('input.get_mouse_position')

    def click(self, x: int, y: int, button: int = 0) -> None:
        """
        点击鼠标

        Args:
            x: X 坐标
            y: Y 坐标
            button: 按钮类型 (0=左键, 1=中键, 2=右键)
        """
        self._call('input.click', x=x, y=y, button=button)

    def move(self, x: int, y: int, duration: int = 0) -> None:
        """
        移动鼠标

        Args:
            x: 目标 X 坐标
            y: 目标 Y 坐标
            duration: 移动时长（毫秒）
        """
        self._call('input.move', x=x, y=y, duration=duration)

    def scroll(self, x: int, y: int, delta: int) -> None:
        """滚动滚轮"""
        self._call('input.scroll', x=x, y=y, delta=delta)

    def key_down(self, key: int) -> None:
        """按键按下"""
        self._call('input.key_down', key=key)

    def key_up(self, key: int) -> None:
        """按键释放"""
        self._call('input.key_up', key=key)

    def key_press(self, key: int) -> None:
        """按键（按下后释放）"""
        self._call('input.key_press', key=key)

    def type_text(self, text: str, delay: int = 0) -> None:
        """输入文本"""
        self._call('input.type', text=text, delay=delay)

    def is_key_down(self, key: int) -> bool:
        """检测按键状态"""
        return self._call('input.is_key_down', key=key)

    def random_delay(self, min_ms: int, max_ms: int) -> None:
        """随机延迟"""
        self._call('input.random_delay', min=min_ms, max=max_ms)


class WindowModule(Module):
    """窗口管理模块"""

    def get_foreground(self) -> Optional[int]:
        """获取前台窗口句柄"""
        return self._call('window.get_foreground')

    def find(self, title: str) -> Optional[int]:
        """查找窗口"""
        return self._call('window.find', title=title)

    def activate(self, hwnd: int) -> bool:
        """激活窗口"""
        return self._call('window.activate', hwnd=hwnd)

    def get_title(self, hwnd: int) -> Optional[str]:
        """获取窗口标题"""
        return self._call('window.get_title', hwnd=hwnd)

    def get_bounds(self, hwnd: int) -> Optional[Dict[str, int]]:
        """获取窗口位置"""
        return self._call('window.get_bounds', hwnd=hwnd)

    def set_bounds(self, hwnd: int, x: int, y: int, width: int, height: int) -> bool:
        """设置窗口位置"""
        return self._call('window.set_bounds', hwnd=hwnd, x=x, y=y, width=width, height=height)

    def wait_for(self, title: str, timeout: int = 10000) -> Optional[int]:
        """等待窗口出现"""
        return self._call('window.wait_for', title=title, timeout=timeout)


class ProcessModule(Module):
    """进程管理模块"""

    def find(self, name: str) -> Optional[int]:
        """查找进程"""
        return self._call('process.find', name=name)

    def start(self, path: str, args: str = '', working_dir: str = '') -> Optional[int]:
        """启动进程"""
        return self._call('process.start', path=path, args=args, working_dir=working_dir)

    def wait(self, pid: int, timeout: int = 30000) -> bool:
        """等待进程退出"""
        return self._call('process.wait', pid=pid, timeout=timeout)

    def terminate(self, pid: int, force: bool = False) -> bool:
        """终止进程"""
        return self._call('process.terminate', pid=pid, force=force)

    def exists(self, pid: int) -> bool:
        """检查进程是否存在"""
        return self._call('process.exists', pid=pid)

    def get_path(self, pid: int) -> Optional[str]:
        """获取进程路径"""
        return self._call('process.get_path', pid=pid)


class SystemModule(Module):
    """系统信息模块"""

    def get_cpu_info(self) -> Dict:
        """获取 CPU 信息"""
        return self._call('system.get_cpu_info')

    def get_memory_info(self) -> Dict:
        """获取内存信息"""
        return self._call('system.get_memory_info')

    def get_disk_info(self, path: str = '') -> Dict:
        """获取磁盘信息"""
        return self._call('system.get_disk_info', path=path)

    def get_gpu_info(self) -> List[Dict]:
        """获取 GPU 信息"""
        return self._call('system.get_gpu_info') or []

    def get_os_info(self) -> Dict:
        """获取系统信息"""
        return self._call('system.get_os_info')

    def get_network_adapters(self) -> List[Dict]:
        """获取网络适配器"""
        return self._call('system.get_network_adapters') or []

    def get_display_info(self) -> List[Dict]:
        """获取显示器信息"""
        return self._call('system.get_display_info') or []

    def get_uptime(self) -> int:
        """获取运行时间（秒）"""
        return self._call('system.get_uptime')

    def get_date_time(self) -> Dict:
        """获取日期时间"""
        return self._call('system.get_date_time')

    def get_process_count(self) -> int:
        """获取进程数量"""
        return self._call('system.get_process_count')

    def get_thread_count(self) -> int:
        """获取线程数量"""
        return self._call('system.get_thread_count')


class HttpModule(Module):
    """HTTP 模块"""

    def get(self, url: str, headers: Dict = None) -> Dict:
        """HTTP GET 请求"""
        return self._call('http.get', url=url, headers=headers or {})

    def post(self, url: str, body: str, headers: Dict = None) -> Dict:
        """HTTP POST 请求"""
        return self._call('http.post', url=url, body=body, headers=headers or {})

    def put(self, url: str, body: str, headers: Dict = None) -> Dict:
        """HTTP PUT 请求"""
        return self._call('http.put', url=url, body=body, headers=headers or {})

    def delete(self, url: str, headers: Dict = None) -> Dict:
        """HTTP DELETE 请求"""
        return self._call('http.delete', url=url, headers=headers or {})


class JsonModule(Module):
    """JSON 模块"""

    def encode(self, obj: Any) -> str:
        """序列化 JSON"""
        import json
        return self._call('json.encode', data=json.dumps(obj))

    def decode(self, data: str) -> Any:
        """解析 JSON"""
        result = self._call('json.decode', data=data)
        import json
        return json.loads(result)


class KvModule(Module):
    """KV 存储模块"""

    def set(self, key: str, value: str) -> None:
        """设置键值"""
        self._call('kv.set', key=key, value=value)

    def get(self, key: str) -> Optional[str]:
        """获取键值"""
        return self._call('kv.get', key=key)

    def delete(self, *keys: str) -> None:
        """删除键"""
        self._call('kv.delete', keys=list(keys))

    def exists(self, key: str) -> bool:
        """检查键是否存在"""
        return self._call('kv.exists', key=key)

    def incr(self, key: int, by: int = 1) -> int:
        """自增"""
        return self._call('kv.incr', key=key, by=by)

    def decr(self, key: str, by: int = 1) -> int:
        """自减"""
        return self._call('kv.decr', key=key, by=by)

    def hset(self, hash: str, field: str, value: str) -> None:
        """设置哈希字段"""
        self._call('kv.hset', hash=hash, field=field, value=value)

    def hget(self, hash: str, field: str) -> Optional[str]:
        """获取哈希字段"""
        return self._call('kv.hget', hash=hash, field=field)

    def hgetall(self, hash: str) -> Dict[str, str]:
        """获取所有哈希字段"""
        return self._call('kv.hgetall', hash=hash) or {}

    def hdel(self, hash: str, *fields: str) -> None:
        """删除哈希字段"""
        self._call('kv.hdel', hash=hash, fields=list(fields))

    def lpush(self, list: str, *values: str) -> int:
        """左推入列表"""
        return self._call('kv.lpush', list=list, values=list(values))

    def lpop(self, list: str) -> Optional[str]:
        """左弹出列表"""
        return self._call('kv.lpop', list=list)

    def rpush(self, list: str, *values: str) -> int:
        """右推入列表"""
        return self._call('kv.rpush', list=list, values=list(values))

    def rpop(self, list: str) -> Optional[str]:
        """右弹出列表"""
        return self._call('kv.rpop', list=list)

    def llen(self, list: str) -> int:
        """获取列表长度"""
        return self._call('kv.llen', list=list)
