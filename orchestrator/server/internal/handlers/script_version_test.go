package handlers

import (
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/scripts"
	"github.com/gin-gonic/gin"
)

// 脚本版本管理（146）：内容即版本——创建/保存维护 models.Script.Sha256，
// run_script 下发 payload 携带 contentSha256，Execution 记录落 scriptSha256，
// 列表对存量空行懒回填。run_script 的 runtime 侧忽略未知字段，向后兼容。

func TestScriptVersionLifecycle(t *testing.T) {
	env := setupBatchEnv(t)
	env.addAgent("ver-target")
	sh := NewScriptHandler(env.db, env.dir, env.registry)

	r := gin.New()
	r.POST("/create", asAdmin(1), sh.HandleCreate)
	r.POST("/save", asAdmin(1), sh.HandleSave)
	r.GET("/scripts", asAdmin(1), sh.HandleList)
	r.POST("/run", asAdmin(1), sh.HandleRun)

	// 创建：行 sha256 = 模板内容指纹
	w := doJSON(r, "POST", "/create", map[string]any{"name": "demo", "description": "ver"})
	if w.Code != http.StatusOK {
		t.Fatalf("create: %d %s", w.Code, w.Body.String())
	}
	var row models.Script
	if err := env.db.Where("name = ?", "demo").First(&row).Error; err != nil {
		t.Fatalf("query row: %v", err)
	}
	content, err := os.ReadFile(filepath.Join(env.dir, "demo.lua"))
	if err != nil {
		t.Fatalf("read created file: %v", err)
	}
	if row.Sha256 != scripts.Sha256Of(content) {
		t.Errorf("create sha256 mismatch: %s vs %s", row.Sha256, scripts.Sha256Of(content))
	}

	// 列表：sha256 可见
	w = doJSON(r, "GET", "/scripts", nil)
	if w.Code != http.StatusOK || !strings.Contains(w.Body.String(), `"sha256":"`+row.Sha256+`"`) {
		t.Errorf("list should carry sha256, got %d %s", w.Code, w.Body.String())
	}

	// 保存新内容：版本指纹随内容更新
	newContent := "-- v2\nprint('v2')\n"
	w = doJSON(r, "POST", "/save", map[string]any{"path": "demo.lua", "content": newContent})
	if w.Code != http.StatusOK {
		t.Fatalf("save: %d %s", w.Code, w.Body.String())
	}
	var row2 models.Script
	if err := env.db.Where("name = ?", "demo").First(&row2).Error; err != nil {
		t.Fatalf("re-query row: %v", err)
	}
	if row2.Sha256 != scripts.Sha256Of([]byte(newContent)) {
		t.Errorf("save should update sha256: %s vs %s", row2.Sha256, scripts.Sha256Of([]byte(newContent)))
	}

	// 运行：payload 携带 contentSha256，执行记录落 scriptSha256
	w = doJSON(r, "POST", "/run", map[string]any{"path": "demo.lua", "agentId": "ver-target"})
	if w.Code != http.StatusOK {
		t.Fatalf("run: %d %s", w.Code, w.Body.String())
	}
	cmds := env.commandsOf("ver-target")
	if len(cmds) != 1 || cmds[0].Method != "run_script" {
		t.Fatalf("expected one run_script, got %+v", cmds)
	}
	if cmds[0].Data["contentSha256"] != row2.Sha256 {
		t.Errorf("payload contentSha256 mismatch: %v vs %s", cmds[0].Data["contentSha256"], row2.Sha256)
	}
	var exec models.Execution
	if err := env.db.Where("script_path LIKE ?", "%demo.lua").Order("id desc").First(&exec).Error; err != nil {
		t.Fatalf("query execution: %v", err)
	}
	if exec.ScriptSha256 != row2.Sha256 {
		t.Errorf("execution scriptSha256 mismatch: %s vs %s", exec.ScriptSha256, row2.Sha256)
	}
}

func TestScriptListBackfillsLegacyRows(t *testing.T) {
	env := setupBatchEnv(t)
	sh := NewScriptHandler(env.db, env.dir, env.registry)

	r := gin.New()
	r.GET("/scripts", asAdmin(1), sh.HandleList)

	// 存量行：无 sha256 列值（升级前创建）
	content := "-- legacy\n"
	path := filepath.Join(env.dir, "legacy.lua")
	if err := os.WriteFile(path, []byte(content), 0644); err != nil {
		t.Fatal(err)
	}
	env.db.Create(&models.Script{Name: "legacy", Path: "legacy.lua"})

	w := doJSON(r, "GET", "/scripts", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("list: %d %s", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), `"sha256":"`+scripts.Sha256Of([]byte(content))+`"`) {
		t.Errorf("list should backfill sha256, got %s", w.Body.String())
	}
	var row models.Script
	if err := env.db.Where("name = ?", "legacy").First(&row).Error; err != nil {
		t.Fatal(err)
	}
	if row.Sha256 == "" {
		t.Error("backfill should persist sha256 on the row")
	}

	// 文件已删除的行：跳过不动行，不报错
	env.db.Create(&models.Script{Name: "ghost", Path: "ghost.lua"})
	if w := doJSON(r, "GET", "/scripts", nil); w.Code != http.StatusOK {
		t.Errorf("list with ghost row should still 200, got %d", w.Code)
	}
}
