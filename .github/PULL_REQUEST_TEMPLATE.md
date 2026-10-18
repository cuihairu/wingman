## 改了什么 / What changed

<对齐 CONTRIBUTING.md「Pull Request」：解决了什么、是否触及架构约束>

## 验证 / Verification

贴命令与实际结果（含通过数）：

```
# 例：docs-only
cd docs && pnpm docs:build

# 例：代码改动（按改动面裁剪）
cmake --build build && ctest --test-dir build -R <suite>
```

- [ ] 测试全绿（失败请贴输出，不要用放宽断言或删用例凑绿）
- [ ] 未 bump 任何 version 字段（版本由维护者给定）
- [ ] 未夹带无关格式化/换行改动

## 文档同步 / Docs

- [ ] 未触及 runtime / 本地 UI / 远程编排（无需改架构文档）
- [ ] 已同次更新 `docs/architecture-decisions.md` 及相关说明文档
- [ ] API 面变更已重跑 `scripts/gen_lua_hints.py` 并提交 `assets/vscode-wingman-lua/wingman.d.lua`

## 关联 / Related

Closes #
