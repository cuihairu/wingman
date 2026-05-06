# API: process

进程管理模块。

## 函数

### find(name)

查找指定名称的进程。

**参数：**
- `name` (string) - 进程名称（不需要 .exe 后缀）

**返回：**
- `number` | `nil` - 进程 ID (PID)，未找到返回 nil
- `boolean` - 是否找到

**示例：**
```lua
local pid, found = process.find("notepad")
if found then
    print("找到记事本进程，PID: " .. pid)
end
```

### start(path, args, workingDir)

启动新进程。

**参数：**
- `path` (string) - 可执行文件路径
- `args` (string, 可选) - 命令行参数
- `workingDir` (string, 可选) - 工作目录

**返回：**
- `number` - 新进程的 PID

**示例：**
```lua
-- 启动记事本
local pid = process.start("notepad.exe")

-- 启动带参数的程序
local pid = process.start("cmd.exe", "/c dir C:\\")

-- 指定工作目录
local pid = process.start("cmd.exe", "", "C:\\Temp")
```

### wait(pid, timeout)

等待进程结束。

**参数：**
- `pid` (number) - 进程 ID
- `timeout` (number, 可选) - 超时时间（毫秒），0 表示无限等待

**返回：**
- `boolean` - 进程是否已结束（超时返回 false）

**示例：**
```lua
local pid = process.start("notepad.exe")

-- 等待进程结束（无限等待）
if process.wait(pid) then
    print("进程已结束")
end

-- 带超时等待
if process.wait(pid, 5000) then
    print("进程在5秒内结束")
else
    print("等待超时")
end
```

### terminate(pid, force)

终止进程。

**参数：**
- `pid` (number) - 进程 ID
- `force` (boolean) - 是否强制终止

**返回：**
- `boolean` - 是否成功

**示例：**
```lua
local pid, found = process.find("notepad")
if found then
    if process.terminate(pid, false) then
        print("进程已终止")
    end
end
```

### exists(pid)

检查进程是否存在。

**参数：**
- `pid` (number) - 进程 ID

**返回：**
- `boolean` - 进程是否存在

**示例：**
```lua
local pid = 1234
if process.exists(pid) then
    print("进程 " .. pid .. " 正在运行")
else
    print("进程 " .. pid .. " 不存在")
end
```

### waitFor(name, timeout)

等待进程出现。

**参数：**
- `name` (string) - 进程名称
- `timeout` (number) - 超时时间（毫秒），默认 5000

**返回：**
- `boolean` - 是否在超时前找到进程

**示例：**
```lua
-- 启动应用程序
process.start("notepad.exe")

-- 等待进程出现
if process.waitFor("notepad", 5000) then
    print("记事本进程已启动")
    local pid, found = process.find("notepad")
    print("PID: " .. pid)
else
    print("超时：进程未启动")
end
```

## 完整示例

```lua
-- 检查记事本是否运行
local pid, found = process.find("notepad")
if not found then
    print("记事本未运行，正在启动...")
    pid = process.start("notepad.exe")

    -- 等待进程启动
    util.sleep(500)
end

-- 获取进程信息
pid, found = process.find("notepad")
if found then
    print("记事本 PID: " .. pid)

    -- 检查进程是否存在
    if process.exists(pid) then
        print("进程正在运行")
    end

    -- 等待5秒后终止
    util.sleep(5000)
    if process.terminate(pid, false) then
        print("进程已终止")
    end
end
```

## 常见使用场景

### 启动并等待程序完成

```lua
-- 启动命令行程序并等待完成
local pid = process.start("cmd.exe", "/c timeout 3")
if process.wait(pid, 5000) then
    print("命令执行完成")
end
```

### 确保只有一个实例

```lua
-- 检查程序是否已运行
local pid, found = process.find("myapp")
if found then
    print("程序已在运行，PID: " .. pid)
else
    print("启动新实例...")
    process.start("myapp.exe")
end
```

### 关闭所有匹配的进程

```lua
-- 注意：需要实现 process.findAll 才能列出所有匹配进程
-- 当前版本只能查找到的第一个进程
local pid, found = process.find("notepad")
while found do
    process.terminate(pid, true)
    util.sleep(100)
    pid, found = process.find("notepad")
end
```

## 注意事项

1. 进程 ID (PID) 是系统分配的数字，每次运行可能不同
2. 终止系统关键进程可能导致系统不稳定
3. 管理员权限的进程需要管理员权限才能操作
4. 使用 `force=false` 优先让进程正常退出，`force=true` 强制杀死
