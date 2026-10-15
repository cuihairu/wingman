#!/usr/bin/env python3
"""Audit event-emission conventions across C++ script modules.

Frozen conventions (development-todo「统一回调/通知策略」, 代决 2026-10-16):

1. Cross-background-thread callbacks run **synchronously on the calling thread**
   and are gated by ``ScriptValue::callableThreadSafe``. Non-thread-safe
   callables (Lua) are rejected at registration time -- there is deliberately no
   main-thread queue/coroutine pump in the runtime to dispatch them onto.
2. A rejected registration reports on the script bus as ``<module>.error`` with
   a single-key payload ``{"error": <reason>}``, where ``<module>`` is one of the
   modules registered in the same file.
3. Every ``<module>.error`` event emitted by a script module must be listed in
   the 内置事件源 table of docs/api/event.md (payload cell exactly ``{error}``),
   and every ``*.error`` row of that table must be emitted somewhere in the C++
   tree.

Scope: rule 1-2 gate ``lib/wingman/src/script/modules/*.cpp`` only (the script
bus). Transport-channel events (e.g. agentcore's ``proxy.error``, sent upstream
rather than to EventHub) follow their own contract documented in
docs/protocols.md and are intentionally out of scope for the payload rule.

Exit 0 when no gating violation remains; nonzero otherwise. Runs in CI next to
``scripts/audit_pyi_drift.py`` (job ``event-conventions``).
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MODULES = ROOT / "lib" / "wingman" / "src" / "script" / "modules"
DOC = ROOT / "docs" / "api" / "event.md"
SCAN_ROOTS = (ROOT / "lib" / "wingman" / "src", ROOT / "libs", ROOT / "apps")

EMIT_CALL = re.compile(r"emit\(\s*")
MOD_NAME = re.compile(r'mod\.name\s*=\s*"(\w+)"')


def string_literal(src, i):
    """src[i] == '"' or "'"; return (value, index just past the closing quote)."""
    quote = src[i]
    i += 1
    out = []
    while i < len(src):
        c = src[i]
        if c == "\\":
            if i + 1 < len(src):
                out.append(src[i + 1])
                i += 2
                continue
            break
        if c == quote:
            return "".join(out), i + 1
        out.append(c)
        i += 1
    return "".join(out), i


def match_braces(src, start):
    """src[start] == '{'; return the full balanced region, or None if unbalanced."""
    depth = 0
    i = start
    while i < len(src):
        c = src[i]
        if c in "\"'":
            i = string_literal(src, i)[1]
            continue
        if c == "/" and i + 1 < len(src):
            if src[i + 1] == "/":
                i = src.find("\n", i)
                i = len(src) if i < 0 else i
                continue
            if src[i + 1] == "*":
                end = src.find("*/", i + 2)
                i = len(src) if end < 0 else end + 2
                continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[start : i + 1]
        i += 1
    return None


def payload_keys(payload):
    """Top-level initializer_list keys of ``{{"k", v}, ...}`` (pair at depth 2)."""
    keys = []
    i, depth = 1, 1
    while i < len(payload):
        c = payload[i]
        if c in "\"'":
            value, i = string_literal(payload, i)
            j = i
            while j < len(payload) and payload[j] in " \t\r\n":
                j += 1
            if depth == 2 and j < len(payload) and payload[j] == ",":  # {"key", value}
                keys.append(value)
            continue
        if c == "/" and i + 1 < len(payload):
            if payload[i + 1] == "/":
                i = payload.find("\n", i)
                i = len(payload) if i < 0 else i
                continue
            if payload[i + 1] == "*":
                end = payload.find("*/", i + 2)
                i = len(payload) if end < 0 else end + 2
                continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        i += 1
    return keys


def scan_file(path):
    """Return (modules, emits) where emits = [(line, name, keys or None)]."""
    src = path.read_text(encoding="utf-8")
    modules = set(MOD_NAME.findall(src))
    emits = []
    for m in EMIT_CALL.finditer(src):
        j = m.end()
        if j >= len(src) or src[j] not in "\"'":
            continue
        name, end = string_literal(src, j)
        k = end
        while k < len(src) and src[k] in " \t\r\n":
            k += 1
        if k < len(src) and src[k] == ",":  # name 与载荷之间的逗号
            k += 1
            while k < len(src) and src[k] in " \t\r\n":
                k += 1
        keys = None
        if k < len(src) and src[k] == "{":
            region = match_braces(src, k)
            if region is not None:
                keys = payload_keys(region)
        emits.append((src.count("\n", 0, m.start()) + 1, name, keys))
    return modules, emits


def doc_error_rows():
    """Parse the 内置事件源 table: {event: (source_cell, payload_cell)} for error rows."""
    rows = {}
    for line in DOC.read_text(encoding="utf-8").splitlines():
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 3 or not cells[0].startswith("`"):
            continue
        names = [n.strip().strip("`") for n in cells[0].split("/")]
        for n in names:
            if n.endswith(".error"):
                rows[n] = (cells[1].strip("`"), cells[2].strip("`"))
    return rows


def main():
    fails = []
    files = sorted(MODULES.glob("*.cpp"))
    sites = 0
    code_error = set()
    for path in files:
        modules, emits = scan_file(path)
        gated = "callableThreadSafe" in path.read_text(encoding="utf-8")
        for line, name, keys in emits:
            sites += 1
            if not name or "." not in name:
                fails.append(
                    f"{path.relative_to(ROOT)}:{line}: event {name!r} is not "
                    "<module>.<verb>"
                )
                continue
            module = name.split(".", 1)[0]
            if module not in modules:
                fails.append(
                    f"{path.relative_to(ROOT)}:{line}: event {name!r} module prefix "
                    f"{module!r} is not registered in this file ({sorted(modules)})"
                )
                continue
            if not name.endswith(".error"):
                continue  # dynamic prefixes ("notify.tray.") and lifecycle verbs are free-form
            code_error.add(name)
            if keys is None:
                fails.append(
                    f"{path.relative_to(ROOT)}:{line}: {name} payload is not a literal "
                    "initializer list"
                )
            elif keys != ["error"]:
                fails.append(
                    f"{path.relative_to(ROOT)}:{line}: {name} payload keys {keys!r} "
                    "must be exactly ['error']"
                )
            if not gated:
                fails.append(
                    f"{path.relative_to(ROOT)}:{line}: {name} is emitted but the file "
                    "has no callableThreadSafe gate"
                )

    doc_rows = doc_error_rows()
    missing_docs = sorted(code_error - set(doc_rows))
    for name in missing_docs:
        fails.append(
            f"docs/api/event.md: {name} is emitted by a script module but missing from "
            "the 内置事件源 table"
        )
    tree_error = set()
    for root in SCAN_ROOTS:
        for path in root.rglob("*.cpp"):
            src = path.read_text(encoding="utf-8", errors="ignore")
            for m in re.finditer(r'"([a-z_][a-z_.]*\.error)"', src):
                tree_error.add(m.group(1))
    for name in sorted(set(doc_rows) - tree_error):
        fails.append(f"docs/api/event.md: row {name} is not emitted anywhere in the C++ tree")
    for name, (source, payload) in sorted(doc_rows.items()):
        module = name.split(".", 1)[0]
        if payload != "{error}":
            fails.append(
                f"docs/api/event.md: row {name} payload cell {payload!r} must be exactly "
                f"'{'{error}'}'"
            )
        if source != f'"{module}"':
            fails.append(
                f"docs/api/event.md: row {name} source cell {source!r} must be "
                f'"{module}"'
            )

    print(f"== scanned {len(files)} module files, {sites} emit sites, "
          f"{len(code_error)} error events, {len(doc_rows)} doc rows ==")
    for f in fails:
        print(f"FAIL {f}")
    if fails:
        print(f"== summary: {len(fails)} gating failures ==")
        return 1
    print("== summary: 0 gating failures ==")
    return 0


if __name__ == "__main__":
    sys.exit(main())
