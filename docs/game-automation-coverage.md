# Game Automation Coverage Analysis

This document records the current game-automation coverage of Wingman and compares it with similar products. It is intended to guide future implementation and prevent scope drift.

## Positioning

Wingman targets non-invasive game automation based on:

- Screen capture, pixel/color/image recognition, OCR.
- Mouse and keyboard input simulation.
- Lua/Python scripting.
- Local IPC control from the desktop UI.
- Optional central orchestration through the Go server, with runtime connecting outbound.

Wingman should not become an injection, memory-reading, or anti-cheat bypass framework.

## Current Coverage

Current coverage is strong at the engine/API layer and weaker at the product UX layer.

| Area | Coverage | Notes |
| --- | --- | --- |
| Screen capture | High | Supports screenshots and bitmap operations. |
| Pixel/color detection | High | Covers cooldowns, health bars, resource indicators, UI state checks. |
| Image/template matching | Medium-High | Core capability exists, but template asset management and visual tooling are still needed. |
| OCR | Medium | Useful for UI prompts and text detection; robustness depends on preprocessing and OCR quality. |
| Input simulation | High | Mouse and keyboard automation covers macro and action execution. |
| Macro recording/replay | Medium | Core recording/replay exists, but timeline editing and user workflow are incomplete. |
| Trigger system | High at engine level | Conditions/actions/cooldowns are aligned with game automation needs. GUI editing is still missing. |
| Scripting | High | Lua and Python support make Wingman stronger than many simple macro tools. |
| FSM / behavior tree / task orchestration | High | Suitable for combat loops, gathering loops, and complex state handling. |
| Humanized input | Medium-High | Random delay and human-like movement direction is correct; tuning UI and profiles are needed. |
| Game profiles | Medium | Basic profile direction exists; needs stronger asset/config binding per game. |
| Local UI control | Medium | IPC architecture is correct; UI needs trigger/profile/debug workflows. |
| Remote orchestration | Medium | Architecture is correct: Go server as central controller, runtime connects outbound. Protocol and UX still need maturation. |
| Team coordination | Medium | Direction exists; voting, member state, task assignment, and server arbitration need productization. |
| Advanced vision / ML | Low-Medium | Basic CV exists; object detection and robust scene understanding are future work. |
| Product usability | Medium-Low | Main gap: visual configuration, hotkeys, preview/debugging, template management. |

Approximate coverage:

- Engine capability: 70-80%.
- Game-user product experience: 40-50%.
- Advanced bot/AI/memory-based automation: low, and memory/injection should remain out of scope.

## Covered Game Scenarios

| Scenario | Status |
| --- | --- |
| Auto-clicking and simple idle loops | Covered. |
| Repeated gathering/crafting actions | Covered with macro, input, and script loops. |
| Skill cooldown detection | Covered with pixel/color/image detection. |
| Health/mana monitoring | Covered with region-based color/pixel detection. |
| Button/icon appears then click | Mostly covered; needs better template tooling. |
| UI prompt text detection | Covered partially through OCR. |
| Complex combat rotation | Supported through scripting, FSM, behavior tree, and triggers. |
| Multi-character/team coordination | Architecture exists, but server-side protocol and UX need work. |
| No-code configuration for normal players | Not enough yet. |
| Robust navigation/pathfinding | Not enough yet. |
| Object recognition with ML models | Not enough yet. |
| Memory reading/injection bot | Out of scope. |

## Similar Product Comparison

| Product | Main Capabilities | Wingman Comparison |
| --- | --- | --- |
| Chimpeon | Game automation with triggers, fast pixel detection, keyboard/mouse/macro actions, combat rotation, AFK handling, prompt detection. | Wingman is comparable or stronger at engine level because of Lua/Python, FSM, behavior tree, and orchestration. Chimpeon is stronger in game-focused UX and trigger configuration. |
| AutoHotkey | Hotkeys, scripting, macro automation, keyboard/mouse/window automation, large ecosystem. | Wingman is more game-automation-specific and has built-in vision/OCR/behavior modules. AutoHotkey is stronger in hotkeys, community scripts, and Windows automation maturity. |
| Pulover's Macro Creator | GUI macro creator based on AutoHotkey, recording, loops, conditions, variables, image/pixel search. | Wingman has stronger engine architecture, but Pulover is stronger in visual macro editing and low-code workflow. |
| SikuliX | Image-driven automation, wait/click on images, OCR support, visual scripting style. | Wingman has similar primitives but lacks the convenient high-level image API and region-based visual workflow. |
| TinyTask | Minimal record/replay, loops, playback speed, simple executable workflow. | Wingman is far more powerful, but TinyTask wins on simplicity. Wingman needs a simple mode for casual users. |
| Macro Recorder | Mouse/keyboard recording, playback, image/OCR search, GUI action editing. | Wingman can support similar scenarios but lacks mature action-list editing, condition branches, and visual debugging. |

