package handlers

import (
	"net/http"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agentstore"
)

// 连通测试：HandleSetTags 写库后，新 Registry + agentstore 重新 Register 恢复标签。
func TestSetTagsHandlerPersistsForNewRegistry(t *testing.T) {
	r, db, adminID := setupHandlerRouter(t)
	registry, _ := newRegistry(t)
	registry.SetTagStore(agentstore.NewAgentTagStore(db)) // PUT tags 经此写穿落库
	registry.Register("a1", "host1", "10.0.0.1", nil)
	ah := NewAgentHandler(registry, db)
	grp := r.Group("/api/agents").Use(asAdmin(adminID))
	grp.PUT("/tags-test/:agentId/tags", ah.HandleSetTags)

	if w := doJSON(r, "PUT", "/api/agents/tags-test/a1/tags", map[string]any{"tags": []string{"prod"}}); w.Code != http.StatusOK {
		t.Fatalf("set tags: %d %s", w.Code, w.Body.String())
	}

	// 新注册表模拟 server 重启
	registry2, _ := newRegistry(t)
	registry2.SetTagStore(agentstore.NewAgentTagStore(db))
	registry2.Register("a1", "host1", "10.0.0.1", nil)
	info, ok := registry2.Get("a1")
	if !ok {
		t.Fatal("a1 should exist in new registry")
	}
	if len(info.Tags) != 1 || info.Tags[0] != "prod" {
		t.Errorf("tags should be restored from DB, got %v", info.Tags)
	}
}
