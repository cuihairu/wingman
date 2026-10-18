````text
[0m
> build · gpt-6-luna
[0m→ [0mSkill "code-review" 
> build · gpt-6-luna
我会按全仓架构审查处理，先确认项目约定与代码/文档分布，再核对具体风险点和文档—实现偏差；不会修改文件。
[0m→ [0mRead . 
[0m✱ [0mGlob "**/AGENTS.md" [90min . · 0 matches[0m
[0m✱ [0mGlob "**/{CLAUDE.md,README*,project.yaml,package.json,pyproject.toml,Cargo.toml,go.mod}" [90min . · 41 matches[0m
[0m$ [0mgit status --short && git log -1 --oneline
06d41c2 docs: fix first-visit language redirect direction
> build · gpt-6-luna
[0m→ [0mRead CLAUDE.md 
[0m→ [0mRead todo.md 
[0m→ [0mRead README.md 
[0m→ [0mRead README.zh.md 
[0m$ [0mgit ls-files | awk 'BEGIN{FS="/"} $1!="third_party" && $1!="vcpkg_installed" && $1!="vcpkg" && $1!=".git" {print}' | sed -n '1,260p'
.gitattributes
.github/workflows/build-agent.yml
.github/workflows/build-package.yml
.github/workflows/ci.yml
.github/workflows/deploy-docs.yml
.github/workflows/deploy-server.yml
.github/workflows/docs.yml
.github/workflows/nightly.yml
.github/workflows/release.yml
.github/workflows/verify-platforms.yml
.gitignore
.gitmodules
.spec-workflow/templates/design-template.md
.spec-workflow/templates/product-template.md
.spec-workflow/templates/requirements-template.md
.spec-workflow/templates/structure-template.md
.spec-workflow/templates/tasks-template.md
.spec-workflow/templates/tech-template.md
.spec-workflow/user-templates/README.md
.vscode/extensions.json
.vscode/launch.json
.vscode/settings.json
.vscode/tasks.json
BUILD.md
CHANGELOG.md
CLAUDE.md
CMakeLists.txt
CONTRIBUTING.md
LICENSE
README.md
README.zh.md
ROADMAP.md
SECURITY_AUDIT_SUMMARY.md
apps/agent/CMakeLists.txt
apps/agent/config/agent.toml
apps/agent/include/wingman/runtime/agent.hpp
apps/agent/include/wingman/runtime/cli.hpp
apps/agent/include/wingman/runtime/commands/build_command.hpp
apps/agent/include/wingman/runtime/commands/crash_test_command.hpp
apps/agent/include/wingman/runtime/commands/script_command.hpp
apps/agent/include/wingman/runtime/commands/start_command.hpp
apps/agent/include/wingman/runtime/commands/stop_command.hpp
apps/agent/include/wingman/runtime/config.hpp
apps/agent/include/wingman/runtime/crash_setup.hpp
apps/agent/include/wingman/runtime/event_log_sink.hpp
apps/agent/include/wingman/runtime/local_ipc_server.hpp
apps/agent/include/wingman/runtime/notify_bridge.hpp
apps/agent/include/wingman/runtime/packer.hpp
apps/agent/include/wingman/runtime/resource_loader.hpp
apps/agent/include/wingman/runtime/resource_pack.hpp
apps/agent/include/wingman/runtime/rpc/config_handler.hpp
apps/agent/include/wingman/runtime/rpc/event_handler.hpp
apps/agent/include/wingman/runtime/rpc/macro_handler.hpp
apps/agent/include/wingman/runtime/rpc/screenshot_handler.hpp
apps/agent/include/wingman/runtime/rpc/system_handler.hpp
apps/agent/include/wingman/runtime/runtime_context.hpp
apps/agent/include/wingman/runtime/standalone_mode.hpp
apps/agent/src/agent.cpp
apps/agent/src/agent_config.cpp
apps/agent/src/cli.cpp
apps/agent/src/commands/build_command.cpp
apps/agent/src/commands/crash_test_command.cpp
apps/agent/src/commands/script_command.cpp
apps/agent/src/commands/start_command.cpp
apps/agent/src/commands/stop_command.cpp
apps/agent/src/crash_setup.cpp
apps/agent/src/local_ipc_server.cpp
apps/agent/src/main.cpp
apps/agent/src/notify_bridge.cpp
apps/agent/src/packer.cpp
apps/agent/src/platform/agent_platform.hpp
apps/agent/src/platform/posix/agent_platform.cpp
apps/agent/src/platform/win/agent_platform.cpp
apps/agent/src/resource_loader.cpp
apps/agent/src/rpc/handlers/config_handler.cpp
apps/agent/src/rpc/handlers/event_handler.cpp
apps/agent/src/rpc/handlers/macro_handler.cpp
apps/agent/src/rpc/handlers/screenshot_handler.cpp
apps/agent/src/rpc/handlers/script_handler.cpp
apps/agent/src/rpc/handlers/system_handler.cpp
apps/agent/src/runtime_context.cpp
apps/agent/src/standalone_mode.cpp
apps/agent/src/system_event_bridge.cpp
apps/agent/tests/CMakeLists.txt
apps/agent/tests/agent_config_test.cpp
apps/agent/tests/agent_loopback_test.cpp
apps/agent/tests/cli_test.cpp
apps/agent/tests/cli_test_stubs.cpp
apps/agent/tests/notify_bridge_test.cpp
apps/agent/tests/resource_loader_interface_test.cpp
apps/agent/tests/resource_pack_test.cpp
apps/agent/tests/rpc_ipc_test.cpp
apps/agent/tests/screen_runtime_test.cpp
apps/agent/tests/screenshot_handler_test.cpp
apps/agent/tests/standalone_mode_coverage_test.cpp
apps/agent/tests/system_event_bridge_test.cpp
apps/agent/tests/system_handler_test.cpp
apps/android/.gitignore
apps/android/README.md
apps/android/app/build.gradle.kts
apps/android/app/src/main/AndroidManifest.xml
apps/android/app/src/main/java/com/wingman/agent/AgentPrefs.kt
apps/android/app/src/main/java/com/wingman/agent/BootCompletedReceiver.kt
apps/android/app/src/main/java/com/wingman/agent/BootStartGate.kt
apps/android/app/src/main/java/com/wingman/agent/CrashAlarmReceiver.kt
apps/android/app/src/main/java/com/wingman/agent/CrashRestartHandler.kt
apps/android/app/src/main/java/com/wingman/agent/MainActivity.kt
apps/android/app/src/main/java/com/wingman/agent/RestartPolicy.kt
apps/android/app/src/main/java/com/wingman/agent/RestrictedSettingsPolicy.kt
apps/android/app/src/main/java/com/wingman/agent/ScreenCaptureManager.kt
apps/android/app/src/main/java/com/wingman/agent/SecretStore.kt
apps/android/app/src/main/java/com/wingman/agent/WatchdogPolicy.kt
apps/android/app/src/main/java/com/wingman/agent/WingmanAccessibilityService.kt
apps/android/app/src/main/java/com/wingman/agent/WingmanApplication.kt
apps/android/app/src/main/java/com/wingman/agent/WingmanJni.kt
apps/android/app/src/main/java/com/wingman/agent/WingmanService.kt
apps/android/app/src/main/res/drawable/ic_launcher_fg.xml
apps/android/app/src/main/res/layout/activity_main.xml
apps/android/app/src/main/res/mipmap-anydpi-v26/ic_launcher.xml
apps/android/app/src/main/res/values-night/themes.xml
apps/android/app/src/main/res/values/colors.xml
apps/android/app/src/main/res/values/strings.xml
apps/android/app/src/main/res/values/themes.xml
apps/android/app/src/main/res/xml/accessibility_config.xml
apps/android/app/src/test/java/com/wingman/agent/BootStartGateTest.kt
apps/android/app/src/test/java/com/wingman/agent/RestartPolicyTest.kt
apps/android/app/src/test/java/com/wingman/agent/RestrictedSettingsPolicyTest.kt
apps/android/app/src/test/java/com/wingman/agent/TokenMigrationTest.kt
apps/android/app/src/test/java/com/wingman/agent/WatchdogPolicyTest.kt
apps/android/build.gradle.kts
apps/android/cpp/CMakeLists.txt
apps/android/cpp/agent/android_agent.cpp
apps/android/cpp/agent/android_agent.hpp
apps/android/cpp/agent/android_screenshot.cpp
apps/android/cpp/agent/android_screenshot.hpp
apps/android/cpp/agent/base64.hpp
apps/android/cpp/jni_bridge.cpp
apps/android/cpp/vcpkg-android.cmake
apps/android/cpp/vcpkg.json
apps/android/gradle.properties
apps/android/settings.gradle.kts
apps/gui/.gitignore
apps/gui/.npmrc
apps/gui/README.md
apps/gui/index.html
apps/gui/package-lock.json
apps/gui/package.json
apps/gui/pnpm-lock.yaml
apps/gui/pnpm-workspace.yaml
apps/gui/src-tauri/Cargo.lock
apps/gui/src-tauri/Cargo.toml
apps/gui/src-tauri/build.rs
apps/gui/src-tauri/icons/128x128.png
apps/gui/src-tauri/icons/128x128@2x.png
apps/gui/src-tauri/icons/32x32.png
apps/gui/src-tauri/icons/64x64.png
apps/gui/src-tauri/icons/README.md
apps/gui/src-tauri/icons/Square107x107Logo.png
apps/gui/src-tauri/icons/Square142x142Logo.png
apps/gui/src-tauri/icons/Square150x150Logo.png
apps/gui/src-tauri/icons/Square284x284Logo.png
apps/gui/src-tauri/icons/Square30x30Logo.png
apps/gui/src-tauri/icons/Square310x310Logo.png
apps/gui/src-tauri/icons/Square44x44Logo.png
apps/gui/src-tauri/icons/Square71x71Logo.png
apps/gui/src-tauri/icons/Square89x89Logo.png
apps/gui/src-tauri/icons/StoreLogo.png
apps/gui/src-tauri/icons/android/mipmap-anydpi-v26/ic_launcher.xml
apps/gui/src-tauri/icons/android/mipmap-hdpi/ic_launcher.png
apps/gui/src-tauri/icons/android/mipmap-hdpi/ic_launcher_foreground.png
apps/gui/src-tauri/icons/android/mipmap-hdpi/ic_launcher_round.png
apps/gui/src-tauri/icons/android/mipmap-mdpi/ic_launcher.png
apps/gui/src-tauri/icons/android/mipmap-mdpi/ic_launcher_foreground.png
apps/gui/src-tauri/icons/android/mipmap-mdpi/ic_launcher_round.png
apps/gui/src-tauri/icons/android/mipmap-xhdpi/ic_launcher.png
apps/gui/src-tauri/icons/android/mipmap-xhdpi/ic_launcher_foreground.png
apps/gui/src-tauri/icons/android/mipmap-xhdpi/ic_launcher_round.png
apps/gui/src-tauri/icons/android/mipmap-xxhdpi/ic_launcher.png
apps/gui/src-tauri/icons/android/mipmap-xxhdpi/ic_launcher_foreground.png
apps/gui/src-tauri/icons/android/mipmap-xxhdpi/ic_launcher_round.png
apps/gui/src-tauri/icons/android/mipmap-xxxhdpi/ic_launcher.png
apps/gui/src-tauri/icons/android/mipmap-xxxhdpi/ic_launcher_foreground.png
apps/gui/src-tauri/icons/android/mipmap-xxxhdpi/ic_launcher_round.png
apps/gui/src-tauri/icons/android/values/ic_launcher_background.xml
apps/gui/src-tauri/icons/icon.icns
apps/gui/src-tauri/icons/icon.ico
apps/gui/src-tauri/icons/icon.png
apps/gui/src-tauri/icons/ios/AppIcon-20x20@1x.png
apps/gui/src-tauri/icons/ios/AppIcon-20x20@2x-1.png
apps/gui/src-tauri/icons/ios/AppIcon-20x20@2x.png
apps/gui/src-tauri/icons/ios/AppIcon-20x20@3x.png
apps/gui/src-tauri/icons/ios/AppIcon-29x29@1x.png
apps/gui/src-tauri/icons/ios/AppIcon-29x29@2x-1.png
apps/gui/src-tauri/icons/ios/AppIcon-29x29@2x.png
apps/gui/src-tauri/icons/ios/AppIcon-29x29@3x.png
apps/gui/src-tauri/icons/ios/AppIcon-40x40@1x.png
apps/gui/src-tauri/icons/ios/AppIcon-40x40@2x-1.png
apps/gui/src-tauri/icons/ios/AppIcon-40x40@2x.png
apps/gui/src-tauri/icons/ios/AppIcon-40x40@3x.png
apps/gui/src-tauri/icons/ios/AppIcon-512@2x.png
apps/gui/src-tauri/icons/ios/AppIcon-60x60@2x.png
apps/gui/src-tauri/icons/ios/AppIcon-60x60@3x.png
apps/gui/src-tauri/icons/ios/AppIcon-76x76@1x.png
apps/gui/src-tauri/icons/ios/AppIcon-76x76@2x.png
apps/gui/src-tauri/icons/ios/AppIcon-83.5x83.5@2x.png
apps/gui/src-tauri/src/commands/config.rs
apps/gui/src-tauri/src/commands/connection.rs
apps/gui/src-tauri/src/commands/events.rs
apps/gui/src-tauri/src/commands/macros.rs
apps/gui/src-tauri/src/commands/mod.rs
apps/gui/src-tauri/src/commands/profiles.rs
apps/gui/src-tauri/src/commands/screen.rs
apps/gui/src-tauri/src/commands/script_files.rs
apps/gui/src-tauri/src/commands/scripts.rs
apps/gui/src-tauri/src/commands/system.rs
apps/gui/src-tauri/src/commands/triggers.rs
apps/gui/src-tauri/src/hotkeys.rs
apps/gui/src-tauri/src/ipc/client.rs
apps/gui/src-tauri/src/ipc/integration_tests.rs
apps/gui/src-tauri/src/ipc/mod.rs
apps/gui/src-tauri/src/main.rs
apps/gui/src-tauri/src/models.rs
apps/gui/src-tauri/src/state.rs
apps/gui/src-tauri/src/tray.rs
apps/gui/src-tauri/tauri.conf.json
apps/gui/src/App.svelte
apps/gui/src/app.css
apps/gui/src/lib/components/ColorPicker.svelte
apps/gui/src/lib/components/RegionPicker.svelte
apps/gui/src/lib/components/ScreenPickerModal.svelte
apps/gui/src/lib/components/layout/Sidebar.svelte
apps/gui/src/lib/components/layout/TopBar.svelte
apps/gui/src/lib/router.svelte.ts
apps/gui/src/lib/stores/connection.ts
apps/gui/src/lib/stores/events.ts
apps/gui/src/lib/stores/logs.ts
apps/gui/src/lib/stores/macros.ts
apps/gui/src/lib/stores/profiles.ts
apps/gui/src/lib/stores/screen.ts
apps/gui/src/lib/stores/script-files.ts
apps/gui/src/lib/stores/scripts.ts
apps/gui/src/lib/stores/settings.ts
apps/gui/src/lib/stores/theme.ts
apps/gui/src/lib/stores/triggers.ts
apps/gui/src/main.ts
apps/gui/src/routes/dashboard/+page.svelte
apps/gui/src/routes/logs/+page.svelte
apps/gui/src/routes/macros/+page.svelte
apps/gui/src/routes/screen/+page.svelte
apps/gui/src/routes/scripts/+page.svelte
apps/gui/src/routes/settings/+page.svelte
apps/gui/src/routes/triggers/+page.svelte
apps/gui/src/svelte.d.ts
apps/gui/svelte.config.js
apps/gui/tests/app-invoke-2.test.ts
apps/gui/tests/app-invoke.test.ts
apps/gui/tests/app-shell.test.ts
apps/gui/tests/components.test.ts
apps/gui/tests/connection-store-2.test.ts
apps/gui/tests/connection-store.test.ts
> build · gpt-6-luna
[0m→ [0mRead docs/architecture-decisions.md 
[0m→ [0mRead ROADMAP.md 
[0m→ [0mRead docs/architecture.md 
[0m→ [0mRead docs/审计-文档一致性.md 
[0m$ [0mgit ls-files 'orchestrator/server/**/*.go' 'libs/**/*.cpp' 'libs/**/*.hpp' 'apps/agent/src/**/*.cpp' 'docs/**/*.md' | wc -l && find orchestrator/server -name '*.go' -not -path '*/vendor/*' | sort
434
orchestrator/server/docs/docs.go
orchestrator/server/docs/docs_test.go
orchestrator/server/integration/agent_sim_test.go
orchestrator/server/integration/android_restricted_settings_script_test.go
orchestrator/server/integration/batch_test.go
orchestrator/server/integration/dash_ws_test.go
orchestrator/server/integration/guacd_e2e_script_test.go
orchestrator/server/integration/guacd_e2e_test.go
orchestrator/server/integration/harness_test.go
orchestrator/server/integration/lifecycle_test.go
orchestrator/server/integration/protocol_test.go
orchestrator/server/integration/rbac_test.go
orchestrator/server/integration/screenshot_test.go
orchestrator/server/integration/team_inbox_test.go
orchestrator/server/integration/token_auth_test.go
orchestrator/server/integration/trigger_test.go
orchestrator/server/integration/workflow_test.go
orchestrator/server/internal/agent/capabilities.go
orchestrator/server/internal/agent/client.go
orchestrator/server/internal/agent/guac_relay.go
orchestrator/server/internal/agent/listener_coverage2_test.go
orchestrator/server/internal/agent/listener_extra_test.go
orchestrator/server/internal/agent/listener_gaps_test.go
orchestrator/server/internal/agent/listener.go
orchestrator/server/internal/agent/listener_platform_test.go
orchestrator/server/internal/agent/listener_test.go
orchestrator/server/internal/agent/listener_token_test.go
orchestrator/server/internal/agent/registry_capabilities_test.go
orchestrator/server/internal/agent/registry_extra_test.go
orchestrator/server/internal/agent/registry.go
orchestrator/server/internal/agent/registry_heartbeat_test.go
orchestrator/server/internal/agent/registry_link_test.go
orchestrator/server/internal/agent/registry_platform_test.go
orchestrator/server/internal/agent/registry_tags_test.go
orchestrator/server/internal/agent/registry_test.go
orchestrator/server/internal/agent/team_coverage_test.go
orchestrator/server/internal/agent/team_delivery_test.go
orchestrator/server/internal/agent/team.go
orchestrator/server/internal/agent/team_test.go
orchestrator/server/internal/agent/token_store.go
orchestrator/server/internal/agent/token_store_test.go
orchestrator/server/internal/agent/types.go
orchestrator/server/internal/assets/store.go
orchestrator/server/internal/assets/store_test.go
orchestrator/server/internal/config/config.go
orchestrator/server/internal/config/config_test.go
orchestrator/server/internal/handlers/agent.go
orchestrator/server/internal/handlers/agent_token.go
orchestrator/server/internal/handlers/agent_token_handler_test.go
orchestrator/server/internal/handlers/asset.go
orchestrator/server/internal/handlers/asset_test.go
orchestrator/server/internal/handlers/audit.go
orchestrator/server/internal/handlers/auth.go
orchestrator/server/internal/handlers/auth_test.go
orchestrator/server/internal/handlers/batch.go
orchestrator/server/internal/handlers/batch_test.go
orchestrator/server/internal/handlers/capstore.go
orchestrator/server/internal/handlers/capstore_test.go
orchestrator/server/internal/handlers/coverage_admin_test.go
orchestrator/server/internal/handlers/coverage_blank_test.go
orchestrator/server/internal/handlers/coverage_errors2_test.go
orchestrator/server/internal/handlers/coverage_errors_test.go
orchestrator/server/internal/handlers/coverage_gaps_test.go
orchestrator/server/internal/handlers/coverage_guards_test.go
orchestrator/server/internal/handlers/coverage_misc_test.go
orchestrator/server/internal/handlers/coverage_round3_test.go
orchestrator/server/internal/handlers/coverage_round4_test.go
orchestrator/server/internal/handlers/coverage_round5_test.go
orchestrator/server/internal/handlers/coverage_script_test.go
orchestrator/server/internal/handlers/coverage_workflow_test.go
orchestrator/server/internal/handlers/debugger.go
orchestrator/server/internal/handlers/debugger_test.go
orchestrator/server/internal/handlers/executions.go
orchestrator/server/internal/handlers/executions_test.go
orchestrator/server/internal/handlers/feedback.go
orchestrator/server/internal/handlers/guacamole_filesystem_test.go
orchestrator/server/internal/handlers/guacamole.go
orchestrator/server/internal/handlers/guacamole_mock_test.go
orchestrator/server/internal/handlers/guacamole_test.go
orchestrator/server/internal/handlers/handlers_test.go
orchestrator/server/internal/handlers/helpers.go
orchestrator/server/internal/handlers/integration_test.go
orchestrator/server/internal/handlers/messages.go
orchestrator/server/internal/handlers/metrics.go
orchestrator/server/internal/handlers/metrics_test.go
orchestrator/server/internal/handlers/profile.go
orchestrator/server/internal/handlers/profile_test.go
orchestrator/server/internal/handlers/rbac_test.go
orchestrator/server/internal/handlers/recordings.go
orchestrator/server/internal/handlers/recordings_test.go
orchestrator/server/internal/handlers/remote_fileops.go
orchestrator/server/internal/handlers/remote_fileops_test.go
orchestrator/server/internal/handlers/remote_session.go
orchestrator/server/internal/handlers/remote_session_test.go
orchestrator/server/internal/handlers/roles.go
orchestrator/server/internal/handlers/routes.go
orchestrator/server/internal/handlers/routes_test.go
orchestrator/server/internal/handlers/schemas.go
orchestrator/server/internal/handlers/screenshot.go
orchestrator/server/internal/handlers/script.go
orchestrator/server/internal/handlers/script_version_test.go
orchestrator/server/internal/handlers/serialization_test.go
orchestrator/server/internal/handlers/settings.go
orchestrator/server/internal/handlers/status.go
orchestrator/server/internal/handlers/tagstore.go
orchestrator/server/internal/handlers/tagstore_test.go
orchestrator/server/internal/handlers/teams.go
orchestrator/server/internal/handlers/teams_test.go
orchestrator/server/internal/handlers/trigger.go
orchestrator/server/internal/handlers/trigger_test.go
orchestrator/server/internal/handlers/update.go
orchestrator/server/internal/handlers/update_handler_test.go
orchestrator/server/internal/handlers/users.go
orchestrator/server/internal/handlers/vault.go
orchestrator/server/internal/handlers/vault_provider_test.go
orchestrator/server/internal/handlers/vault_test.go
orchestrator/server/internal/handlers/window.go
orchestrator/server/internal/handlers/workflow.go
orchestrator/server/internal/middleware/auth.go
orchestrator/server/internal/middleware/auth_test.go
orchestrator/server/internal/middleware/cors.go
orchestrator/server/internal/middleware/middleware_test.go
orchestrator/server/internal/middleware/ratelimit.go
orchestrator/server/internal/middleware/requestlog.go
orchestrator/server/internal/middleware/requestlog_test.go
orchestrator/server/internal/models/agent.go
orchestrator/server/internal/models/agent_token.go
orchestrator/server/internal/models/execution.go
orchestrator/server/internal/models/models.go
orchestrator/server/internal/models/models_test.go
orchestrator/server/internal/models/remote_session.go
orchestrator/server/internal/models/role.go
orchestrator/server/internal/models/update.go
orchestrator/server/internal/models/vault.go
orchestrator/server/internal/models/workflow.go
orchestrator/server/internal/rbac/rbac_bench_test.go
orchestrator/server/internal/rbac/rbac_extra_test.go
orchestrator/server/internal/rbac/rbac.go
orchestrator/server/internal/rbac/rbac_seed_test.go
orchestrator/server/internal/rbac/rbac_test.go
orchestrator/server/internal/remoteticket/ticket.go
orchestrator/server/internal/remoteticket/ticket_test.go
orchestrator/server/internal/scripts/inline.go
orchestrator/server/internal/scripts/inline_test.go
orchestrator/server/internal/scripts/paths.go
orchestrator/server/internal/scripts/paths_test.go
orchestrator/server/internal/security/password.go
orchestrator/server/internal/security/password_test.go
orchestrator/server/internal/security/vault.go
orchestrator/server/internal/security/vault_test.go
orchestrator/server/internal/vault/local.go
orchestrator/server/internal/vault/local_test.go
orchestrator/server/internal/vault/registry.go
orchestrator/server/internal/vault/vault.go
orchestrator/server/internal/workflow/engine_bench_test.go
orchestrator/server/internal/workflow/engine_cond_test.go
orchestrator/server/internal/workflow/engine_cov_test.go
orchestrator/server/internal/workflow/engine_exec_record_test.go
orchestrator/server/internal/workflow/engine_extra_test.go
orchestrator/server/internal/workflow/engine.go
orchestrator/server/internal/workflow/engine_requires_test.go
orchestrator/server/internal/workflow/engine_select_test.go
orchestrator/server/internal/workflow/engine_test.go
orchestrator/server/internal/workflow/exec_record.go
orchestrator/server/internal/workflow/templates.go
orchestrator/server/internal/workflow/templates_test.go
orchestrator/server/main_coverage_test.go
orchestrator/server/main.go
orchestrator/server/main_test.go
orchestrator/server/main_wait_test.go
orchestrator/server/pkg/websocket/hub_count_test.go
orchestrator/server/pkg/websocket/hub_extra_test.go
orchestrator/server/pkg/websocket/hub.go
orchestrator/server/pkg/websocket/hub_test.go
orchestrator/server/shutdown_signal_unix_test.go
orchestrator/server/shutdown_signal_windows_test.go
orchestrator/server/skip_notwindows_test.go
orchestrator/server/skip_windows_test.go
[0m$ [0mgrep -RInE 'TODO|FIXME|XXX|not implemented|未实现|暂不支持|规划中' --exclude-dir=third_party --exclude-dir=vcpkg_installed --exclude-dir=.git --include='*.go' --include='*.cpp' --include='*.hpp' --include='*.rs' --include='*.kt' --include='*.md' . | head -200
findSlider/findSpinner/findProgressBar | 查无，已搜：全仓 `grep -rn "findCheckBox"`（*.cpp/hpp/h/lua/pyi/ts/js/md，排除 build、vcpkg_installed、node_modules）仅命中本文档自身与 docs/.vitepress/dist 构建产物；uia 模块实注册仅 13 函数（lib/wingman/src/script/modules/misc_modules.cpp:466-580）；权威 API 页 docs/api/uia/index.md:5 的「已实现」清单亦无任何 find*控件型函数 | 超前 | P0 | 15 项勾选改回未完成，或删除 Phase 19 段；docs/api/uia/ 下 checkbox.md 等逐控件页同病，需一并处理 |
./docs/审计-文档一致性.md:164:| J-TRA-01 | libs/transport/README.md:3,15 | 「分层传输架构，提供 TCP/WebSocket 通信能力」；架构图「Transport (传输层) ← TCP/WebSocket」 | src/transport_client.cpp:5-11 与 src/transport_server.cpp:5-11：`create(TransportType::WebSocket)` 返回 `nullptr`；tests/stream_transport_test.cpp:1336-1338 `WebSocketTypeUnsupported` 断言两侧均为 nullptr。已搜 libs/transport 全部 *.hpp/*.cpp「websocket」（大小写不敏感）：仅 transport.hpp:23 枚举占位与本测试 | 超前 | P0 | README 明示「仅 TCP；WebSocket 为枚举占位未实现」，或删去 WebSocket 表述 |
./docs/审计-文档一致性.md:168:| A-D06 | docs/README.md:74 | ML/YOLO「目标检测高层 API 仍在规划中」 | 已落地：lib/wingman/src/script/modules/vision_module.cpp:73 `aiSetup`、:110 `aiSetupLocal`、:130 `aiLocate`、:151 `aiElements`；lib/wingman/src/vision_ai.cpp、ml_postprocess.cpp；Android 同形（android_script_api.cpp:384-466）；对应提交 1f1ab81/0c782df/b2343b1 | 缺失 | P1 | 更新为「AI 视觉检测 API 已可用（云端 provider + 本地 ONNX）」 |
./docs/审计-文档一致性.md:203:| E-CB-1 | clipboard.md:3,11,281-303 | 「支持文本、HTML、图像和文件列表」「图像剪贴板 - 设置/读取/检测位图图像」（无任何平台限定） | Linux 后端 lib/wingman/src/platform/linux/x11_clipboard.cpp:159-183：`setImage` 恒 `return false`（注释 "not implemented in MVP"）、`hasImage` 恒 false、`getImage` 恒空；HTML 为纯文本透传（:159-168 setHTML→setText、getHTML→getText）。Windows（win32_clipboard.cpp:91+）与 macOS（cocoa_clipboard.cpp:25+）为完整实现。文档的图像能力在 Linux 面不成立 | 超前 | P1 | 文档补平台支持表（同 hotkey.md 风格）：图像=Win/macOS、HTML 在 Linux 为纯文本降级 |
./docs/审计-文档一致性.md:213:| E-SEC-3 | security.md:60-75,335 | 「verify_integrity() 验证程序完整性」「返回: 是否未被篡改」 | lib/wingman/src/security.cpp:364-373：默认 `enableIntegrityCheck=false`（lib/wingman/include/wingman/security.hpp:26）时**恒返回 true**；开启后走 `spdlog::warn("verifyIntegrity: integrity checking is not yet implemented … Returning false.")` 恒 false。完整性校验本体未实现 | 超前 | P1 | 文档注明「当前为占位实现：默认恒 true，显式开启后恒 false 并打警告日志」 |
./docs/审计-文档一致性.md:245:| G-T8 | 25 | 动作 `type`：… `lua_script` …（字段 `script`） | 动作类型解析存在（misc_modules.cpp:80 `lua_script → LUA_SCRIPT`），但执行为 no-op：lib/wingman/src/smart_trigger.cpp:175-177 `case TriggerActionType::LUA_SCRIPT: spdlog::debug("… executing Lua script"); break;` 不执行任何脚本 | 超前 | P1 | 文档标注 `lua_script` 当前仅解析未实现执行，或删去该类型直至实现 |
./docs/审计-文档一致性.md:257:| H-AR5 | architecture.md:265-273 | 核心能力模块文件表：Input→`src/input.cpp`、Process→`src/process.cpp`、Recorder→`src/recorder.cpp`、Trigger→`src/trigger.cpp`、QRCode→`src/qrcode.cpp` | lib/wingman/src/ 下无 input.cpp/process.cpp/recorder.cpp/trigger.cpp/qrcode.cpp（ls 证实）；实际：输入在 src/platform/（如 platform/win/sendinput_input.cpp）、进程在 platform/{win,linux,mac}/*_process.cpp、录制在 platform/*_recorder.cpp + script/modules/macro_module.cpp、触发在 trigger_engine.cpp/smart_trigger.cpp/platform/*_trigger.cpp；QR 全仓无实现，仅存未定义声明 libs/lua/include/wingman/lua/lua_bindings.hpp:64 `void registerQRCodeModule();` | 不符 | P1 | 按真实文件改表；QRCode 行删除或标注未实现 |
./docs/审计-文档一致性.md:280:| I-D-15 | game-automation-coverage.md:114-121（Orchestration/Team Gaps） | 缺口列表：「Server-arbitrated team voting / Member state model / Runtime inbox controlled by server / Team event broadcast …」 | server 侧多数已实现：`orchestrator/server/internal/agent/team.go:222` CreateVote（votes 表）；`listener.go:492-499` team.vote_create/vote_cast/status_report/broadcast、`listener.go:486-491` inbox.register/heartbeat/ack/report、`listener.go:846-854` inbox.register_ack/heartbeat_ack；`team_test.go` 11 例。仍未实现（该两项 gap 仍成立）：task assignment/lease（team.go 无 Task/Assign/lease 命中）、Dashboard 团队视图（`orchestrator/dashboard/src` 无 team/vote/inb … | 缺失 | P1 | Gap 列表收窄为「任务分派/lease、Dashboard 团队视图」，已实现的投票/inbox/广播移出缺口 |
./docs/审计-文档一致性.md:290:| J-TODO-02 | docs/development-todo.md:341 | 9.3 `[x]` `:rightClick()` - 右键点击 | UIElement 脚本对象 12 方法无 right_click（misc_modules.cpp:375-463：get_info/click/double_click/focus/get_value/set_value/get_children/expand/collapse/is_expanded/is_visible/is_enabled）；接口 IUIAElement 亦无 rightClick（lib/wingman/include/wingman/ui_automation.hpp:105-133）。已搜 rightClick 全仓仅命中 human 模块（HumanMouse::rightClick，无关） | 超前 | P1 | 勾掉该项或补实现 |
./docs/审计-文档一致性.md:291:| J-TODO-03 | docs/development-todo.md:558,561 | Phase 14 `[x]` 「实现选择项操作 (selectItem/getSelection)」；「Lua 绑定更新 (getParent, expand, collapse, isExpanded, selectItem, getSelection)」 | selectItem/getSelection/getParent 全仓查无（同上搜索口径，lib/wingman/src、include、tests 零命中）；expand/collapse/is_expanded 存在（misc_modules.cpp:434-457） | 超前 | P1 | selectItem/getSelection/getParent 三项取消勾选；expand 组保留 |
./docs/审计-文档一致性.md:292:| J-TODO-04 | docs/development-todo.md:329-337,340-348,580-582 | 9.2/9.3/Phase 17 以 camelCase 记 API：`uia.fromForeground()`、`uia.findByName`、`:getValue()`、`uia.onPropertyChanged` 等 | Lua 面实为 snake_case：misc_modules.cpp:469-521 注册 `from_foreground/from_point/from_window/find_by_name/find_by_id/wait_for_name/find_button/find_edit/find_text`，:530-575 注册 `on_property_changed/on_structure_changed/remove_event_listener`；module_registry.cpp 无 camelCase 别名/归一逻辑（已搜 alias/camel/normalize 零命中）；docs/api/uia/index.md:5 权威清单全用 snake_case。照 todo 调 `uia.fromForeground()` 得 nil | 不符 | P1 | todo 的 uia 条目统一改 snake_case 真名 |
./docs/审计-文档一致性.md:326:| D-V16 | orchestration.md:36-40,244-255 | 提交参数 `workflow: {name, steps, timeout, metadata}`；工作流对象含 `result`(any)、`error`(string)、`createdAt`(number)、`updatedAt`(number) | C++ 侧为纯 stub（`orchestration_module.cpp:11-26`，恒 null/空/false），无任何对象产出；Go 面 `orchestrator/server/internal/handlers/workflow.go:29-65` 创建请求为 `{name, description, steps, sharedContext}`（无顶层 `timeout`/`metadata`），详情 DTO 字段为 `id/name/description/status/steps/sharedContext/stepStatus/createdTime/startTime/endTime`——无 `result`/`error`/`createdAt`/`updatedAt`（时间字段叫 `createdTime`，毫秒 int64；gorm.Model 序列化为大写 `CreatedAt`/`Update … | 超前 | P2 | 该节标注「规划中/以 Go API 文档为准」，字段表改为与 `WorkflowDetailData` 一致，或删去具体字段承诺 |
./docs/审计-文档一致性.md:347:| G-UIA-D12 | 3 | 「允许你直接与 Windows 应用程序的 UI 控件交互」 | 另有 macOS Accessibility 后端：lib/wingman/src/platform/mac/mac_automation.cpp:630 `createUIAManager()`（Windows：lib/wingman/src/platform/win/win_automation.cpp:666）；Linux 无后端：lib/wingman/src/ui_automation.cpp:31-33 warn + return false。docs/api/uia/index.md:5 自述「Windows UIAutomation + macOS Accessibility 双平台（Linux 暂不支持）」 | 缺失 | P2 | 补一句 macOS 支持、Linux 不可用 |
./docs/审计-文档一致性.md:380:| J-TODO-05 | docs/development-todo.md:346 | 9.3 `[x]` `:getName()` - 获取名称 | UIElement 12 方法无 get_name（misc_modules.cpp:375-463）；名称需经 `get_info().name` 取得（:383-397） | 超前 | P2 | 改记为 `get_info().name` 或补 get_name 方法 |
./docs/审计-文档一致性.md:381:| J-TODO-06 | docs/development-todo.md:135 | A1 `[x]` 「`android_agent.cpp` 支持 run_script/stop_script/screenshot.capture/system.shutdown 四命令」 | apps/android/cpp/agent/android_agent.cpp:183-186：`system.shutdown` 显式返回 `CommandResult::error("system.shutdown not supported on android agent")`（注释「不响应远端关停」）；实际支持三命令 | 不符 | P2 | 措辞改为「三命令 + shutdown 显式拒绝」 |
./docs/审计-文档一致性.md:382:| J-TODO-07 | docs/development-todo.md:29,62,70 | 头注「event/fsm/task/notify 各 38/44/42/39 条用例」；`wingman.task`（42 条测试）；`wingman.notify`（39 条测试） | 实测 `grep -c "^TEST"`：task_module_test.cpp=47、notify_module_test.cpp=46（pause/resume 2026-10-01、tray 2026-10-04 落地后增长，文档未同步）；event 50（正文 :44 正确、头注 38 亦过期）、fsm 44 ✓、timer 12 ✓ | 不符 | P2 | 计数同步或去数字化（改「XX+ 条」） |
./docs/审计-文档一致性.md:383:| J-TODO-08 | docs/development-todo.md:284-288 | Phase 7.3 `[x]` 「代码签名 (验证支持)」「混淆支持 (字符串加密、哈希)」 | security_module.cpp:12-41 全部 8 函数为 getRandomDelay/getRandomOffset/isDebuggerPresent/isRunningInVM/verifyIntegrity/hashString/generateRandomString/filterSensitive：无代码签名验证（verifyIntegrity 是完整性自检）、无字符串加密（哈希 ✓、反调试/反VM 检测 ✓）。已搜 lib/wingman 无签名校验入口 | 超前 | P2 | 两项措辞收敛为「完整性自检」「哈希」，或标注未实现部分 |
./docs/审计-文档一致性.md:384:| J-TODO-09 | docs/development-todo.md:588 | Phase 18 `[x]` 「更新 `docs/api/uia.md` 添加事件监听器 API 文档」 | 查无 docs/api/uia.md；实际为目录 docs/api/uia/（index.md 承载事件监听节，:862「## 事件监听」）。已搜 ls docs/api/ 全列 | 不符 | P2 | 路径改 docs/api/uia/index.md |
./docs/agent-token-auth-design.en.md:197:   - When an agent has not implemented challenge, it continues to use the §2.1 plaintext
./docs/agent-token-auth-design.en.md:232:3. **TLS deployment form** (not implemented—decision between two forms, see below):
./docs/platforms.en.md:62:| `wingman.clipboard` (clipboard read/write) | ✅ | ✅ X11 | ✅ Cocoa | ❌ not implemented (no such API surface on the A line) | 📋 |
./docs/platforms.en.md:63:| `wingman.filewatcher` (file change monitoring) | ✅ ReadDirectoryChangesW | ✅ inotify | ✅ FSEvents | ❌ not implemented | 📋 |
./docs/platforms.en.md:64:| `wingman.process` (process enumerate/start/terminate) | ✅ Win32 | ✅ POSIX | ✅ POSIX | ❌ not implemented | 📋 |
./docs/platforms.en.md:65:| `wingman.macro` (macro record/playback) | ✅ system hooks | ✅ XRecord | ✅ Cocoa events | ❌ not implemented | 📋 |
./docs/platforms.en.md:66:| `wingman.uia` (UI element tree automation) | ✅ UI Automation | ❌ **no backend**: Linux has no equivalent framework; calls degrade to returning failure with a warning | ✅ AXUIElement | ❌ not implemented | 📋 |
./docs/platforms.en.md:67:| `wingman.hotkey` (global hotkey listening) | ✅ GetAsyncKeyState | ✅ XQueryKeymap (X11) | ⚠️ CGEventSourceKeyState requires Accessibility permission; real behavior pending on-device verification | ❌ not implemented | 📋 |
./docs/platforms.en.md:210:[iOS Support Design](ios-agent-design.md) (I1, design only, not implemented); its constraints determine that it is "planned":
./docs/mobile-automation-design.md:199:> takeScreenshot 兜底通道与两通道聚合均**未实现（计划中）**，属本设计的
./docs/mobile-automation-design.md:214:计划项，尚未实现。）
./docs/mobile-automation-design.md:427:| **D3 兜底采集**（计划中，未实现） | takeScreenshot（API 30+，限流 ~1Hz）兜底通道 + `AndroidCaptureSource` 两通道聚合（见 D3 状态注） | 投屏未授权时低频找色找图脚本仍可用；capabilities 上报当前激活通道 |
./docs/guides/yolo-guide.md:112:    dynamic=False          # 动态输入尺寸（Wingman 暂不支持）
./docs/guides/android-restricted-settings.en.md:118:> device.capabilities and is registered separately as not implemented.
./docs/guides/uia-guide.md:5:Wingman 的 UI Automation (UIA) 模块允许你直接与 Windows（UIAutomation）/ macOS（Accessibility）应用程序的 UI 控件交互，无需依赖坐标点击。Linux 平台暂不支持。
./docs/guides/uia-guide.md:732:    # （select() 未实现（计划中），选中列表项的现役替代是 click）
./docs/guides/uia-guide.md:747:    -- （select() 未实现（计划中），选中列表项的现役替代是 click）
./docs/guides/uia-guide.md:804:> **`set_toggle_state()` 未实现（计划中）**。现役替代：`click()` 只能在选中/未选中间切换；"不确定"状态脚本层暂无法设置。
./docs/guides/uia-guide.md:943:- `click()` - 点击/执行菜单项（`invoke()` 未实现（计划中），菜单项用 `click()`）
./docs/guides/uia-guide.md:1328:> `select()` 未实现（计划中）。选中列表项/单选按钮的现役替代是 `click()`。
./docs/guides/triggers.md:27:> **动作 `type`**：`click` / `key_press` / `wait` / `lua_script` / `log` / `stop`（字段：`x` / `y` / `key` / `keyCode` / `waitMs` / `script` / `message`；`key`/`keyCode` 为 vkCode 整数，如 `0x31` = "1"，传字符串会按 0 处理）。注意 `lua_script` 动作当前仅解析、未实现执行。
./docs/development-todo.md:75:  - [x] `trayShow()/trayHide()/traySetBadge(text?)/traySetTooltip(text?)`（2026-10-04 落地：向 EventHub 发 `notify.tray.*` 意图事件，runtime notify_bridge 转投 EventBuffer，GUI 经 tauri `tray_control` 驱动系统托盘；badge 因 tauri v2 TrayIcon 无角标 API 暂不支持——GUI 端返回错误走前端日志（2026-10-05 nightly 实证修正），待上游支持或自绘方案再接）
./docs/development-todo.md:284:- [x] 完整性自检 (verifyIntegrity；代码签名验证未实现)
./docs/development-todo.md:287:- [x] 哈希 (hashString；字符串加密未实现)
./docs/development-todo.md:574:    - [x] 示例配置 schema 对账（2026-10-15 第九批，修三处真漂移）：examples/configs/ 与 examples/profiles/ 对实现 schema 核验。①`examples/configs/config.json` 含完整 `tray` 节（menuItems/minimizeToTray/tooltip 等）——`TrayConfig` 全源码零命中（lib/apps 非测试面），config.md 已明写「配置结构中没有 tray 节、未实现（计划中）」——删 tray 节使示例回归文档化 schema。②`examples/profiles/example_game/profile.json` window 两键名漂移：`process`→`processName`、`exact_match`→`exactMatch`（JSON 序列化面真值，`game_profile.cpp:20/21/94/95`；INI 面才是 `process`/`exact_match` snake 键，`:514/515`，两词典并行已实证）；colors/images/triggers/scripts/settings 六节与 struct 键全对。③**重写** `examples/configs/triggers.lua`：原文件为全局 `triggers` 数据表、无任何加载器消费（全仓 grep 仅 GUI 测试按脚本名引用概念），且字段词汇全面失真——condition `type="pixel"`（真值 COLOR_FOUND 等十类）、`color="0x00FF00"` 字符串（`toColor` 仅收 int 0xRRGGBB 或 {r,g,b}，字符串静默退化 Color() 零值）、`region={100,100,50,50}` 位置数组（`toRect` 仅收 {x,y,width,height} 对象，数组静默退化零 Rect）、action `type="key"`（真值 KEY_PRESS 等六类）、`key="1"` 字符串（真值 int 键码）、`cooldown` 与 `enabled` 两字段不存在（真值：冷却=WAIT 动作+setCheckInterval、禁用=未 start）——重写为可运行 smarttrigger 脚本（create→addCondition/addAction→setCheckInterval→start→isRunning），保留原三触发器意图（技能冷却/血量/蓝量），`luac -p` 通过。核验：两 JSON `json.load` 键集与 config.md/GameProfile 结构一致、triggers.lua 语法通过、既有 `game_profile_io_test`/`game_profile_test` 覆盖两词典面不受影响（commit d522dd3）docs 根 24 页 + guide 页全部反引号路径以多根解析（repo 根/orchestrator{,server,dashboard,dashboard/src}/lib/wingman/apps{agent,gui,android,android/cpp}/libs/examples/docs）核验。①`android-agent.md` 环境表「工具链 `scripts/buildsystems/vcpkg.cmake`」为仓内不存在的假路径（scripts/ 下无任何 .cmake）——真值：NDK 构建经仓内 `apps/android/cpp/vcpkg-android.cmake` 包装，gradle 传 `-DWINGMAN_VCPKG_ROOT=<vcpkg 根>`，`scripts/buildsystems/vcpkg.cmake` 属 vcpkg 自带路径（`apps/android/cpp/CMakeLists.txt:4`、`build.gradle.kts:37-38`、`vcpkg-android.cmake:27`）——已改并注明归属。②`dependencies.md` 「`cpp/vcpkg-android.cmake`」缺 apps/android 前缀（仓根无 cpp/）——补全为 `apps/android/cpp/vcpkg-android.cmake`。③非缺陷逐条核销：DEVELOPER_TOOLS.md `luarocks/` = install 脚本安装目标（`install-luarocks.cmd` 实存）、DEVELOPMENT.md `build-tests/` = 示例命令建议目录名、architecture-decisions.md `apps/runtime` = 2026-10-09 改名历史记录、design 文档 `internal/…`/`platform/<os>/`/`services/…` = 模块根相对设计正文、architecture.md `/stop-script` = IPC 方法名、project-structure.md `xxx.hpp` = 占位。**连带登记**：`build-scripts/` 24 文件正被并行席位在途删除（HEAD 仍跟踪），届时 development-environment.md 2 处、guide/structure.md 3 处、guide/runtime-gui.md 3 处引用同批成漂，待其落地后一起改，本席不抢examples/configs/ 与 examples/profiles/ 对实现 schema 核验。①`examples/configs/config.json` 含完整 `tray` 节（menuItems/minimizeToTray/tooltip 等）——`TrayConfig` 全源码零命中（lib/apps 非测试面），config.md 已明写「配置结构中没有 tray 节、未实现（计划中）」——删 tray 节使示例回归文档化 schema。②`examples/profiles/example_game/profile.json` window 两键名漂移：`process`→`processName`、`exact_match`→`exactMatch`（JSON 序列化面真值，`game_profile.cpp:20/21/94/95`；INI 面才是 `process`/`exact_match` snake 键，`:514/515`，两词典并行已实证）；colors/images/triggers/scripts/settings 六节与 struct 键全对。③**重写** `examples/configs/triggers.lua`：原文件为全局 `triggers` 数据表、无任何加载器消费（全仓 grep 仅 GUI 测试按脚本名引用概念），且字段词汇全面失真——condition `type="pixel"`（真值 COLOR_FOUND 等十类）、`color="0x00FF00"` 字符串（`toColor` 仅收 int 0xRRGGBB 或 {r,g,b}，字符串静默退化 Color() 零值）、`region={100,100,50,50}` 位置数组（`toRect` 仅收 {x,y,width,height} 对象，数组静默退化零 Rect）、action `type="key"`（真值 KEY_PRESS 等六类）、`key="1"` 字符串（真值 int 键码）、`cooldown` 与 `enabled` 两字段不存在（真值：冷却=WAIT 动作+setCheckInterval、禁用=未 start）——重写为可运行 smarttrigger 脚本（create→addCondition/addAction→setCheckInterval→start→isRunning），保留原三触发器意图（技能冷却/血量/蓝量），`luac -p` 通过。核验：两 JSON `json.load` 键集与 config.md/GameProfile 结构一致、triggers.lua 语法通过、既有 `game_profile_io_test`/`game_profile_test` 覆盖两词典面不受影响
./docs/development-todo.md:576:    - [x] 示例与文档代码块 API 面对账（2026-10-15 第八批）：examples/ 全部 33 个脚本（29 lua + 其余）+ docs 全部 82 个含 ```lua/```python 栅栏的文件，抽 `wingman.mod.func(` 直接调用与 `local X = wingman.mod` 局部别名解析后的 `X.func(` 调用，对**双端注册面**核验——桌面面 = `lib/wingman/src/script/modules/*.cpp` 的 `functions.push_back` + Python camelToSnake 别名，Android 面 = `libs/androidagent/src/android_script_api.cpp` 的 `set_function`（input.{click,tap,longPress,swipe,delay}、screen.*、vision.*、trigger.{add,start,stop,remove,startAll,stopAll,isRunning,getTriggerCount,count}）。**零死调用**。首轮 8 个候选死 API（android-agent-design.md:285 `wingman.input.swipe`、343/358-63 `wingman.trigger.*` 六个）经核全部为 Android 面真实注册（swipe 在 android_script_api.cpp:103、trigger 族 :509-637）——首轮误报系比对面只含桌面注册、漏 Android 面所致，比对面补齐后全清。AUDIT-REPORT.md 等台账历史文本不计入。本批零修复，仅记台账；附带 examples 全部 29 个 lua 脚本 `luac -p`（5.4.8）语法核验 0 失败examples/configs/ 与 examples/profiles/ 对实现 schema 核验。①`examples/configs/config.json` 含完整 `tray` 节（menuItems/minimizeToTray/tooltip 等）——`TrayConfig` 全源码零命中（lib/apps 非测试面），config.md 已明写「配置结构中没有 tray 节、未实现（计划中）」——示例与文档互相矛盾且都非真值，删 tray 节使示例回归文档化 schema。②`examples/profiles/example_game/profile.json` window 两键名漂移：`process`→`processName`、`exact_match`→`exactMatch`（JSON 序列化面真值，`game_profile.cpp:20/21/94/95`；INI 面才是 `process`/`exact_match` snake 键，`:514/515`，两词典并行已实证）；colors/images/triggers/scripts/settings 六节与 struct 键全对。③**重写** `examples/configs/triggers.lua`：原文件为全局 `triggers` 数据表、无任何加载器消费（全仓 grep 仅 GUI 测试按脚本名引用概念），且字段词汇全面失真——condition `type="pixel"`（真值 COLOR_FOUND 等十类）、`color="0x00FF00"` 字符串（`toColor` 仅收 int 0xRRGGBB 或 {r,g,b}，字符串静默退化 Color() 零值）、`region={100,100,50,50}` 位置数组（`toRect` 仅收 {x,y,width,height} 对象，数组静默退化零 Rect）、action `type="key"`（真值 KEY_PRESS 等六类）、`key="1"` 字符串（真值 int 键码）、`cooldown` 与 `enabled` 两字段不存在（真值：冷却=WAIT 动作+setCheckInterval、禁用=未 start）——重写为可运行 smarttrigger 脚本（create→addCondition/addAction→setCheckInterval→start→isRunning），保留原三触发器意图（技能冷却/血量/蓝量），`luac -p` 通过。核验：两 JSON `json.load` 键集与 config.md/GameProfile 结构一致、triggers.lua 语法通过、既有 `game_profile_io_test`/`game_profile_test` 覆盖两词典面不受影响examples/ 全部 33 个脚本（29 lua + 其余）+ docs 全部 82 个含 ```lua/```python 栅栏的文件，抽 `wingman.mod.func(` 直接调用与 `local X = wingman.mod` 局部别名解析后的 `X.func(` 调用，对**双端注册面**核验——桌面面 = `lib/wingman/src/script/modules/*.cpp` 的 `functions.push_back` + Python camelToSnake 别名，Android 面 = `libs/androidagent/src/android_script_api.cpp` 的 `set_function`（input.{click,tap,longPress,swipe,delay}、screen.*、vision.*、trigger.{add,start,stop,remove,startAll,stopAll,isRunning,getTriggerCount,count}）。**零死调用**。首轮 8 个候选死 API（android-agent-design.md:285 `wingman.input.swipe`、343/358-363 `wingman.trigger.*` 六个）经核全部为 Android 面真实注册（swipe 在 android_script_api.cpp:103、trigger 族 :509-637）——首轮误报系比对面只含桌面注册、漏 Android 面所致，比对面补齐后全清。AUDIT-REPORT.md 等台账历史文本不计入。本批零修复，仅记台账；附带 examples 全部 29 个 lua 脚本 `luac -p`（5.4.8）语法核验 0 失败
./docs/api/debugger.md:5:> [调试器尚未实现，当前为 stub。`start` 恒返回 false。完整调试 API（断点管理/单步/求值/堆栈）规划中。]
./docs/api/debugger.md:37:**说明**：启动调试器（stub，未实现，恒返回 `false`）。
./docs/api/debugger.md:60:print(ok)  # False（未实现）
./docs/api/debugger.md:69:print(ok)  -- false（未实现）
./docs/api/notify.md:398:# notify.traySetBadge("3")         # 角标意图：GUI 端暂不支持（tauri v2 无角标 API）
./docs/api/notify.md:409:-- wingman.notify.traySetBadge("3") -- 角标意图：GUI 端暂不支持（tauri v2 无角标 API）
./docs/api/notify.md:430:| `traySetBadge(text?)` | `traySetBadge(text?)` | 托盘角标意图（GUI 端暂不支持） | text: 角标文本(省略清除) |
./docs/api/vision.md:492:系统性偏移）；分割（segment）未实现，诚实报错返回空位图。
./docs/api/AUDIT-REPORT.en.md:15:  - 3.1 文档描述的能力代码未实现（文档有/代码无）
./docs/api/security.md:64:**说明**：验证程序完整性。**当前为占位实现**：默认（完整性检查开关未开启）恒返回 `True`/`true`；显式开启后走未实现分支，恒返回 `False`/`false` 并打警告日志。不能据此判断程序是否被篡改。
./docs/api/ml.md:493:以下能力在文档历史版本中曾以 `Model` 对象形式描述，但当前脚本层尚未实现：
./docs/api/serialize.md:12:| **YAML** | *(规划中)* | 更易读、支持注释 | 复杂配置、人工编辑的文件 |
./docs/api/serialize.md:56:**示例（规划中）：**
./docs/api/serialize.md:162:### 使用 YAML（规划中）
./docs/api/serialize.md:206:- yaml 模块（规划中）
./docs/api/uia/index.md:7:> [**已实现**：根元素获取（`from_foreground`/`from_window`/`from_point`）+ UI 树导出（`tree`）+ 通用查找（`find_by_name`/`find_by_id`/`find_all_by_control_type`/`wait_for_name`/`wait_for_id`/`wait_for_role`/`wait_for`）+ 专用查找（`find_button`/`find_edit`/`find_text`/`find_check_box`/`find_radio_button`/`find_combo_box`/`find_list`/`find_list_item`/`find_tree`/`find_tree_item`/`find_menu_item`/`find_hyperlink`/`find_image`/`find_slider`/`find_spinner`/`find_progress_bar`/`find_tab`/`find_tab_item`）+ UIElement 对象 18 方法（`get_info`/`click`/`right_click`/`double_click`/`focus`/`get_value`/`set_value`/`get_children`/`get_parent`/`expand`/`collapse`/`is_expanded`/`is_visible`/`is_enabled`/`select`/`get_selection`/`is_checked`/`set_checked`）+ 事件监听（`on_property_changed`/`on_structure_changed`/`remove_event_listener`）。支持 **Windows UIAutomation（COM 事件处理线程）+ macOS Accessibility（AXObserver run loop）** 双平台（Linux 暂不支持）。]
./docs/api/uia/progressbar.md:44:> **未实现（计划中）**：`get_info()` 的返回键中**没有数值进度字段**（无 `value`/`minimum`/`maximum`）；`get_value()` 返回的是元素文本（实现为 `getText`），不是数值进度。若应用恰好把进度写进元素文本/名称（如 "80%"），可经 `get_value()` 读取后自行解析，但这不是保证存在的形态。
./docs/api/uia/scrollbar.md:53:> **未实现（计划中）**：`get_info()` 的返回键中没有滚动数值字段（无 `value`），`get_value()` 返回的是元素文本（实现为 `getText`），不是滚动位置。滚动位置读取暂不可用。
./docs/api/uia/scrollbar.md:59:> **未实现（计划中）**：`set_value(number)` 设置滚动位置的能力未实现——`set_value` 的实现是设置元素文本（`setText`），对滚动条调用不会改变滚动位置。滚动请改用 `input.scroll(x, y, delta)`（模拟鼠标滚轮，input 模块）。
./docs/api/uia/scrollbar.md:87:> **未实现（计划中）**：`get_info()` 的返回键中没有 `minimum`/`maximum` 字段，滚动范围读取暂不可用。
./docs/api/uia/scrollbar.md:101:> **未实现（计划中）**：滚动位置/范围的读取（`get_value` 数值语义、`minimum`/`maximum` 字段）与设置（`set_value` 数值语义）均未实现——`get_value`/`set_value` 实为读取/设置元素文本，仅对文本型控件有效。
./docs/api/uia/slider.md:42:> **未实现（计划中）**：`get_info()` 的返回键中没有数值字段（无 `value`），`get_value()` 返回的是元素文本（实现为 `getText`），不是滑块数值。滑块当前值读取暂不可用。
./docs/api/uia/slider.md:77:> **未实现（计划中）**：`set_value(number)` 设置滑块值的能力未实现——`set_value` 的实现是设置元素文本（`setText`），对滑块调用不会拖动滑块或改变数值。调值需等待 Range/Value 模式接口补齐；过渡方案可用 `input.scroll()` 或拖拽模拟。
./docs/api/uia/slider.md:107:> **未实现（计划中）**：`get_info()` 的返回键中没有 `minimum`/`maximum` 字段，滑块范围读取暂不可用。
./docs/api/uia/slider.md:121:> **未实现（计划中）**：滑块值/范围的读取（`get_value` 数值语义、`minimum`/`maximum` 字段）与设置（`set_value` 数值语义）均未实现——`get_value`/`set_value` 实为读取/设置元素文本，仅对文本型控件有效。
./docs/api/uia/checkbox.md:168:> `set_toggle_state(state)`（'On'/'Off'/'Indeterminate' 三态显式建模）尚未实现，Indeterminate 态暂不可直接设置——两态场景请用 `set_checked(bool)`。当前可用操作：`set_checked` / `click()` 切换与通用方法（见 [概述](./index.md#uielement-通用方法)）。
./docs/api/uia/checkbox.md:279:> 勾选状态读取与设置请用 `is_checked` / `set_checked`（见上节）；`get_value`/`set_value` 实为读取/设置元素文本，仅对文本型控件有效；三态 `set_toggle_state` 未实现。
./docs/api/uia/checkbox.en.md:279:> Use `is_checked` / `set_checked` to read and set the checked state (see above); `get_value`/`set_value` actually read/set the element text and only work for text-type controls; the three-state `set_toggle_state` is not implemented.
./docs/api/uia/slider.en.md:121:> **Not implemented (planned)**: Reading the slider value/range (`get_value` numeric semantics, `minimum`/`maximum` fields) and setting it (`set_value` numeric semantics) are both not implemented — `get_value`/`set_value` actually read/set the element text and only work for text-type controls.
./docs/api/uia/scrollbar.en.md:101:> **Not implemented (planned)**: Reading the scroll position/range (`get_value` numeric semantics, `minimum`/`maximum` fields) and setting it (`set_value` numeric semantics) are both not implemented — `get_value`/`set_value` actually read/set the element text and only work for text-type controls.
./docs/api/AUDIT-REPORT.md:39:- **[标TODO]**：计划要做的功能 → 保留文档并明确标注"未实现/规划中"
./docs/api/AUDIT-REPORT.md:44:### 3.1 文档描述的能力代码未实现（文档有/代码无）
./docs/api/AUDIT-REPORT.md:51:| debugger | `debugger.md`+`debugging.md` 合计 ~19 函数，代码仅 4 个 stub | [标TODO] stub 返回 false | debugger_module.cpp:13-30 |
./docs/api/AUDIT-REPORT.md:53:| ml | `detect`/`classify`（目标检测/分类） | [标TODO] 文档已自注待补充 | ml_module.cpp |
./docs/api/AUDIT-REPORT.md:137:- **D2 "文档有/代码无"的功能归属**：human/uia/ml 两套 API + util/config/kv/vision/ocr/http/fsm/screen/input 的大量未实现函数。统一判断为 (a) 废弃→删文档，还是 (b) 计划中→标 TODO 保留。
./docs/api/debugger.en.md:37:**Description**: Starts the debugger (stub, not implemented, always returns `false`).
./docs/api/debugger.en.md:60:print(ok)  # False（未实现）
./docs/api/debugger.en.md:69:print(ok)  -- false（未实现）
./docs/api/clipboard.md:13:| 图像 | 完整 | 完整 | **未实现**（setImage 恒返回 false，hasImage 恒 false，getImage 恒空） |
./docs/API.md:377:当前 runtime local IPC 未实现独立 ping/pong。GUI 应使用 `system.getStatus` 作为连接健康检查。
./docs/agent-token-auth-design.md:179:   - agent 未实现 challenge 时继续走 §2.1 明文 token 路径（双模式并存，
./docs/mobile-support-feasibility.en.md:100:- Script delivery: Request `run_script` (content inlined) — **already landed** (A1, see `docs/android-agent-design.md` §3.2); incremental version sync of the `script.pull` type is reserved (not implemented)
./docs/mobile-support-feasibility.en.md:101:- `device.capabilities` capability reporting: the register capabilities field is already passed through; actively reporting changes is reserved (not implemented)
./docs/mobile-support-feasibility.en.md:102:- `asset.sync` template image / asset sync: reserved (not implemented, A4)
./docs/mobile-support-feasibility.en.md:152:| AccessibilityService.takeScreenshot | Rate-limited (about 1 per second) | API 30+ | Fallback / low-frequency scenarios (a candidate scheme, not implemented — a `mobile-automation-design.md` D3 planned item) |
./docs/guide/config.md:46:### Tray (托盘配置)——未实现（计划中）
./docs/guide/config.md:48:配置结构中**没有** `tray` 节。`TrayConfig` 及其相关字段（`minimizeToTray` / `iconPath` / `menuItems` / 图标状态等）尚未实现，属规划项，此处不提供配置说明。
./docs/guide/structure.md:121:Lua 脚本调试基于 EmmyLua attach（runtime 监听 `:9966`），脚本内可用 `debugger` 模块的四个函数：`start` / `stop` / `breakpoint` / `breakHere`。DAP 协议适配为规划项，尚未实现。
./docs/guide/config.en.md:13:  - Tray (托盘配置)——未实现（计划中）
./docs/android-agent.en.md:124:- **A3 reliability (remaining items implemented, 2026-09-30)**: boot autostart (BootCompletedReceiver + switch), crash self-restart (backoff alarm + crash-string abandonment + coreRunning gating), the core watchdog, device-model keep-alive guidance (docs/guides/android-keep-alive.md); restricted settings guidance (docs/guides/android-restricted-settings.md + the pre-authorization script + in-app RestrictedSettingsPolicy guidance, landed the same day); disconnect-buffer autonomy has been carried by the ready-made C++ RemoteClient since A1 (outbox/reconnect, covered by agentcore_test). The token auth P1 landed on 2026-09-20 (see docs/agent-token-auth-design.md), with the nightly CI building the APK and the JVM unit tests entering the build-android job. Part of the A3-P2 security evolution landed (2026-10-10: per-agent tokens stored server-side + Dashboard management UI/audit, with zero changes on the agent side; token storage migrated to the Keystore encrypted store + a one-time plaintext migration; challenge-response/TLS not implemented).
./README.zh.md:53:- **跨平台** - Windows、macOS、Linux 统一接口抽象；Android 实验性支持、iOS 规划中（[平台支持详情](docs/platforms.md)）
./CHANGELOG.md:59:- **顺带修正**：`platforms.en.md` 5 处中文源锚点残留（`#ci-验证对应`、`#api-适用性标注`、`#各环境差异说明`、`#ios-为什么是-规划中-而不是-支持`、`#平台支持矩阵` → 英文 slug）。
./CHANGELOG.md:313:- **缺口清单反向对账（a2bdbeb）**：development-todo「当前缺口清单」六处仍写 file/hotkey/tray「未实现」——三项 2026-10-04 已落地，勾选同步（头注仍缺段、notify tray 子项、文件 IO、热键监听、UIA 事件绑定、P2 优先级行）；其余未勾项（编排依赖/并发/条件、任务与订阅持久化、等待类 API、模板批量、进程/窗口事件源）逐条对码核实仍真实未实现，保持原样。
./CHANGELOG.md:338:- **notify 托盘（[72da5cc](https://github.com/cuihairu/wingman/commit/72da5cc)）**：脚本层 `trayShow/trayHide/traySetBadge/traySetTooltip` 四函数只发 `notify.tray.*` 意图事件，runtime notify_bridge 订阅转投 EventBuffer（method 去前缀，installNotifyBridge 幂等防订阅泄漏），GUI events.ts 轮询分发经 tauri `tray_control` 驱动系统托盘——托盘本体归 GUI，无 GUI 附着时事件仅入有界缓冲（1000 条）；badge 意图通道保留但 tauri v2 无角标 API，GUI 端暂不支持（同日 fix 条实证修正）。测试：notify_module 4 + notify_bridge 2 + events.test.ts 3。
./CHANGELOG.md:411:`docs/development-todo.md` 缺口清单条目「pause(taskId) / resume(taskId)（未实现）」落地，Lua/Python 双侧 API 同步（清单中「API 形状统一」「命名风格统一」两项范围模糊，按任务指示留后）。
./CHANGELOG.md:674:- **收掉全仓唯一代码 TODO**：`resource_loader` 里「Encrypted resources are not supported yet」的两侧一致禁用，改为口令派生密钥的真闭环——打包 `--encrypt --password`（或 `WINGMAN_PACK_PASSWORD`）→ 运行期 `WINGMAN_SCRIPT_PASSWORD` 解密加载。加密产物没有口令就是永久打不开的字节流，故 `build()` 在复制 stub **之前**硬拒空口令，而不是产出一个打不开的 exe。
./CHANGELOG.md:679:- **让非 Windows 跑到生产代码**：`Packer::buildResourceBytes()` / `ResourceLoader::loadScriptFromBytes()` 这对平台无关字节级入口是往返与错口令用例的执行体（PE 读写 `BeginUpdateResource*`/`FindResourceA` 仍是 Windows-only，只负责换容器），ELF 侧嵌入未实现即明确失败，不静默写「看着成功其实没嵌脚本」的产物。顺带修 `main.cpp` 上「Version/Size 恒打 0」的存量显示 bug（头部字段要解头后才进 `ResourceInfo`）。
./CHANGELOG.md:930:- 已知胶水不可达分支（不硬凑）：`getVoteResult` 命中 `votes_` 的分支仅由 `TeamClient::handleServerMessage` 填充，胶水层无入口；`UdpSocket::recvFrom` 的 timeoutMs 参数未实现（asio `receive_from` 同步阻塞），超时分支不存在。
./CHANGELOG.md:1047:- 平台支持与 API 适用性文档 `docs/platforms.md`：平台信息唯一对账表（README 承诺 / CI 验证范围 / 代码条件编译三线对齐）——支持矩阵（桌面三平台已支持、Android 实验性、iOS 规划中）、36 个 Lua 模块逐平台 API 适用性标注（Android 端侧仅 A2 三子表，每条不可用写明原因）、实际踩坑环境差异、CI 覆盖缺口如实标注（Linux/macOS C++ 核心无 CI 测试、macOS continue-on-error、Android 仅打包）、Linux 本机验证命令与收敛路线（[0e016f2](https://github.com/cuihairu/wingman/commit/0e016f2)）
./CHANGELOG.md:1074:- M1 实现代码库遗留 stub 与 TODO（[22be8af](https://github.com/cuihairu/wingman/commit/22be8af)）
./vcpkg/buildtrees/tiff/src/v4.7.2-910d586e47.clean/contrib/win_dib/Tiffile.cpp:361:        case ORIENTATION_RIGHTBOT: /* XXX */
./vcpkg/buildtrees/tiff/src/v4.7.2-910d586e47.clean/contrib/win_dib/Tiffile.cpp:362:        case ORIENTATION_LEFTBOT:  /* XXX */
./vcpkg/buildtrees/tiff/src/v4.7.2-910d586e47.clean/contrib/win_dib/Tiffile.cpp:370:        case ORIENTATION_RIGHTTOP: /* XXX */
./vcpkg/buildtrees/tiff/src/v4.7.2-910d586e47.clean/contrib/win_dib/Tiffile.cpp:371:        case ORIENTATION_LEFTTOP:  /* XXX */
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/docs/mkdocs/docs/api/basic_json/dump.md:24::   If `ensure_ascii` is true, all non-ASCII characters in the output are escaped with `\uXXXX` sequences, and the
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/docs/mkdocs/docs/home/architecture.md:89:TODO
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/docs/mkdocs/docs/home/releases.md:352:- The library can read and write a subset of **[BSON](http://bsonspec.org/) (Binary JSON)**. All data types known from JSON are supported, whereas other types more tied to MongoDB such as timestamps, object ids, or binary data are currently not implemented. See [the README](https://github.com/nlohmann/json#binary-formats-bson-cbor-messagepack-and-ubjson) for examples. #1244 #1320
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/docs/mkdocs/docs/home/exceptions.md:348:The parsing of the corresponding BSON record type is not implemented (yet).
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/single_include/nlohmann/json.hpp:11646:            get();  // TODO(niels): may we ignore N here?
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/single_include/nlohmann/json.hpp:18832:    in the output are escaped with `\uXXXX` sequences, and the result consists
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/single_include/nlohmann/json.hpp:19117:                             \uXXXX sequences
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/single_include/nlohmann/json.hpp:21251:          m_data(std::move(other.m_data)) // cppcheck-suppress[accessForwarded] TODO check
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/single_include/nlohmann/json.hpp:21253:        , start_position(other.start_position) // cppcheck-suppress[accessForwarded] TODO check
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/single_include/nlohmann/json.hpp:21254:        , end_position(other.end_position) // cppcheck-suppress[accessForwarded] TODO check
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/include/nlohmann/detail/output/serializer.hpp:102:    in the output are escaped with `\uXXXX` sequences, and the result consists
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/include/nlohmann/detail/output/serializer.hpp:387:                             \uXXXX sequences
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/include/nlohmann/detail/input/binary_reader.hpp:1861:            get();  // TODO(niels): may we ignore N here?
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/include/nlohmann/json.hpp:1228:          m_data(std::move(other.m_data)) // cppcheck-suppress[accessForwarded] TODO check
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/include/nlohmann/json.hpp:1230:        , start_position(other.start_position) // cppcheck-suppress[accessForwarded] TODO check
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/include/nlohmann/json.hpp:1231:        , end_position(other.end_position) // cppcheck-suppress[accessForwarded] TODO check
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/thirdparty/Fuzzer/test/StrncmpTest.cpp:13:  // TODO: check other sizes.
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/thirdparty/Fuzzer/test/MemcmpTest.cpp:11:  // TODO: check other sizes.
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/thirdparty/Fuzzer/FuzzerSHA1.cpp:63:/* public API - prototypes - TODO: doxygen*/
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/thirdparty/Fuzzer/afl/afl_driver.cpp:124:  assert(0 && "GetPeakRSSMb() is not implemented for your platform");
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/thirdparty/Fuzzer/FuzzerUtilWindows.cpp:161:  // TODO: make this implementation more efficient.
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/thirdparty/Fuzzer/FuzzerTraceState.cpp:141:  // TODO: std::set is too inefficient, need to have a custom DS here.
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/thirdparty/Fuzzer/FuzzerUtilDarwin.cpp:102:  // FIXME: We probably shouldn't hardcode the shell path.
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/thirdparty/Fuzzer/FuzzerUtilPosix.cpp:102:  assert(0 && "GetPeakRSSMb() is not implemented for your platform");
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/src/unit-to_chars.cpp:9:// XXX:
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/src/unit-element_access2.cpp:1493:// TODO(falbrechtskirchinger) merge with the other test case; clean up
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/src/unit-udt.cpp:208:    // TODO(nlohmann) test exceptions
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/abi/include/nlohmann/json_v3_10_5.hpp:10003:            get();  // TODO(niels): may we ignore N here?
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/abi/include/nlohmann/json_v3_10_5.hpp:16136:    in the output are escaped with `\uXXXX` sequences, and the result consists
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/tests/abi/include/nlohmann/json_v3_10_5.hpp:16421:                             \uXXXX sequences
./vcpkg/buildtrees/nlohmann-json/src/v3.12.0-89fed620e6.clean/ChangeLog.md:3263:- Implemented the JSON spec for string parsing for everything but the \uXXXX escaping [\#21](https://github.com/nlohmann/json/pull/21) ([Teemperor](https://github.com/Teemperor))
./vcpkg/buildtrees/sol2/src/v3.5.0-f13bb4a017.clean/include/sol/stack_push.hpp:456:			// TODO: figure out a better way to do this...?
./vcpkg/buildtrees/sol2/src/v3.5.0-f13bb4a017.clean/include/sol/version.hpp:174:// TODO: ARM codes? So far, not needed explicitly...
./vcpkg/buildtrees/sol2/src/v3.5.0-f13bb4a017.clean/include/sol/usertype_container.hpp:1007:				// TODO: should we warn or error about someone calling insert on an ordered / lookup container with no associativity?
./vcpkg/buildtrees/sol2/src/v3.5.0-f13bb4a017.clean/include/sol/optional_implementation.hpp:282:		// TODO make a version which works with MSVC
./vcpkg/buildtrees/sol2/src/v3.5.0-f13bb4a017.clean/include/sol/optional_implementation.hpp:1252:		// TODO check exception guarantee
./vcpkg/buildtrees/sol2/src/v3.5.0-f13bb4a017.clean/include/sol/optional_implementation.hpp:2292:	// TODO SFINAE
./vcpkg/buildtrees/sol2/src/v3.5.0-f13bb4a017.clean/include/sol/stack_get_unqualified.hpp:312:			// so now I would get the error W4XXX unreachable
./vcpkg/buildtrees/sol2/src/v3.5.0-f13bb4a017.clean/include/sol/function_types.hpp:211:				// TODO: hoist into lambda in this case for yielding???
./vcpkg/buildtrees/sol2/src/v3.5.0-f13bb4a017.clean/include/sol/pairs_iterator.hpp:154:					// TODO: abort, or throw an error?
./vcpkg/buildtrees/sol2/src/v3.5.0-f13bb4a017.clean/tests/run_time/source/basic.cpp:636:	// TODO: will this fail on certain lower Lua versions?
./vcpkg/buildtrees/sol2/src/v3.5.0-f13bb4a017.clean/tests/exceptions/source/functions.cpp:139:		// TODO: better handling of no_prop in this case here
./vcpkg/buildtrees/sol2/src/v3.5.0-f13bb4a017.clean/tests/exceptions/source/functions.cpp:155:		// TODO: this should work at some point, yeah?
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/FAQ.md:692:## Why do I get return code XXX from an HTTP server?
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/internals/CONNECTION-FILTERS.md:117:  int sockindex;                 /* TODO: like to get rid off this */
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/HELP-US.md:70:## TODO items
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/HELP-US.md:74:[TODO](https://curl.se/docs/todo.html) file. Some of the ideas are
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/HELP-US.md:78:Before you invest a lot of time on a TODO item, do bring it up for discussion
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/SECURITY-ADVISORY.md:126:The top two are almost always `upgrade curl to version XXX` and `apply the
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_REDIR_PROTOCOLS_STR.md:96:Returns CURLE_UNKNOWN_OPTION if the option is not implemented,
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_PROTOCOLS_STR.md:94:Returns CURLE_UNKNOWN_OPTION if the option is not implemented,
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_DATA.md:86:  /* CA cert in PEM format, replace the XXXs */
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_DATA.md:89:    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_DATA.md:90:    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_DATA.md:91:    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_DATA.md:92:    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_DATA.md:93:    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_DATA.md:94:    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLINFO_RTSP_SERVER_CSEQ.md:34:Listening for server initiated requests is not implemented.
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_FUNCTION.md:146:  /* CA cert in PEM format, replace the XXXs */
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_FUNCTION.md:149:    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_FUNCTION.md:150:    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_FUNCTION.md:151:    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_FUNCTION.md:152:    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_FUNCTION.md:153:    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/libcurl/opts/CURLOPT_SSL_CTX_FUNCTION.md:154:    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/BUGS.md:240:The `KNOWN_BUGS` document has a sibling document known as `TODO`.
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/BUGS.md:242:## `TODO`
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/BUGS.md:246:*enhancement* or *feature-request* and get added to the `TODO` document and
./vcpkg/buildtrees/curl/src/url-8_21_0-3ffa9f46b7.clean/docs/BUGS.md:247:the issues are closed. We do not keep TODO items open in the issue tracker.


[showing lines 23-200 of 200; full output saved to /home/cui/.local/share/opencode/shell/741a5d0583f8c2dd9f9ae9f3c10157e24331e1fa/sh_14e7b9ac4002VwMmelKvXGw53c.out]
> build · gpt-6-luna
初步看到两个明显审查焦点：架构决策文档本身登记了 execution/artifact 等未闭环项；另外 README 与路线图对 Android 安全成熟度有“能力已落地”与“认证/TLS 未完”的并列表述。我正在核实网络入口、执行生命周期和实际文档内容，避免把已明确标注的规划误报成实现缺陷。
[0m→ [0mRead orchestrator/server/internal/agent/listener.go [90m[offset=1, limit=240][0m
[0m→ [0mRead docs/agent-token-auth-design.md [90m[offset=1, limit=270][0m
[0m→ [0mRead libs/transport/src/transport_server.cpp 
[0m→ [0mRead orchestrator/server/internal/config/config.go 
[0m→ [0mRead orchestrator/server/internal/workflow/engine.go [90m[offset=1, limit=260][0m
[0m→ [0mRead orchestrator/server/internal/handlers/executions.go 
> build · gpt-6-luna
[0m→ [0mRead docs/development-todo.md [90m[offset=520, limit=100][0m
[0m→ [0mRead docs/development-todo.md [90m[offset=1, limit=130][0m
[0m→ [0mRead orchestrator/server/internal/handlers/routes.go [90m[offset=1, limit=250][0m
[0m→ [0mRead README.zh.md [90m[offset=35, limit=45][0m
[0m→ [0mRead orchestrator/server/main.go [90m[offset=80, limit=180][0m
[0m✱ [0mGrep "CreateExecution|UpdateExecution|ExecutionLog|execution_logs|executionId|screenshot.capture|case "screenshot"|StepType.*Screenshot" [90min orchestrator/server · 65 matches[0m
> build · gpt-6-luna
[0m→ [0mRead orchestrator/server/internal/workflow/engine.go [90m[offset=226, limit=160][0m
[0m→ [0mRead orchestrator/server/internal/workflow/engine.go [90m[offset=570, limit=100][0m
[0m→ [0mRead docs/api/notify.md [90m[offset=1, limit=30][0m
[0m→ [0mRead libs/transport/README.md 
[0m→ [0mRead orchestrator/server/internal/models/execution.go 
[0m→ [0mRead orchestrator/server/internal/models/models.go [90m[offset=80, limit=50][0m
[0m→ [0mRead orchestrator/server/internal/handlers/script.go [90m[offset=455, limit=100][0m
[0m✱ [0mGrep "func \(e \*Engine\).*Recover|interrupted|recovery|恢复" [90min orchestrator/server/internal/workflow · 0 matches[0m
> build · gpt-6-luna
[0m✱ [0mGrep "authRequired\(|handleRegister|tokenValid|challenge" [90min orchestrator/server/internal/agent/listener.go · 25 matches[0m
[0m→ [0mRead orchestrator/server/internal/agent/listener.go [90m[offset=232, limit=35][0m
[0m→ [0mRead docs/platforms.md [90m[offset=1, limit=110][0m
[0m→ [0mRead README.zh.md [90m[offset=65, limit=15][0m
[0m→ [0mRead docs/api/debugger.md [90m[offset=1, limit=80][0m
[0m→ [0mRead orchestrator/server/internal/models/workflow.go [90m[offset=1, limit=150][0m
> build · gpt-6-luna
[0m→ [0mRead orchestrator/server/internal/agent/listener.go [90m[offset=631, limit=70][0m
> build · gpt-6-luna
[0m→ [0mRead lib/wingman/src/script/modules/notify_module.cpp [90m[offset=45, limit=70][0m
[0m→ [0mRead docs/development-todo.md [90m[offset=5, limit=22][0m
[0m→ [0mRead README.zh.md [90m[offset=68, limit=8][0m
[0m→ [0mRead lib/wingman/src/script/modules/notify_module.cpp [90m[offset=188, limit=26][0m
[0m→ [0mRead docs/architecture-decisions.md [90m[offset=656, limit=58][0m
[0m→ [0mRead orchestrator/server/internal/agent/listener.go [90m[offset=68, limit=15][0m
> build · gpt-6-luna
[0m✱ [0mGrep "models\.Execution\{|Execution\{" [90min orchestrator/server/internal · 44 matches[0m
> build · gpt-6-luna
[0m→ [0mRead orchestrator/server/internal/workflow/exec_record.go 
[0m→ [0mRead orchestrator/server/internal/workflow/engine.go [90m[offset=680, limit=85][0m
[0m→ [0mRead orchestrator/server/internal/agent/listener.go [90m[offset=490, limit=50][0m
[0m→ [0mRead orchestrator/server/main.go [90m[offset=94, limit=34][0m
> build · gpt-6-luna
[0m✱ [0mGrep "start|not yet implemented|false" [90min lib/wingman/src/script/modules/debugger_module.cpp · 5 matches[0m
> build · gpt-6-luna
[0m→ [0mRead libs/androidagent/src/android_script_api.cpp [90m[offset=75, limit=45][0m
> build · gpt-6-luna
````

## 1) 最薄弱的 3 处（按风险排序）

1. **Agent TCP 链路缺少默认的 fail-closed 安全基线。**  
   `orchestrator/server/internal/agent/listener.go:74-80,180-183,233-244,651-687`：注册 token 与 DB token 都未启用时，注册鉴权关闭；服务端直接监听裸 TCP。`docs/agent-token-auth-design.md:16-30,155-161,209-228` 也明确记录了明文通道和 TLS 尚未落地。部署若把 Agent 端口暴露到不可信网络，连接可被伪造或中间人干预，命令与脚本内容也没有传输加密。当前是**部署配置决定安全性**，不是默认安全。

2. **Go 工作流状态有持久化记录，却没有进程重启恢复闭环。**  
   `orchestrator/server/internal/workflow/engine.go:23-30,96-105,243-281`：运行调度和取消句柄保存在内存 `running` map，提交后启动 goroutine；没有从 DB 重建运行状态的入口。进程重启后，DB 中可能留下 `running` 工作流和 `pending/running` 步骤，但新进程不会接管或标记中断。对长任务而言，用户看到的状态可能永久不终结。

3. **Execution 虽是核心模型，日志关联仍不能稳定到单次执行。**  
   `orchestrator/server/main.go:111-123` 收到脚本输出后只写 `ScriptID/Output/Level`；`orchestrator/server/internal/models/models.go:92-98` 的 `ExecutionLog` 也没有 `ExecutionID`。同一脚本多次或并发运行时，日志难以归属于某一次执行，削弱审计与故障定位。当前 Execution 记录链路也未覆盖所有步骤类型，部分缺口见 `docs/architecture-decisions.md:701-711`。

## 2) 最值得补的 3 项新能力

1. **TLS/证书校验与注册鉴权默认策略。**  
   现在只有可选 token/challenge；TLS 明确延期，空 token 源仍可关闭鉴权。适合在 Agent 端口可被远程访问时优先补：至少加密传输并校验证书；同时让公网/非回环监听必须显式启用鉴权。现在没有，是因为设计选择了兼容旧部署、假设内网信任，并把 TLS 留给后续安全里程碑（`docs/agent-token-auth-design.md:155-161,209-228`）。

2. **工作流中断检测与恢复策略。**  
   先提供可预测的“启动时标记 interrupted、告知哪些步骤可能已执行”，再考虑可恢复执行；不要未经定义就自动重跑。现在没有，是因为任意脚本可能有外部副作用，仓库尚未定义幂等键、步骤检查点或重放语义；盲目重试可能重复转账、点击或写入（`engine.go:262-281`）。

3. **Execution 级日志与产物归档。**  
   将日志、截图、脚本版本、结果、审计统一挂到 `executionId`，并提供保留策略和下载权限。现在没有，是因为 v1 主要完成了记录与只读查询；产物存储路径仍未建设，部分步骤还没有 Execution 记录（`models/execution.go:46-50`、`docs/architecture-decisions.md:692-711`）。

## 3) 文档与实现可能对不上的地方

- **调试器被 README 表述成可用能力，但实现是 stub。**  
  `README.zh.md:71` 写“VS Code 断点调试”；实际 `lib/wingman/src/script/modules/debugger_module.cpp:13-15` 的 `start()` 恒返回 `false`。`docs/api/debugger.md:5-19` 已明确说明未实现，建议统一 README 与功能表口径。

- **Android 脚本 API 被说成与桌面“同名同形”，但实际只支持子集且语义有差异。**  
  `README.zh.md:72` 这样概括；`docs/platforms.md:86-89` 明确 Android ScriptRunner 只暴露三张子表。比如 `libs/androidagent/src/android_script_api.cpp:81-108` 的输入 API 包含 `tap/swipe`，且 `click` 被映射为触屏操作；不能据此承诺桌面 API 全量兼容。

- **README 把 Toast/Webhook 列为通知能力，缺少“默认不可见/不可发”的关键限制。**  
  `README.zh.md:70` 未标注限制；实际 `lib/wingman/src/script/modules/notify_module.cpp:57-66,198-206` 显示 Toast 只发事件、当前无消费者，Webhook 默认关闭且空白名单全拒。API 文档 `docs/api/notify.md:9-14` 已写清，应同步到 README。

- **“57 项已完成”不等于所有路线图交付已完成。**  
  `todo.md:6-8` 把人工视频录制列为剩余项；`ROADMAP.md:565-571` 仍将 M8.2 视频教程标为 `[ ]`。这不一定是实现遗漏，但项目级完成状态应明确区分“工程待办已清”与“发布交付未完成”。

- **旧开发待办页仍有过期状态。**  
  `docs/development-todo.md:18,21` 仍把 VS Code 调试标成“进行中”、WebSocket 标成“已实现”；至少调试器与当前 stub 实现不符。`libs/transport/README.md:3-4` 也注明该库的 WebSocket 仅为枚举占位，建议说明该待办表的能力边界或更新状态。

*以上为静态代码与文档核对，未运行全量测试。*