References:

- Chimpeon: https://chimpeon.com/
- Chimpeon help: https://chimpeon.com/support/help?id=2
- AutoHotkey docs: https://ahkscript.github.io/ko/docs/Program.htm
- Pulover's Macro Creator features: https://www.macrocreator.com/features/
- SikuliX docs: https://sikulix.github.io/docs
- SikuliX OCR docs: https://sikulix.github.io/docs/api/ocr/
- TinyTask: https://www.tinytask.net/
- Macro Recorder docs: https://www.macrorecorder.com/doc/
- Macro Recorder bitmap/OCR search: https://www.macrorecorder.com/doc/find/

## Main Gaps

### Product UX Gaps

- Global hotkeys: start, pause, resume, stop, emergency stop, switch profile.
- Visual screen preview: live screenshot, selected region, current cursor, pixel color.
- Region selector: drag to select screen/window area and save as named region.
- Template capture tool: crop image from preview and save as a named template.
- Template asset manager: organize button/icon/state images per game profile.
- Trigger GUI editor: condition, action, cooldown, max count, enabled state, logs.
- Macro timeline editor: reorder actions, edit delays, insert loops/conditions.
- Runtime debug panel: last screenshot, matched regions, confidence, trigger hits.
- Profile import/export: share game profiles and templates.
- Hot reload: update scripts/configs without restarting runtime.

### Engine/API Gaps

- High-level visual API similar to SikuliX: `region.waitImage(...).click()`.
- Better OCR preprocessing: thresholding, scaling, region filters, language/profile presets.
- Multi-resolution and DPI adaptation.
- Window-relative coordinates instead of absolute screen-only coordinates.
- Retry/timeout/fallback primitives for image/OCR operations.
- Failure recovery: stuck detection, window lost, game restart, reset flow.
- Optional object detection layer: ONNX/YOLO-style target detection.
- Asset versioning for templates and regions.

### Orchestration/Team Gaps

- Server-arbitrated team voting.
- Member state model: online, busy, idle, failed, paused.
- Task assignment and lease/ack model.
- Runtime inbox controlled by server.
- Team event broadcast from server to runtimes.
- Dashboard view for team state and vote progress.

## Recommended Priority

1. Global hotkeys and emergency stop.
2. Screen preview, region selection, and pixel/color picker.
3. Template capture and template asset management.
4. Trigger GUI editor.
5. Macro timeline editor.
6. Runtime debug panel with hit visualization.
7. Window-relative coordinate system and DPI/resolution adaptation.
8. High-level region/image/OCR API.
9. Failure recovery primitives.
10. Team coordination protocol through Go server arbitration.

## Architecture Guardrails

- Runtime local UI control must use IPC, not a runtime HTTP/WebSocket server.
- Runtime must connect outbound to the Go orchestrator for remote control.
- Go server remains the central controller for team state, voting, task assignment, and dashboard communication.
- Runtime may consume server-assigned work, but should not become an independent remote-control server.
- Local `event` should remain an in-process event bus.
- Message queues are acceptable as server-managed infrastructure or server-assigned runtime inboxes, but not as arbitrary remote publish APIs exposed through the local event module.

## Product Direction

The next step should not be adding many more low-level modules. The current core is already broad enough for most non-invasive game automation. The highest-value work is turning existing primitives into a game-focused product workflow:

1. Capture the screen.
2. Select a region.
3. Pick a color or save a template.
4. Create a trigger.
5. Attach an action or macro.
6. Preview/debug the match result.
7. Run with hotkeys and emergency stop.
8. Package the configuration as a game profile.

This path will make Wingman competitive with game automation tools while preserving the current architecture.
