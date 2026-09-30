// scripts/android-restricted-settings.sh 的契约测试（假 adb 驱动）。
//
// 脚本是「部署期一次性预授权」的人工入口，没有契约测试就没有回归护栏：
// 某次改脚本把「无设备（环境未就绪）」判成 FAIL、或 appops 取值解析坏了、
// 或 allow 忘了复核幂等，都要等到有人在真机上手跑才发现——而真机+USB 调试
// 的组合日常根本不具备。
//
// 这里用**假 adb**（由状态文件驱动）把脚本的全部路径跑一遍，不碰真设备：
//   - 无 adb / 无已授权设备 → SKIP(2)（环境未就绪，绝不与「检查不通过」混同）
//   - API < 33              → PASS(0)（不受限设置约束）
//   - 包未装                → FAIL(1)（部署前提不满足，可行动）
//   - 未预授权 / 已预授权    → FAIL(1) / PASS(0)
//   - allow 幂等 + 下发命令形状、revoke 后 check 转 FAIL、status 输出
//
// 假 adb 的 shell 子命令输出带 CRLF（对齐真 adb 的 pty 行尾），get-state 是
// 宿主侧命令只输出 LF——脚本里的 `tr -d '\r'` 因此被真实执行而非形同虚设。

package integration

import (
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

func androidScriptPath(t *testing.T) string {
	t.Helper()
	return filepath.Join(repoRoot(t), "scripts", "android-restricted-settings.sh")
}

// runAndroidScript 在净化 PATH 下跑 android-restricted-settings.sh（无 adb，
// 确定性触发 SKIP 分支）；extraEnv 里的 PATH= 覆盖净化 PATH 用于注入假 adb。
func runAndroidScript(t *testing.T, env []string, args ...string) scriptRun {
	t.Helper()
	path := noEnginePATH(t)
	cmd := exec.Command("bash", append([]string{androidScriptPath(t)}, args...)...)
	cmd.Dir = repoRoot(t)
	cmd.Env = append([]string{
		"PATH=" + path,
		"HOME=" + os.Getenv("HOME"),
	}, env...)
	var out, errBuf strings.Builder
	cmd.Stdout = &out
	cmd.Stderr = &errBuf
	err := cmd.Run()
	code := 0
	if err != nil {
		if ee, ok := err.(*exec.ExitError); ok {
			code = ee.ExitCode()
		} else {
			t.Fatalf("run android script: %v", err)
		}
	}
	return scriptRun{code: code, stdout: out.String(), stderr: errBuf.String()}
}

// fakeAdbState 描述假设备的状态：设备在位、已装的包名（空=未装）、
// API level、ACCESS_RESTRICTED_SETTINGS 当前值。
type fakeAdbState struct {
	device       bool
	installedPkg string
	sdk          string
	appops       string
}

// fakeAdbEnv 造一个含假 adb 的 PATH 目录（叠加净化 coreutils）与状态文件目录。
// 返回注入用 env 与状态目录（其下 calls 文件记录每次 adb 调用的参数，供断言）。
func fakeAdbEnv(t *testing.T, st fakeAdbState) (env []string, dir string) {
	t.Helper()
	bin := t.TempDir()
	linkCoreutils(t, bin)
	dir = t.TempDir()
	writeFile := func(name, content string) {
		if err := os.WriteFile(filepath.Join(dir, name), []byte(content), 0o644); err != nil {
			t.Fatalf("write fake adb state %s: %v", name, err)
		}
	}
	if st.device {
		writeFile("device", "ok")
	}
	if st.installedPkg != "" {
		writeFile("installed_pkg", st.installedPkg)
	}
	if st.sdk != "" {
		writeFile("sdk", st.sdk)
	}
	if st.appops != "" {
		writeFile("appops", st.appops)
	}
	// 参数形状对齐脚本的调用点：get-state / shell pm path PKG /
	// shell getprop ro.build.version.sdk / shell appops {get,set} PKG OP [MODE]
	fake := "#!/usr/bin/env bash\n" +
		"dir=\"${FAKE_ADB_DIR:?FAKE_ADB_DIR not set}\"\n" +
		"printf '%s\\n' \"$*\" >> \"$dir/calls\"\n" +
		"read_file() { if [[ -f \"$dir/$1\" ]]; then cat \"$dir/$1\"; fi }\n" +
		"case \"$1\" in\n" +
		"  get-state)\n" +
		"    if [[ \"$(read_file device)\" == \"ok\" ]]; then echo device; exit 0; fi\n" +
		"    echo 'error: no devices/emulators found' >&2\n" +
		"    exit 1\n" +
		"    ;;\n" +
		"  shell)\n" +
		"    case \"$2\" in\n" +
		"      getprop)\n" +
		"        if [[ \"$3\" == ro.build.version.sdk ]]; then printf '%s\\r\\n' \"$(read_file sdk)\"; fi\n" +
		"        exit 0\n" +
		"        ;;\n" +
		"      pm)\n" +
		"        if [[ \"$(read_file installed_pkg)\" != \"\" && \"$(read_file installed_pkg)\" == \"$4\" ]]; then\n" +
		"          printf 'package:/data/app/~~fake/%s-y/base.apk\\r\\n' \"$4\"\n" +
		"          exit 0\n" +
		"        fi\n" +
		"        exit 1\n" +
		"        ;;\n" +
		"      appops)\n" +
		"        if [[ \"$3\" == get ]]; then\n" +
		"          printf 'ACCESS_RESTRICTED_SETTINGS: %s\\r\\n' \"$(read_file appops)\"\n" +
		"        else\n" +
		"          printf '%s' \"${6:-default}\" > \"$dir/appops\"\n" +
		"        fi\n" +
		"        exit 0\n" +
		"        ;;\n" +
		"    esac\n" +
		"    ;;\n" +
		"esac\n" +
		"exit 0\n"
	if err := os.WriteFile(filepath.Join(bin, "adb"), []byte(fake), 0o755); err != nil {
		t.Fatalf("write fake adb: %v", err)
	}
	return []string{"PATH=" + bin, "FAKE_ADB_DIR=" + dir}, dir
}

// fakeAdbCalls 读假 adb 的调用记录。
func fakeAdbCalls(t *testing.T, dir string) []string {
	t.Helper()
	raw, err := os.ReadFile(filepath.Join(dir, "calls"))
	if err != nil {
		t.Fatalf("read calls: %v", err)
	}
	return strings.Split(strings.TrimRight(string(raw), "\n"), "\n")
}

const androidDefaultPkg = "com.wingman.agent"

// TestAndroidRestrictedScriptSkipsWithoutAdb 三态之一：无 adb 属「环境未就绪」
// → SKIP(2)，且必须给出无 adb 时的替代路径（手册手动允许），不能只报错。
func TestAndroidRestrictedScriptSkipsWithoutAdb(t *testing.T) {
	for _, action := range []string{"check", "allow", "revoke", "status"} {
		t.Run(action, func(t *testing.T) {
			got := runAndroidScript(t, nil, action)
			if got.code != 2 {
				t.Fatalf("%s: exit=%d want 2 (SKIP)\n%s", action, got.code, got.output())
			}
			if !strings.Contains(got.output(), "SKIP") {
				t.Errorf("%s: 缺 SKIP 标记:\n%s", action, got.output())
			}
			if !strings.Contains(got.output(), "docs/guides/android-restricted-settings.md") {
				t.Errorf("%s: 缺无 adb 时的手动路径指引:\n%s", action, got.output())
			}
		})
	}
}

// TestAndroidRestrictedScriptSkipsWithoutDevice adb 在位但无已授权设备同样判
// SKIP(2)——「没跑成」不算失败；四个动作共用 require_device，一并通过。
func TestAndroidRestrictedScriptSkipsWithoutDevice(t *testing.T) {
	env, _ := fakeAdbEnv(t, fakeAdbState{device: false, sdk: "34"})
	for _, action := range []string{"check", "allow", "revoke", "status"} {
		t.Run(action, func(t *testing.T) {
			got := runAndroidScript(t, env, action)
			if got.code != 2 {
				t.Fatalf("%s: exit=%d want 2 (SKIP)\n%s", action, got.code, got.output())
			}
			if !strings.Contains(got.output(), "无已授权设备") {
				t.Errorf("%s: 应说明是设备缺位而非检查不通过:\n%s", action, got.output())
			}
		})
	}
}

// TestAndroidRestrictedCheckPassesBelowApi33 API < 33 的设备不受限设置约束，
// check 应直接 PASS——否则会在 Android 12- 上报误导性的 FAIL。
func TestAndroidRestrictedCheckPassesBelowApi33(t *testing.T) {
	env, _ := fakeAdbEnv(t, fakeAdbState{device: true, installedPkg: androidDefaultPkg, sdk: "31", appops: "default"})
	got := runAndroidScript(t, env, "check")
	if got.code != 0 {
		t.Fatalf("exit=%d want 0\n%s", got.code, got.output())
	}
	if !strings.Contains(got.output(), "31") || !strings.Contains(got.output(), "不受限设置约束") {
		t.Errorf("应报 API 31 不受限设置约束:\n%s", got.output())
	}
}

// TestAndroidRestrictedCheckFailsWhenPackageMissing 包未装是部署前提不满足
// → FAIL(1)，且信息必须可行动（先装 APK）。
func TestAndroidRestrictedCheckFailsWhenPackageMissing(t *testing.T) {
	env, _ := fakeAdbEnv(t, fakeAdbState{device: true, sdk: "34"})
	got := runAndroidScript(t, env, "check")
	if got.code != 1 {
		t.Fatalf("exit=%d want 1\n%s", got.code, got.output())
	}
	if !strings.Contains(got.output(), "未安装") || !strings.Contains(got.output(), androidDefaultPkg) {
		t.Errorf("应报包未装且带包名:\n%s", got.output())
	}
}

// TestAndroidRestrictedCheckFailsWhenNotPreauthorized Android 13+ 未预授权是
// 真失败 → FAIL(1)，并给出修复指引（allow 子命令 / 手动允许）。
func TestAndroidRestrictedCheckFailsWhenNotPreauthorized(t *testing.T) {
	env, _ := fakeAdbEnv(t, fakeAdbState{device: true, installedPkg: androidDefaultPkg, sdk: "34", appops: "default"})
	got := runAndroidScript(t, env, "check")
	if got.code != 1 {
		t.Fatalf("exit=%d want 1\n%s", got.code, got.output())
	}
	if !strings.Contains(got.output(), "FAIL") {
		t.Errorf("缺 FAIL 标记:\n%s", got.output())
	}
	if !strings.Contains(got.output(), "android-restricted-settings.sh allow") ||
		!strings.Contains(got.output(), "手动允许") {
		t.Errorf("应同时给出脚本修复与手动两条路径:\n%s", got.output())
	}
}

// TestAndroidRestrictedCheckPassesWhenAllowed 已预授权（allow）→ PASS(0)。
func TestAndroidRestrictedCheckPassesWhenAllowed(t *testing.T) {
	env, _ := fakeAdbEnv(t, fakeAdbState{device: true, installedPkg: androidDefaultPkg, sdk: "34", appops: "allow"})
	got := runAndroidScript(t, env, "check")
	if got.code != 0 {
		t.Fatalf("exit=%d want 0\n%s", got.code, got.output())
	}
	if !strings.Contains(got.output(), "PASS") {
		t.Errorf("缺 PASS 标记:\n%s", got.output())
	}
}

// TestAndroidRestrictedAllowIsIdempotent allow 必须下发正确的 appops 命令、
// 复核通过后 PASS，且重复执行仍 PASS（幂等）。
func TestAndroidRestrictedAllowIsIdempotent(t *testing.T) {
	env, dir := fakeAdbEnv(t, fakeAdbState{device: true, installedPkg: androidDefaultPkg, sdk: "34", appops: "default"})
	got := runAndroidScript(t, env, "allow")
	if got.code != 0 {
		t.Fatalf("allow: exit=%d want 0\n%s", got.code, got.output())
	}
	want := "shell appops set " + androidDefaultPkg + " ACCESS_RESTRICTED_SETTINGS allow"
	found := false
	for _, call := range fakeAdbCalls(t, dir) {
		if call == want {
			found = true
		}
	}
	if !found {
		t.Errorf("adb 调用缺 %q:\n%v", want, fakeAdbCalls(t, dir))
	}
	// 幂等：已 allow 的设备再 allow 仍 PASS
	if got := runAndroidScript(t, env, "allow"); got.code != 0 {
		t.Fatalf("allow(幂等二跑): exit=%d want 0\n%s", got.code, got.output())
	}
	// 复核通过后 check 也应 PASS（状态文件已被假 adb 的 set 更新）
	if got := runAndroidScript(t, env, "check"); got.code != 0 {
		t.Fatalf("check after allow: exit=%d want 0\n%s", got.code, got.output())
	}
}

// TestAndroidRestrictedAllowHonorsPackageOverride WINGMAN_ANDROID_PKG 必须贯穿
// 全部 adb 命令（多包名部署场景）。
func TestAndroidRestrictedAllowHonorsPackageOverride(t *testing.T) {
	const other = "com.example.wingman2"
	env, dir := fakeAdbEnv(t, fakeAdbState{device: true, installedPkg: other, sdk: "34", appops: "default"})
	got := runAndroidScript(t, append(env, "WINGMAN_ANDROID_PKG="+other), "allow")
	if got.code != 0 {
		t.Fatalf("exit=%d want 0\n%s", got.code, got.output())
	}
	want := "shell appops set " + other + " ACCESS_RESTRICTED_SETTINGS allow"
	found := false
	for _, call := range fakeAdbCalls(t, dir) {
		if call == want {
			found = true
		}
	}
	if !found {
		t.Errorf("adb 调用缺 %q:\n%v", want, fakeAdbCalls(t, dir))
	}
}

// TestAndroidRestrictedRevokeThenCheckFails revoke 还原 default 后，check 必须
// 转回 FAIL——证明 revoke 真的改了设备状态而非空喊 PASS。
func TestAndroidRestrictedRevokeThenCheckFails(t *testing.T) {
	env, _ := fakeAdbEnv(t, fakeAdbState{device: true, installedPkg: androidDefaultPkg, sdk: "34", appops: "allow"})
	if got := runAndroidScript(t, env, "revoke"); got.code != 0 {
		t.Fatalf("revoke: exit=%d want 0\n%s", got.code, got.output())
	}
	got := runAndroidScript(t, env, "check")
	if got.code != 1 {
		t.Fatalf("check after revoke: exit=%d want 1\n%s", got.code, got.output())
	}
	if !strings.Contains(got.output(), "FAIL") {
		t.Errorf("revoke 后 check 应 FAIL:\n%s", got.output())
	}
}

// TestAndroidRestrictedStatusPrintsState status 打印包名/API/受限设置三项，
// 不做判定（exit 0）；缺设备时与其他动作一样 SKIP(2)。
func TestAndroidRestrictedStatusPrintsState(t *testing.T) {
	env, _ := fakeAdbEnv(t, fakeAdbState{device: true, installedPkg: androidDefaultPkg, sdk: "34", appops: "allow"})
	got := runAndroidScript(t, env, "status")
	if got.code != 0 {
		t.Fatalf("exit=%d want 0\n%s", got.code, got.output())
	}
	for _, want := range []string{androidDefaultPkg, "API level", "34", "allow"} {
		if !strings.Contains(got.output(), want) {
			t.Errorf("status 缺 %q:\n%s", want, got.output())
		}
	}
}

// TestAndroidRestrictedScriptUsage 未知子命令 → exit 2 + 用法列出四个子命令；
// --help 是显式求助 → exit 0。
func TestAndroidRestrictedScriptUsage(t *testing.T) {
	got := runAndroidScript(t, nil, "frobnicate")
	if got.code != 2 {
		t.Fatalf("exit=%d want 2\n%s", got.code, got.output())
	}
	if !strings.Contains(got.output(), "未知子命令") {
		t.Errorf("应报未知子命令:\n%s", got.output())
	}
	for _, sub := range []string{"check", "allow", "revoke", "status"} {
		if !strings.Contains(got.output(), sub) {
			t.Errorf("用法未列 %s:\n%s", sub, got.output())
		}
	}
	if got := runAndroidScript(t, nil, "--help"); got.code != 0 {
		t.Fatalf("--help exit=%d want 0\n%s", got.code, got.output())
	} else if !strings.Contains(got.output(), "android-restricted-settings.sh") {
		t.Errorf("--help 应回显用法:\n%s", got.output())
	}
}
