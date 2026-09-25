// verify-guacd-e2e.sh 的契约测试（脚本三态自测）。
//
// 脚本本身是「人跑的操作入口」，不进 Go 测试就没法防回归：某次改脚本把
// SKIP 判成 PASS、或忘了 --keep 语义、或 compose 路径写错，都要等到有人
// 手工跑 e2e 才发现——而 e2e 需要容器栈，日常根本没人跑。
//
// 这里用**假引擎 + 假端口**把脚本的三条路径都跑一遍，不碰真容器：
//   - 无容器引擎        → SKIP(2)，并提示纯逻辑验证的替代命令
//   - 端口未就绪        → FAIL(1)，且报出缺哪个端口
//   - 端口已就绪        → 进入 test 路径（此处用 -h 短路，不真跑 e2e）
// 另测 compose 文件本身的契约（镜像钉 digest、四服务齐全、端口对齐用例）。

package integration

import (
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"strconv"
	"strings"
	"testing"
	"time"
)

// repoRoot 从本包位置（orchestrator/server/integration）上溯到仓库根。
func repoRoot(t *testing.T) string {
	t.Helper()
	_, file, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatal("runtime.Caller failed")
	}
	return filepath.Clean(filepath.Join(filepath.Dir(file), "..", "..", ".."))
}

func scriptPath(t *testing.T) string {
	return filepath.Join(repoRoot(t), "scripts", "verify-guacd-e2e.sh")
}

type scriptRun struct {
	code   int
	stdout string
	stderr string
}

func (r scriptRun) output() string { return r.stdout + r.stderr }

// coreutilsNeededByScript 脚本（或假引擎的 shebang）会用到的命令。构造净化
// PATH 时只链这些，**故意不含 podman/docker**——否则测试会真的去
// `compose up` 拉镜像（实测一次误跑就是 6 分钟 + 数 GB 拉取）。
//
// bash/env/dirname 必须在列：脚本首行 `cd "$(dirname "$0")/.."`，假引擎的
// shebang 是 `#!/usr/bin/env bash`。缺任何一个都会让「假环境」自身先崩，
// 症状看起来像被测逻辑有 bug（实测踩过：dirname 缺失 → cd /.. → 相对路径失效）。
var coreutilsNeededByScript = []string{
	"bash", "env", "dirname", "basename",
	"sleep", "awk", "sed", "grep", "cat", "head", "sort", "tr", "cut", "date", "wc", "timeout",
}

// linkCoreutils 把基础命令软链进 dir。
func linkCoreutils(t *testing.T, dir string) {
	t.Helper()
	for _, bin := range coreutilsNeededByScript {
		src, err := exec.LookPath(bin)
		if err != nil {
			continue // 宿主没有该命令，跳过（脚本对应路径不会被触发）
		}
		_ = os.Symlink(src, filepath.Join(dir, bin))
	}
}

// noEnginePATH 造一个只含基础命令的 PATH 目录，返回其路径。
func noEnginePATH(t *testing.T) string {
	t.Helper()
	dir := t.TempDir()
	linkCoreutils(t, dir)
	// 自检：这个 PATH 里绝不能有容器引擎
	for _, engine := range []string{"podman", "docker"} {
		if _, err := os.Stat(filepath.Join(dir, engine)); err == nil {
			t.Fatalf("净化 PATH 意外含 %s", engine)
		}
	}
	return dir
}

// runScript 在**净化过的 PATH** 下跑脚本，确定性地触发「无容器引擎」分支。
// extraEnv 里的 PATH= 会覆盖净化 PATH（用于注入假引擎）。
func runScript(t *testing.T, env []string, args ...string) scriptRun {
	t.Helper()
	path := noEnginePATH(t)
	cmd := exec.Command("bash", append([]string{scriptPath(t)}, args...)...)
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
			t.Fatalf("run script: %v", err)
		}
	}
	return scriptRun{code: code, stdout: out.String(), stderr: errBuf.String()}
}

