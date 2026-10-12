#!/usr/bin/env python3
"""Generate assets/vscode-wingman-lua/wingman.d.lua from C++ module descriptors.

Single source of truth is the same scan as audit_pyi_drift.py: `mod.name` +
`mod.functions.push_back({"fn", ..., "sig"})` in lib/wingman/src. Signature
strings ("name:string, timeout?:number -> bool") are parsed into EmmyLua
@param/@return annotations; unmapped types fall back to `any`.

Run manually:  python3 scripts/gen_lua_hints.py
CI keeps it in sync: regenerate + git diff --exit-code (see ci.yml
api-typing-audit job) — hand edits to the generated file will fail CI.
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_pyi_drift import scan_cpp_sigs

OUT = Path(__file__).resolve().parent.parent / "assets" / "vscode-wingman-lua" / "wingman.d.lua"

TYPE_MAP = {
    "string": "string",
    "str": "string",
    "int": "integer",
    "integer": "integer",
    "float": "number",
    "number": "number",
    "bool": "boolean",
    "boolean": "boolean",
    "array": "table",
    "table": "table",
    "object": "table",
    "function": "function",
    "callable": "function",
    "callback": "function",
    "nil": "nil",
}

# Lua 保留字：表字段需以 ["name"] 赋值形式生成（如 bt.repeat）
LUA_RESERVED = {
    "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
    "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return",
    "then", "true", "until", "while",
}


def map_type(t: str) -> str:
    t = t.split("=")[0].strip().rstrip("?").strip()
    if not t:
        return "any"
    return TYPE_MAP.get(t, "any")


def split_top_level(s: str) -> list[str]:
    """按顶层逗号切分（花括号/方括号/圆括号内逗号不切）。"""
    parts, depth, cur = [], 0, []
    for c in s:
        if c in "{[(":
            depth += 1
        elif c in "}])":
            depth -= 1
        if c == "," and depth == 0:
            parts.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
    tail = "".join(cur).strip()
    if tail:
        parts.append(tail)
    return parts


def parse_sig(sig: str):
    """sig -> (params:[(name, lua_type, optional)], return type or None)。"""
    sig = sig.strip()
    if sig.startswith("()"):
        args_part, ret = "", sig[2:]
    elif sig.startswith("->"):
        args_part, ret = "", sig[2:]
    elif " -> " in sig:
        args_part, ret = sig.split(" -> ", 1)
    else:
        args_part, ret = sig, ""
    args_part = args_part.strip().strip("()").strip()

    params = []
    if args_part:
        for raw in split_top_level(args_part):
            name, sep, type_ = raw.partition(":")
            name = name.strip()
            optional = name.endswith("?")
            name = name.rstrip("?").strip()
            if not sep and not raw.endswith("?"):
                optional = raw.endswith("?")
            if not name:
                continue
            params.append((name, map_type(type_), optional))

    ret = ret.strip()
    ret_type = None
    if ret and ret not in ("nil", "()"):
        ret_type = map_type(ret)
    return params, ret_type


def lua_safe_name(name: str) -> str:
    """参数名撞 Lua 保留字时加下划线后缀（生成的桩自洽即可）。"""
    if name in LUA_RESERVED or not re.fullmatch(r"[A-Za-z_]\w*", name):
        return name + "_"
    return name


def lua_field(name: str) -> str:
    if name in LUA_RESERVED or not re.fullmatch(r"[A-Za-z_]\w*", name):
        return f'["{name}"]'
    return name


def emit_function(lines: list[str], mod: str, fn: str, sig: str) -> None:
    params, ret_type = parse_sig(sig)
    for name, type_, optional in params:
        opt = "?" if optional else ""
        lines.append(f"---@param {lua_safe_name(name)}{opt} {type_}")
    if ret_type:
        lines.append(f"---@return {ret_type}")
    arg_names = ", ".join(lua_safe_name(n) for n, _, _ in params)
    if fn in LUA_RESERVED or not re.fullmatch(r"[A-Za-z_]\w*", fn):
        lines.append(f'{mod}{lua_field(fn)} = function({arg_names}) end')
    else:
        lines.append(f"function {mod}.{fn}({arg_names}) end")
    lines.append("")


def main() -> int:
    modules = scan_cpp_sigs()
    lines = [
        "--[=[",
        "    Wingman Lua API 类型定义（生成文件，勿手改）",
        "",
        "    由 scripts/gen_lua_hints.py 从 C++ 模块描述符（lib/wingman/src 中",
        "    mod.functions.push_back 注册面）生成，覆盖全部脚本模块与函数签名；",
        "    CI 的 api-typing-audit job 重生成后 git diff --exit-code 保持同步。",
        "",
        "    使用方法：",
        "    1. 将此文件放在你的脚本项目根目录",
        "    2. VSCode 安装 EmmyLua 插件",
        "    3. 享受代码提示！（保留字函数如 bt.repeat 以 [\"repeat\"] 形式补全）",
        "]=]",
        "",
    ]
    for mod in sorted(modules):
        lines.append(f"-- ========== {mod} 模块 ==========")
        lines.append("")
        lines.append(f"---@class {mod}")
        lines.append(f"local {mod} = {{}}")
        lines.append("")
        for fn in sorted(modules[mod]):
            emit_function(lines, mod, fn, modules[mod][fn])
        lines.append("")

    lines.append("-- ========== 导出 ==========")
    lines.append("")
    lines.append("local wingman = {")
    for mod in sorted(modules):
        lines.append(f"    {mod} = {mod},")
    lines.append("}")
    lines.append("")
    lines.append("return wingman")

    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {OUT} ({len(modules)} modules, "
          f"{sum(len(v) for v in modules.values())} functions)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
