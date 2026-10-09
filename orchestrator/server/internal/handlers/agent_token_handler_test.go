package handlers

import (
	"bytes"
	"fmt"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

func setupTokenRouter(t *testing.T) (*gin.Engine, *agent.TokenStore, *gorm.DB) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	store := agent.NewTokenStore(db)
	h := NewAgentTokenHandler(store, db)

	r := gin.New()
	g := r.Group("/api/agent-tokens").Use(asAdmin(1))
	g.GET("", h.HandleList)
	g.POST("", h.HandleCreate)
	g.DELETE("/:id", h.HandleRevoke)
	return r, store, db
}

func doTokenJSON(r *gin.Engine, method, path string, body any) *httptest.ResponseRecorder {
	var buf bytes.Buffer
	if body != nil {
		_ = json.NewEncoder(&buf).Encode(body)
	}
	req := httptest.NewRequest(method, path, &buf)
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	return w
}

// TestAgentTokenCreateReturnsPlaintextOnce 签发响应含明文（wt_ 前缀），
// 列表与记录不再携带；审计落 agenttoken.create。
func TestAgentTokenCreateReturnsPlaintextOnce(t *testing.T) {
	r, store, db := setupTokenRouter(t)

	w := doTokenJSON(r, http.MethodPost, "/api/agent-tokens", map[string]any{
		"label":   "pixel-8",
		"agentId": "agent-pixel-8",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("create: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Success bool `json:"success"`
		Data    struct {
			Token  string            `json:"token"`
			Record models.AgentToken `json:"record"`
		} `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatalf("unmarshal: %v", err)
	}
	if !resp.Success || !strings.HasPrefix(resp.Data.Token, agent.TokenPrefix) {
		t.Fatalf("response should carry wt_ plaintext once, got %v", resp.Data)
	}
	if resp.Data.Record.AgentID != "agent-pixel-8" || resp.Data.Record.Prefix == "" {
		t.Fatalf("record fields missing: %+v", resp.Data.Record)
	}
	recEncoded, _ := json.Marshal(resp.Data.Record)
	if strings.Contains(string(recEncoded), "tokenHash") {
		t.Fatalf("record JSON must not leak hash: %s", recEncoded)
	}

	// 明文可用（绑定 agent 校验通过）
	if _, ok := store.Verify(resp.Data.Token, "agent-pixel-8"); !ok {
		t.Fatalf("issued token should verify")
	}

	// 列表不带明文/哈希
	w = doTokenJSON(r, http.MethodGet, "/api/agent-tokens", nil)
	body := w.Body.String()
	if strings.Contains(body, resp.Data.Token) || strings.Contains(body, "tokenHash") {
		t.Fatalf("list must not leak plaintext or hash: %s", body)
	}

	// 审计
	var n int64
	db.Model(&models.AuditLog{}).Where("kind = ?", "agenttoken.create").Count(&n)
	if n != 1 {
		t.Fatalf("want 1 create audit, got %d", n)
	}
}

// TestAgentTokenCreateRequiresLabel 空 label 拒绝。
func TestAgentTokenCreateRequiresLabel(t *testing.T) {
	r, _, _ := setupTokenRouter(t)
	w := doTokenJSON(r, http.MethodPost, "/api/agent-tokens", map[string]any{"label": "   "})
	if w.Code != http.StatusBadRequest {
		t.Fatalf("blank label should 400, got %d", w.Code)
	}
}

// TestAgentTokenRevokeIdempotentAndAudited 吊销立即生效、幂等、审计一次。
func TestAgentTokenRevokeIdempotentAndAudited(t *testing.T) {
	r, store, db := setupTokenRouter(t)

	w := doTokenJSON(r, http.MethodPost, "/api/agent-tokens", map[string]any{"label": "t1"})
	var resp struct {
		Data struct {
			Token  string `json:"token"`
			Record struct {
				ID uint `json:"ID"`
			} `json:"record"`
		} `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatalf("unmarshal: %v", err)
	}
	plain := resp.Data.Token

	w = doTokenJSON(r, http.MethodDelete, fmt.Sprintf("/api/agent-tokens/%d", resp.Data.Record.ID), nil)
	if w.Code != http.StatusOK {
		t.Fatalf("revoke: %d %s", w.Code, w.Body.String())
	}
	if _, ok := store.Verify(plain, "any"); ok {
		t.Fatalf("revoked token should not verify")
	}

	// 幂等：二次吊销 success:true、revoked:false
	w = doTokenJSON(r, http.MethodDelete, fmt.Sprintf("/api/agent-tokens/%d", resp.Data.Record.ID), nil)
	if w.Code != http.StatusOK || !strings.Contains(w.Body.String(), `"revoked":false`) {
		t.Fatalf("second revoke should be idempotent success, got %d %s", w.Code, w.Body.String())
	}

	var n int64
	db.Model(&models.AuditLog{}).Where("kind = ?", "agenttoken.revoke").Count(&n)
	if n != 1 {
		t.Fatalf("want 1 revoke audit (idempotent second call not audited), got %d", n)
	}
}

// TestAgentTokenRevokeInvalidID 非法 id 拒绝。
func TestAgentTokenRevokeInvalidID(t *testing.T) {
	r, _, _ := setupTokenRouter(t)
	w := doTokenJSON(r, http.MethodDelete, "/api/agent-tokens/abc", nil)
	if w.Code != http.StatusBadRequest {
		t.Fatalf("invalid id should 400, got %d", w.Code)
	}
}