// TestGuacdE2EScriptSkipsWithoutContainerEngine 三态之一：环境不具备时判
// SKIP(2) 而非 FAIL——「没跑成」绝不能算「通过」。
func TestGuacdE2EScriptSkipsWithoutContainerEngine(t *testing.T) {
	for _, action := range []string{"up", "test", "run"} {
		t.Run(action, func(t *testing.T) {
			got := runScript(t, nil, action)
			if got.code != 2 {
				t.Fatalf("%s: exit=%d want 2 (SKIP)\n%s", action, got.code, got.output())
			}
			if !strings.Contains(got.output(), "SKIP") {
				t.Errorf("%s: 缺 SKIP 标记:\n%s", action, got.output())
			}
			// 必须给出「没有引擎也能做什么」的替代路径，否则人只会卡住
			if !strings.Contains(got.output(), "go test ./integration/") {
				t.Errorf("%s: 缺替代验证指引:\n%s", action, got.output())
			}
		})
	}
}

// TestGuacdE2EScriptSkipsWhenEngineLacksCompose 「装了该命令但不支持 compose」
// 必须报 SKIP 并说清缺什么，不能在 compose 那一步炸出无关的 unknown command。
func TestGuacdE2EScriptSkipsWhenEngineLacksCompose(t *testing.T) {
	bin := t.TempDir()
	// 假 docker：存在、能被 command -v 找到，但 `docker compose version` 失败
	fakeDocker := filepath.Join(bin, "docker")
	script := "#!/usr/bin/env bash\n" +
		"if [[ \"$1\" == \"compose\" ]]; then exit 1; fi\n" +
		"exit 0\n"
	if err := os.WriteFile(fakeDocker, []byte(script), 0o755); err != nil {
		t.Fatalf("write fake docker: %v", err)
	}
	linkCoreutils(t, bin)

	cmd := exec.Command("bash", scriptPath(t), "up")
	cmd.Dir = repoRoot(t)
	cmd.Env = []string{"PATH=" + bin, "HOME=" + os.Getenv("HOME")}
	var out strings.Builder
	cmd.Stdout = &out
	cmd.Stderr = &out
	err := cmd.Run()
	code := 0
	if ee, ok := err.(*exec.ExitError); ok {
		code = ee.ExitCode()
	} else if err != nil {
		t.Fatalf("run script: %v", err)
	}
	if code != 2 {
		t.Fatalf("exit=%d want 2 (SKIP)\n%s", code, out.String())
	}
	if !strings.Contains(out.String(), "不支持 'compose'") {
		t.Errorf("应明确指出引擎缺 compose 子命令:\n%s", out.String())
	}
	// 绝不能出现 compose 步骤的原始报错（说明是提前拦下的）
	if strings.Contains(out.String(), "unknown command") {
		t.Errorf("不应把引擎原始报错抛给人:\n%s", out.String())
	}
}

// TestGuacdE2EScriptStatusReportsNotReady 端口全关时 status 判非就绪（exit 1）。
//
// 用**假引擎**而非宿主真实引擎：宿主可能没装 compose 插件（那样脚本会正确
// 报 SKIP，与本用例要验的「端口未就绪」是两条不同路径），依赖宿主会让用例
// 时绿时红。
func TestGuacdE2EScriptStatusReportsNotReady(t *testing.T) {
	if portOpenForTest(4822) || portOpenForTest(2222) {
		t.Skip("宿主上 guacd/sshd 端口已开（栈在跑），跳过「未就绪」判定")
	}
	got := runScript(t, []string{
		"GUACD_E2E_READY_TIMEOUT=1",
		"PATH=" + fakeWorkingEnginePATH(t),
	}, "status")
	if got.code != 1 {
		t.Fatalf("exit=%d want 1\n%s", got.code, got.output())
	}
	for _, name := range []string{"guacd", "sshd", "vnc", "xrdp"} {
		if !strings.Contains(got.output(), name) {
			t.Errorf("status 未提及 %s:\n%s", name, got.output())
		}
	}
	if !strings.Contains(got.output(), "NOT READY") {
		t.Errorf("缺 NOT READY 标记:\n%s", got.output())
	}
}

