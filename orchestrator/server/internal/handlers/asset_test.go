package handlers

import (
	"encoding/base64"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/assets"
	"github.com/gin-gonic/gin"
)

// assetTestEnv 资产 handler 测试环境（路由装配与 batch 测试同构）
type assetTestEnv struct {
	r        *gin.Engine
	dir      string
	store    *assets.Store
	registry *agent.Registry
	conns    map[string]*handlerMockConn
}

func setupAssetEnv(t *testing.T) *assetTestEnv {
	t.Helper()
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	dir := t.TempDir()
	reg, _ := newRegistry(t)
	store := assets.NewStore(filepath.Join(dir, "assets"))
	ah := NewAssetHandler(db, store, reg)

	r := gin.New()
	readGrp := r.Group("/api").Use(asAdmin(1))
	{
		readGrp.GET("/assets", ah.HandleList)
		readGrp.GET("/assets/content", ah.HandleContent)
	}
	writeGrp := r.Group("/api").Use(asAdmin(1))
	{
		writeGrp.POST("/assets", ah.HandleUpload)
		writeGrp.DELETE("/assets", ah.HandleDelete)
		writeGrp.POST("/assets/sync", ah.HandleSync)
	}

	return &assetTestEnv{r: r, dir: dir, store: store, registry: reg, conns: map[string]*handlerMockConn{}}
}

func (e *assetTestEnv) addAgent(id string, tags ...string) *handlerMockConn {
	conn := &handlerMockConn{}
	e.conns[id] = conn
	e.registry.Register(id, id+"-host", "10.0.0.1", conn)
	if len(tags) > 0 {
		if !e.registry.SetTags(id, tags) {
			panic("SetTags failed for " + id)
		}
	}
	return conn
}

func TestAssetUploadListContentDelete(t *testing.T) {
	env := setupAssetEnv(t)

	content := base64.StdEncoding.EncodeToString([]byte("png-bytes-1"))
	w := doJSON(env.r, "POST", "/api/assets", map[string]any{
		"name":          "btn.png",
		"contentBase64": content,
	})
	if w.Code != http.StatusOK {
		t.Fatalf("upload: %d %s", w.Code, w.Body.String())
	}
	if _, err := os.Stat(filepath.Join(env.store.Dir(), "btn.png")); err != nil {
		t.Fatalf("content file should exist: %v", err)
	}

	// 非法名拒绝
	w = doJSON(env.r, "POST", "/api/assets", map[string]any{
		"name":          "../evil.png",
		"contentBase64": content,
	})
	if w.Code != http.StatusBadRequest {
		t.Errorf("bad name should 400, got %d", w.Code)
	}
	// 坏 base64 拒绝
	w = doJSON(env.r, "POST", "/api/assets", map[string]any{
		"name":          "ok.png",
		"contentBase64": "!!!not-base64!!!",
	})
	if w.Code != http.StatusBadRequest {
		t.Errorf("bad base64 should 400, got %d", w.Code)
	}

	// 列表
	w = doJSON(env.r, "GET", "/api/assets", nil)
	if w.Code != http.StatusOK || !strings.Contains(w.Body.String(), `"name":"btn.png"`) {
		t.Errorf("list should contain btn.png: %d %s", w.Code, w.Body.String())
	}

	// 内容下载 = 原始字节
	w = doJSON(env.r, "GET", "/api/assets/content?name=btn.png", nil)
	if w.Code != http.StatusOK || w.Body.String() != "png-bytes-1" {
		t.Errorf("content should be raw bytes, got %d %q", w.Code, w.Body.String())
	}

	// 删除
	w = doJSON(env.r, "DELETE", "/api/assets?name=btn.png", nil)
	if w.Code != http.StatusOK {
		t.Errorf("delete: %d %s", w.Code, w.Body.String())
	}
	if _, _, err := env.store.Get("btn.png"); err == nil {
		t.Error("asset should be gone after delete")
	}
}

func TestAssetSyncFanout(t *testing.T) {
	env := setupAssetEnv(t)
	online := env.addAgent("a1", "prod")
	env.addAgent("a2")
	offline := env.addAgent("a3", "prod")
	// a3 置为离线（Client 保留但状态非 online）
	env.registry.UpdateStatus("a3", string(agent.StatusOffline), nil)

	if _, err := env.store.Save("tpl.png", []byte("tpl-bytes")); err != nil {
		t.Fatalf("seed asset: %v", err)
	}

	w := doJSON(env.r, "POST", "/api/assets/sync", map[string]any{
		"name": "tpl.png",
		"tags": []string{"prod"},
	})
	if w.Code != http.StatusOK {
		t.Fatalf("sync: %d %s", w.Code, w.Body.String())
	}
	summary := decodeSummary(t, w.Body.String())
	if summary.Total != 2 || summary.Succeeded != 1 || summary.Failed != 1 {
		t.Fatalf("unexpected summary: %+v", summary)
	}
	for _, r := range summary.Results {
		if r.AgentID == "a1" && !r.Success {
			t.Errorf("a1 should succeed: %+v", r)
		}
		if r.AgentID == "a3" && (r.Success || r.Error != "agent offline") {
			t.Errorf("a3 should be offline: %+v", r)
		}
	}

	// a1 收到 asset.sync，payload 含名称/内容 base64/sha256
	cmds := online.dispatchedCommands()
	if len(cmds) != 1 || cmds[0].Method != "asset.sync" {
		t.Fatalf("a1 should receive asset.sync, got %+v", cmds)
	}
	if cmds[0].Data["name"] != "tpl.png" {
		t.Errorf("payload name: %v", cmds[0].Data["name"])
	}
	if cmds[0].Data["contentBase64"] != base64.StdEncoding.EncodeToString([]byte("tpl-bytes")) {
		t.Errorf("payload content mismatch: %v", cmds[0].Data["contentBase64"])
	}
	_, meta, _ := env.store.Get("tpl.png")
	if cmds[0].Data["sha256"] != meta.Sha256 {
		t.Errorf("payload sha256 mismatch: %v", cmds[0].Data["sha256"])
	}

	// 资产不存在 → 404
	w = doJSON(env.r, "POST", "/api/assets/sync", map[string]any{
		"name": "missing.png", "agentIds": []string{"a1"},
	})
	if w.Code != http.StatusNotFound {
		t.Errorf("missing asset sync should 404, got %d", w.Code)
	}
	// 空选择器 → 400
	w = doJSON(env.r, "POST", "/api/assets/sync", map[string]any{"name": "tpl.png"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("empty selector should 400, got %d", w.Code)
	}
	// 零命中 → 200 total=0
	w = doJSON(env.r, "POST", "/api/assets/sync", map[string]any{
		"name": "tpl.png", "agentIds": []string{"nobody"},
	})
	if w.Code != http.StatusOK {
		t.Errorf("zero match should 200, got %d", w.Code)
	}
	if s := decodeSummary(t, w.Body.String()); s.Total != 0 {
		t.Errorf("zero match total should be 0: %+v", s)
	}
	_ = offline
}
