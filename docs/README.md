> [中文](zh/README.md) | **English**

# Wingman Documentation

Welcome to the Wingman project documentation.

## Documentation Navigation

### Quick Start

| Document | Description |
|------|------|
| [Quick Start](guide/getting-started.md) | Build locally, run your first script |
| [Build Guide](../BUILD.md) | Platform dependencies, vcpkg, testing and troubleshooting (repository root) |
| [VS Code Development Environment](development-environment.md) | Lua/Python completion, debugging, and task configuration |
| [Configuration Management Practices](guides/configuration.md) | INI/JSON configuration management practices |
| [Android Agent](../apps/android/README.md) | APK acquisition and installation, capability list, on-device acceptance steps (repository root) |
| [Android Agent](android-agent.md) | On-device Agent for Android (experimental), outbound TCP to the Go orchestrator, Dashboard unified dispatch and viewing |

### API Reference

| Module | Description |
|------|------|
| [API Overview](api/overview.md) | Overall design and naming conventions of the scripting API |
| [API Index](api/index.md) | Entry points for all modules |
| [Core API](api/core.md) | Core capabilities such as screen, input, and windows |
| [Key-Value Store](api/kv.md) / [Database](api/db.md) | kv key-value store and SQLite |
| [Serialization](api/serialize.md) / [JSON](api/json.md) / [INI](api/ini.md) | Format handling for JSON, INI, etc. |
| [Script API](api/script.md) | Script execution, tasks, and events |
| [Debugger](api/debugger.md) | Lua debugging interface documentation |

### Usage Guides

| Guide | Description |
|------|------|
| [Using the Database](guides/database.md) | How to use SQLite and kv |
| [Trigger System](guides/triggers.md) | Automated trigger conditions and actions |
| [UIA Guide](guides/uia-guide.md) | Windows UI Automation usage |
| [YOLO Guide](guides/yolo-guide.md) | Optional ML/ONNX model preparation and load verification |
| [Asset Distribution Guide](guides/asset-sync.md) | Static assets such as template images distributed from the server to agent local storage and referenced by scripts |
| [On-Device Verification Guide](guides/manual-verification.md) | macOS runtime / Linux XRecord on-device script operations and pass criteria |

### Architecture and Development

| Document | Description |
|------|------|
| [Architecture Overview](architecture.md) | Boundaries of Runtime, GUI, and Orchestrator |
| [Architecture Decisions](architecture-decisions.md) | Record of key design decisions |
| [Platform Support](platforms.md) | Support matrix, per-platform API applicability and environment differences |
| [Platform Abstraction Design](platform-abstraction-design.md) | Cross-platform abstraction layer design |
| [UIA Abstraction Design](uia-abstraction-design.md) | UI Automation abstraction layer design |
| [Project Structure](project-structure.md) | Directory responsibilities and module breakdown |
| [Development Guide](DEVELOPMENT.md) | Build, test, and contribution workflow |
| [VS Code Development Environment](development-environment.md) | Repository-level editor configuration and script debugging workflow |
| [Developer Tools](DEVELOPER_TOOLS.md) | Local development helper tools |
| [Dependencies](dependencies.md) | Dependency sources and purposes |

### Example Code

| Category | Description |
|------|------|
| [Examples Index](examples/index.md) | Entry point for all examples |
| [Hello World](examples/hello-world.md) | Minimal script example |
| [Pixel Detection](examples/pixel-detection.md) | Screen color sampling and conditional logic |
| [Image Matching](examples/image-matching.md) | Image finding example |
| [Auto Loop](examples/auto-loop.md) | Continuous detection and action execution |
| [Macro Recording](examples/macro-record.md) | Recording and replaying operations |
| [UI Automation](examples/ui-automation.md) | Windows control automation |

## Feature Status

The default build focuses on the local Runtime, Lua scripting, and core capabilities such as screen/input/window. The following modules require additional dependencies, build toggles, or are still being iterated:

| Module | Current Status |
|------|----------|
| OCR | Optional capability, depends on Tesseract/runtime configuration |
| ML/YOLO | Optional capability, AI vision detection APIs are available (cloud provider + local ONNX: `aiSetup`/`aiSetupLocal`/`aiLocate`/`aiElements`) |
| Vision | Basic image capabilities available, advanced detection enabled per build configuration |
| Debugger | Lua debugging interface exists, requires a debug plugin/configuration |
| Orchestration | Go server + Dashboard + runtime agent remote orchestration, a separate deployment chain |

## Getting Help

- [GitHub Issues](https://github.com/cuihairu/wingman/issues)
- [GitHub Discussions](https://github.com/cuihairu/wingman/discussions)
- Example scripts directory: [`examples/`](../examples/)

---

**[Back to Home](../README.md)**
