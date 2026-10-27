> [中文](../zh/examples/index.md) | **English**

# Example Scripts

The following examples demonstrate various features of Wingman:

## Basic Examples

- [Hello World](hello-world.md) - The simplest example
- [Pixel Detection](pixel-detection.md) - Detect a specified color on the screen
- [Image Matching](image-matching.md) - Find an image on the screen
- [Auto Loop](auto-loop.md) - Continuously monitor and perform actions
- [Macro Record](macro-record.md) - Record and replay operations

## UI Automation Example

- [UI Automation](../zh/examples/ui-automation.md) - Windows control automation interaction

## Feature Examples (examples/lua_scripts)

- `template_batch.lua` - Named template registration and batch recognition (templateAdd / templateFindAll)
- `kv_store.lua`, `color_detect.lua`, `window_control.lua`, etc. - Individual examples for each module; see the directory

## Python Examples (examples/python_scripts)

Python and Lua share the same module surface; functions expose both their camelCase original names and snake_case aliases (snake_case is recommended, consistent with the `libs/python/typing` stubs):

- `hello.py` - KV storage + notifications primer
- `template_batch.py` - Batch template management (same shape as `template_batch.lua`)
- `workflow_pipeline.py` - Workflow orchestration: dependencies, conditional branching, output aggregation (run/when require a thread-safe callable; Lua function bodies are rejected on submission, so the orchestration example is Python-only)
- `events_fsm.py` - Event subscription combined with an FSM state machine

## Running an Example

```bash
wingman-agent.exe script examples/lua_scripts/hello_world.lua
```