// fakeWorkingEnginePATH 造一个「支持 compose 的假引擎」PATH：compose 子命令
// 恒成功、ps 输出空（无容器），其余命令 no-op。用于验端口判定这条路径。
func fakeWorkingEnginePATH(t *testing.T) string {
	t.Helper()
	bin := t.TempDir()
	fake := "#!/usr/bin/env bash\n" +
		"if [[ \"$1\" == \"compose\" ]]; then\n" +
		"  case \"$2\" in\n" +
		"    version) exit 0 ;;\n" +
		"    ps) exit 0 ;;\n" +
		"    *) exit 0 ;;\n" +
		"  esac\n" +
		"fi\n" +
		"exit 0\n"
	if err := os.WriteFile(filepath.Join(bin, "docker"), []byte(fake), 0o755); err != nil {
		t.Fatalf("write fake docker: %v", err)
	}
	linkCoreutils(t, bin)
	return bin
}

// TestGuacdE2EScriptRejectsUnknownSubcommand 未知子命令给用法（exit 2）。
func TestGuacdE2EScriptRejectsUnknownSubcommand(t *testing.T) {
	got := runScript(t, nil, "frobnicate")
	if got.code != 2 {
		t.Fatalf("exit=%d want 2\n%s", got.code, got.output())
	}
	if !strings.Contains(got.output(), "未知子命令") {
		t.Errorf("应报未知子命令:\n%s", got.output())
	}
	// 用法里必须列出四个子命令
	for _, sub := range []string{"up", "down", "test", "run", "status"} {
		if !strings.Contains(got.output(), sub) {
			t.Errorf("用法未列 %s:\n%s", sub, got.output())
		}
	}
}

// TestGuacdE2EScriptRejectsUnknownFlag 未知 flag 走用法（exit 2），不静默忽略。
func TestGuacdE2EScriptRejectsUnknownFlag(t *testing.T) {
	got := runScript(t, nil, "up", "--wat")
	if got.code != 2 {
		t.Fatalf("exit=%d want 2\n%s", got.code, got.output())
	}
}

// TestGuacdE2EScriptExitsZeroOnHelp 显式求助是成功路径。
func TestGuacdE2EScriptExitsZeroOnHelp(t *testing.T) {
	got := runScript(t, nil, "--help")
	if got.code != 0 {
		t.Fatalf("--help exit=%d want 0\n%s", got.code, got.output())
	}
	if !strings.Contains(got.output(), "verify-guacd-e2e.sh") {
		t.Errorf("--help 应回显用法:\n%s", got.output())
	}
}

// TestGuacdE2EComposePinsImageDigests compose 的四服务必须钉 digest。
//
// 这是本轮「容器栈补强」的核心护栏：三条目标端点的上游都用 latest，任何
// 一次上游重建都会静默换掉镜像内容，e2e 会在无人改代码的情况下变红。
// 没有这条断言，改回 latest 不会有任何测试变红。
func TestGuacdE2EComposePinsImageDigests(t *testing.T) {
	raw, err := os.ReadFile(filepath.Join(repoRoot(t), "orchestrator", "server", "integration", "testdata", "guacd-e2e-compose.yml"))
	if err != nil {
		t.Fatalf("read compose: %v", err)
	}
	body := string(raw)

	imageLine := regexp.MustCompile(`(?m)^\s*image:\s*(\S+)\s*$`)
	images := imageLine.FindAllStringSubmatch(body, -1)
	if len(images) != 4 {
		t.Fatalf("应有 4 个 image 定义，实际 %d：\n%s", len(images), body)
	}
	services := 0
	for _, m := range images {
		services++
		ref := m[1]
		if !strings.Contains(ref, "@sha256:") {
			t.Errorf("镜像未钉 digest（上游重建会静默换内容）：%s", ref)
			continue
		}
		sum := ref[strings.Index(ref, "@sha256:")+len("@sha256:"):]
		if len(sum) != 64 || !isHex(sum) {
			t.Errorf("digest 形状非法（应为 sha256: + 64 位十六进制）：%s", ref)
		}
	}
	if services != 4 {
		t.Errorf("service 数应为 4，实际 %d", services)
	}

	// guacd 必须仍是 1.5.5（1.6.0 RDP 双崩溃，见设计 §13.2）
	if !strings.Contains(body, "guacamole/guacd:1.5.5@sha256:") {
		t.Error("guacd 必须锁 1.5.5 + digest（1.6.0 存在 RDP 双崩溃）")
	}
}

func isHex(s string) bool {
	for _, r := range s {
		if !((r >= '0' && r <= '9') || (r >= 'a' && r <= 'f')) {
			return false
		}
	}
	return len(s) > 0
}

