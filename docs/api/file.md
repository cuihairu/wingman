# API: wingman.file

文件 IO 模块，基于 `std::filesystem` 提供常用文件与目录操作，二进制安全、路径支持非 ASCII 字符。

## 模块概述

file 模块提供以下能力：

- **读写文件** - 整文件读入 / 截断写 / 追加写（二进制安全）
- **路径查询** - 存在性、文件/目录判断、大小
- **文件操作** - 移动、复制、删除、递归删除
- **目录操作** - 递归创建、列目录

**注意**：所有系统调用异常都转为 `nil`/`false` 返回值，不向脚本引擎抛出；`write` 不自动创建父目录，需先 `mkdir`。

---

## 读文件

### read(path) / read(path)

**说明**：整文件读入字符串（二进制安全）。文件不存在或读取失败返回 `nil`。

**函数签名**：

```python
read(path: str) -> str | None
```

```lua
read(path: string) -> string | nil
```

**参数**：
- `path` - 文件路径

**返回**：
- `str`/`string` - 文件内容；失败返回 `None`/`nil`

:::tabs

== Python

```python:line-numbers
from wingman import file

content = file.read("config.json")
if content is not None:
    print(content)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local content = wingman.file.read("config.json")
if content ~= nil then
    print(content)
end
```

:::

---

## 写文件

### write(path, content) / write(path, content)

**说明**：截断写（覆盖已有内容）。不自动创建父目录。

**函数签名**：

```python
write(path: str, content: str) -> bool
```

```lua
write(path: string, content: string) -> boolean
```

**参数**：
- `path` - 目标文件路径
- `content` - 写入内容（二进制安全）

**返回**：
- `bool`/`boolean` - 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import file

ok = file.write("out/result.txt", "hello\n")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local ok = wingman.file.write("out/result.txt", "hello\n")
```

:::

---

### append(path, content) / append(path, content)

**说明**：追加写到文件末尾（文件不存在则创建）。

**函数签名**：

```python
append(path: str, content: str) -> bool
```

```lua
append(path: string, content: string) -> boolean
```

**参数**：
- `path` - 目标文件路径
- `content` - 追加内容

**返回**：
- `bool`/`boolean` - 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import file

file.append("log.txt", "line\n")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.file.append("log.txt", "line\n")
```

:::

---

## 路径查询

### exists(path) / exists(path)

**说明**：路径是否存在（文件或目录）。

**函数签名**：

```python
exists(path: str) -> bool
```

```lua
exists(path: string) -> boolean
```

:::tabs

== Python

```python:line-numbers
from wingman import file

if file.exists("data/kv.db"):
    print("exists")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

if wingman.file.exists("data/kv.db") then
    print("exists")
end
```

:::

---

### isFile(path) / isFile(path)

**说明**：路径是否为常规文件。

**函数签名**：

```python
isFile(path: str) -> bool
```

```lua
isFile(path: string) -> boolean
```

---

### isDir(path) / isDir(path)

**说明**：路径是否为目录。

**函数签名**：

```python
isDir(path: str) -> bool
```

```lua
isDir(path: string) -> boolean
```

---

### size(path) / size(path)

**说明**：文件大小（字节）。路径不存在或不可获取时返回 `nil`；`0` 是合法大小。

**函数签名**：

```python
size(path: str) -> int | None
```

```lua
size(path: string) -> number | nil
```

:::tabs

== Python

```python:line-numbers
from wingman import file

sz = file.size("out/result.txt")
if sz is not None:
    print(f"{sz} bytes")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local sz = wingman.file.size("out/result.txt")
if sz ~= nil then
    print(sz)
end
```

:::

---

## 文件操作

### move(src, dst) / move(src, dst)

**说明**：移动/重命名。rename 优先，跨文件系统时回退为 copy+remove。

**函数签名**：

```python
move(src: str, dst: str) -> bool
```

```lua
move(src: string, dst: string) -> boolean
```

**参数**：
- `src` - 源路径
- `dst` - 目标路径

**返回**：
- `bool`/`boolean` - 是否成功

---

