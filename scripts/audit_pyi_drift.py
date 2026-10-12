#!/usr/bin/env python3
"""Audit drift between C++ script-module descriptors and Python typing stubs.

Scans lib/wingman/src for `mod.name = "x"` + `mod.functions.push_back({"fn"`
pairs, scans libs/python/typing/wingman/*.pyi for top-level `def fn(` stubs,
normalizes both sides camelToSnake (python_script_engine exposes every module
function under both the original camelCase name and a snake_case alias), and
reports:
  - modules registered in C++ but missing a .pyi file (and vice versa)
  - per-module C++ functions absent from the stub (gate failure)
  - stub-only functions (informational: hand-written pythonic aliases)

Exit 0 when no gating drift remains; nonzero otherwise. Runs in the
pre-push guard alongside check_platform_boundary.sh (development-todo
「统一 Lua / Python API 形状与文档」).
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "lib" / "wingman" / "src"
PYI = ROOT / "libs" / "python" / "typing" / "wingman"

NAME_RE = re.compile(r'mod\.name\s*=\s*"(\w+)"')
# 签名串可含 \" 转义引号（如 ml.loadModel 的 ep?:string=\"cpu\"）
REG_RE = re.compile(r'mod\.functions\.push_back\(\{"(\w+)",.*?"((?:\\.|[^"\\])*)"\s*\}\);', re.DOTALL)
DEF_RE = re.compile(r'^def (\w+)\(')


def camel_to_snake(name: str) -> str:
    # 与 python_script_engine.cpp 的 camelToSnake 同口径
    out = []
    for i, c in enumerate(name):
        if c.isupper() and i > 0:
            prev = name[i - 1]
            next_is_lower = i + 1 < len(name) and name[i + 1].islower()
            if prev.islower() or prev.isdigit() or (prev.isupper() and next_is_lower):
                out.append("_")
        out.append(c.lower())
    return "".join(out)


def scan_cpp_sigs():
    """module -> {function: signature string} (union across #if/#else branches).

    Registration has two layouts, both matched by REG_RE over the whole file:
      push_back({"fn", [](args) {...}, "sig"});   (lambda body spans lines)
      push_back({"fn", fnRef, "sig"});            (db 模块等命名函数一行式)
    单文件可定义多个模块（misc_modules.cpp 五个），按源码位置归口到最近
    的 mod.name。
    """
    modules = {}
    for path in sorted(SRC.rglob("*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        marks = []
        for m in NAME_RE.finditer(text):
            marks.append((m.start(), "name", m.group(1)))
        for m in REG_RE.finditer(text):
            marks.append((m.start(), "reg", (m.group(1), m.group(2))))
        marks.sort(key=lambda x: x[0])
        current = None
        for _, kind, payload in marks:
            if kind == "name":
                current = payload
                modules.setdefault(current, {})
            elif current is not None:
                modules[current][payload[0]] = payload[1].replace('\\"', '"')
    return modules


def scan_cpp():
    """module -> set of function names (union across #if/#else branches)."""
    sigs = scan_cpp_sigs()
    return {mod: set(fns) for mod, fns in sigs.items()}


def scan_pyi():
    """module -> set of function names（仅顶层 def，docstring 内缩进例不算）。"""
    modules = {}
    for path in sorted(PYI.glob("*.pyi")):
        name = path.stem
        if name in ("__init__", "types"):
            continue
        fns = set()
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if line[:1].isspace():
                continue
            m = DEF_RE.match(line)
            if m:
                fns.add(m.group(1))
        modules[name] = fns
    return modules


def scan_init():
    """__init__.pyi 导入/导出的模块名集合。"""
    text = (PYI / "__init__.pyi").read_text(encoding="utf-8")
    names = set()
    m = re.search(r"^from \. import (.+)$", text, re.MULTILINE)
    if m:
        names |= {n.strip() for n in m.group(1).split(",") if n.strip()}
    for m in re.finditer(r'^\s{4}"(\w+)",$', text, re.MULTILINE):
        names.add(m.group(1))
    return names


def main():
    cpp = scan_cpp()
    pyi = scan_pyi()
    init_names = scan_init()

    missing_pyi = sorted(set(cpp) - set(pyi))
    orphan_pyi = sorted(set(pyi) - set(cpp))

    failures = 0

    print("== C++ modules without .pyi (gate) ==")
    for m in missing_pyi:
        failures += 1
        print(f"  {m}: {len(cpp[m])} functions ({', '.join(sorted(cpp[m]))})")

    print("== .pyi without C++ module (gate) ==")
    for m in orphan_pyi:
        failures += 1
        print(f"  {m}: {len(pyi[m])} functions ({', '.join(sorted(pyi[m]))})")

    init_missing = sorted(set(cpp) - init_names)
    init_orphan = sorted(init_names - set(cpp))
    if init_missing or init_orphan:
        failures += 1
        print("== __init__.pyi module list drift (gate) ==")
        if init_missing:
            print(f"  not exported: {', '.join(init_missing)}")
        if init_orphan:
            print(f"  exported but no C++ module: {', '.join(init_orphan)}")

    print("== per-module drift ==")
    for m in sorted(set(cpp) & set(pyi)):
        cpp_snake = {camel_to_snake(f) for f in cpp[m]}
        pyi_snake = {camel_to_snake(f) for f in pyi[m]}
        missing = sorted(cpp_snake - pyi_snake)
        extra = sorted(pyi_snake - cpp_snake)
        if missing:
            failures += 1
            print(f"  {m}: C++ only ({len(missing)}): {', '.join(missing)}")
        if extra:
            # pythonic 手写别名（after/cancel 等）不算漂移，仅提示
            print(f"  {m}: pyi-only aliases ({len(extra)}): {', '.join(extra)}")

    print(
        f"== summary: {len(cpp)} C++ modules, {len(pyi)} pyi files; "
        f"{len(missing_pyi)} modules missing pyi, {len(orphan_pyi)} orphan pyi "
        f"({failures} gate failures) =="
    )
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