// TestGuacdE2EComposeHasHealthchecks 四个服务都要有 healthcheck：端口可达
// 早于「协议可用」，脚本靠 healthcheck + 端口探测把「栈没起好」与
// 「代码有 bug」两类失败区分开。
func TestGuacdE2EComposeHasHealthchecks(t *testing.T) {
	raw, err := os.ReadFile(filepath.Join(repoRoot(t), "orchestrator", "server", "integration", "testdata", "guacd-e2e-compose.yml"))
	if err != nil {
		t.Fatalf("read compose: %v", err)
	}
	body := string(raw)
	if got := strings.Count(body, "healthcheck:"); got != 4 {
		t.Errorf("四个服务都应有 healthcheck，实际 %d 个", got)
	}
	// 探针只能判端口可达，不能依赖镜像里未必存在的 curl/nc
	for _, line := range strings.Split(body, "\n") {
		trimmed := strings.TrimSpace(line)
		if !strings.HasPrefix(trimmed, "test:") {
			continue
		}
		if strings.Contains(trimmed, "curl") || strings.Contains(trimmed, "nc ") {
			t.Errorf("探针不应依赖 curl/nc（目标镜像未必自带），应用 bash /dev/tcp：%s", trimmed)
		}
		if !strings.Contains(trimmed, "/dev/tcp") {
			t.Errorf("探针应走 bash /dev/tcp：%s", trimmed)
		}
	}
}

// TestGuacdE2EScriptPortsMatchE2ETargets 脚本探测的端口必须与 e2e 用例里
// 的目标端口一致。端口漂了会表现为「栈明明起来了却全红」。
func TestGuacdE2EScriptPortsMatchE2ETargets(t *testing.T) {
	raw, err := os.ReadFile(scriptPath(t))
	if err != nil {
		t.Fatalf("read script: %v", err)
	}
	body := string(raw)

	e2eRaw, err := os.ReadFile(filepath.Join(repoRoot(t), "orchestrator", "server", "integration", "guacd_e2e_test.go"))
	if err != nil {
		t.Fatalf("read e2e test: %v", err)
	}
	e2e := string(e2eRaw)

	// 脚本里声明的端口 → e2e 用例里必须真的用到
	for _, tc := range []struct {
		port    string
		comment string
	}{
		{"4822", "guacd 地址（WINGMAN_GUACD_E2E_GUACD 默认回环）"},
		{"2222", "sshd 目标端口"},
		{"5901", "vnc 目标端口"},
		{"3389", "xrdp 目标端口"},
	} {
		if !strings.Contains(body, tc.port) {
			t.Errorf("脚本未声明端口 %s（%s）", tc.port, tc.comment)
		}
	}
	// e2e 用例确实用这些端口连后端
	for _, want := range []string{`guacSetupChain(t, env, "ssh", 2222`, `guacSetupChain(t, env, "vnc", 5901`, `guacSetupChain(t, env, "rdp", 3389`} {
		if !strings.Contains(e2e, want) {
			t.Errorf("e2e 用例未按预期连 %q（端口可能漂移）", want)
		}
	}
}

// TestGuacdE2ETestsGateOnEnv 三条链路用例必须仍受 WINGMAN_GUACD_E2E 门控，
// 否则 CI（无容器栈）会因连不上 4822 而全红。
func TestGuacdE2ETestsGateOnEnv(t *testing.T) {
	raw, err := os.ReadFile(filepath.Join(repoRoot(t), "orchestrator", "server", "integration", "guacd_e2e_test.go"))
	if err != nil {
		t.Fatalf("read e2e test: %v", err)
	}
	body := string(raw)
	if !strings.Contains(body, `os.Getenv("WINGMAN_GUACD_E2E") != "1"`) {
		t.Error("三协议链路用例的 WINGMAN_GUACD_E2E 门控被移除——CI 会因无容器栈而红")
	}
	if !strings.Contains(body, "t.Skip(") {
		t.Error("门控未走 t.Skip")
	}
}

// portOpenForTest 宿主端口可达性（status 判定用例的前置判断）。
func portOpenForTest(port int) bool {
	c, err := net.DialTimeout("tcp", "127.0.0.1:"+strconv.Itoa(port), 300*time.Millisecond)
	if err != nil {
		return false
	}
	_ = c.Close()
	return true
}