### copy(src, dst) / copy(src, dst)

**说明**：复制文件或目录（目录递归复制；目标已存在则覆盖）。

**函数签名**：

```python
copy(src: str, dst: str) -> bool
```

```lua
copy(src: string, dst: string) -> boolean
```

---

### remove(path) / remove(path)

**说明**：删除单个文件或空目录。递归删除用 `removeAll`。

**函数签名**：

```python
remove(path: str) -> bool
```

```lua
remove(path: string) -> boolean
```

---

### removeAll(path) / removeAll(path)

**说明**：递归删除文件或目录树。路径不存在时也返回 `true`（幂等）。

**函数签名**：

```python
removeAll(path: str) -> bool
```

```lua
removeAll(path: string) -> boolean
```

:::tabs

== Python

```python:line-numbers
from wingman import file

file.remove("out/result.txt")
file.removeAll("out/tmp")   # 目录树递归删
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.file.remove("out/result.txt")
wingman.file.removeAll("out/tmp")   -- 目录树递归删
```

:::

---

## 目录操作

### mkdir(path) / mkdir(path)

**说明**：递归创建目录（含不存在的父级）；目录已存在时返回 `true`。

**函数签名**：

```python
mkdir(path: str) -> bool
```

```lua
mkdir(path: string) -> boolean
```

---

### listDir(path) / listDir(path)

**说明**：列出目录下的条目名（不含 `.` `..`，按名称排序）。仅条目名，不含路径。非目录或读取失败返回 `nil`。

**函数签名**：

```python
listDir(path: str) -> list[str] | None
```

```lua
listDir(path: string) -> string[] | nil
```

:::tabs

== Python

```python:line-numbers
from wingman import file

names = file.listDir("out")
if names is not None:
    for name in names:
        print(name)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local names = wingman.file.listDir("out")
if names ~= nil then
    for _, name in ipairs(names) do
        print(name)
    end
end
```

:::

---

## 完整示例

### Python

```python
from wingman import file

# 建目录并写文件
file.mkdir("out")
file.write("out/result.txt", "hello")

# 读回并追加
content = file.read("out/result.txt")
file.append("out/result.txt", "world")

# 目录遍历
for name in file.listDir("out") or []:
    print(name, file.size(f"out/{name}"))

# 清理
file.removeAll("out")
```

### Lua

```lua
local wingman = require("wingman")

-- 建目录并写文件
wingman.file.mkdir("out")
wingman.file.write("out/result.txt", "hello")

-- 读回并追加
local content = wingman.file.read("out/result.txt")
wingman.file.append("out/result.txt", "world")

-- 目录遍历
local names = wingman.file.listDir("out")
if names ~= nil then
    for _, name in ipairs(names) do
        print(name)
    end
end

-- 清理
wingman.file.removeAll("out")
```

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `read(path)` | `read(path)` | 整文件读入（二进制安全） | path: 文件路径 |
| `write(path, content)` | `write(path, content)` | 截断写 | path: 路径<br>content: 内容 |
| `append(path, content)` | `append(path, content)` | 追加写 | path: 路径<br>content: 内容 |
| `exists(path)` | `exists(path)` | 是否存在 | path: 路径 |
| `isFile(path)` | `isFile(path)` | 是否为常规文件 | path: 路径 |
| `isDir(path)` | `isDir(path)` | 是否为目录 | path: 路径 |
| `size(path)` | `size(path)` | 文件大小（字节） | path: 路径 |
| `move(src, dst)` | `move(src, dst)` | 移动/重命名 | src: 源路径<br>dst: 目标路径 |
| `copy(src, dst)` | `copy(src, dst)` | 复制（目录递归） | src: 源路径<br>dst: 目标路径 |
| `remove(path)` | `remove(path)` | 删除文件或空目录 | path: 路径 |
| `removeAll(path)` | `removeAll(path)` | 递归删除（幂等） | path: 路径 |
| `mkdir(path)` | `mkdir(path)` | 递归创建目录 | path: 路径 |
| `listDir(path)` | `listDir(path)` | 列目录条目名（排序） | path: 目录路径 |
