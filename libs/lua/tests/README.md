# Wingman 测试指南

本项目使用 [busted](https://lunarmodules.github.io/busted/) 作为 Lua 测试框架。

## 安装 busted

### Windows

```powershell
# 安装 LuaRocks
# 下载: https://luarocks.github.io/luarocks/releases/

# 安装 busted
luarocks install busted
```

### 运行测试

```powershell
# 运行所有测试
busted tests

# 只运行单元测试
busted tests/unit

# 只运行集成测试
busted tests/integration

# 运行特定文件
busted tests/unit/screen_spec.lua

# 详细输出
busted tests -v
```

## 测试结构

```
tests/
├── .busted           # Busted 配置文件
├── unit/             # 单元测试
│   └── screen_spec.lua
└── integration/      # 集成测试
    └── api_spec.lua
```

## 编写测试

```lua
describe("模块名", function()
    it("应该能完成某个功能", function()
        -- 准备测试数据
        local result = module.doSomething()

        -- 断言
        assert.is_not_nil(result)
        assert.is_equal("expected", result)
    end)

    pending("待实现的功能", function()
        -- 标记为待实现
    end)
end)
```

## 持续集成

busted 测试为本地手动运行（CI 实跑的是 ctest C++ 测试套件，busted 未接入流水线）。

测试覆盖率报告上传至 [Codecov](https://codecov.io/gh/cuihairu/wingman)。
