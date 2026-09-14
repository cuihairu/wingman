package handlers

import (
	"encoding/json"
	"net/http"
	"strings"
	"testing"

	agentPkg "github.com/cuihaitao/wingman/orchestrator/server/pkg/agent"
	"github.com/gin-gonic/gin"
)

// setupTeamRouter 装配仅含 POST /teams 的路由（asAdmin 绕过 JWT）。
func setupTeamRouter(t *testing.T) (*gin.Engine, *agentPkg.TeamManager) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	tm := agentPkg.NewTeamManager()
	r := gin.New()
	r.POST("/api/teams", asAdmin(7), NewTeamHandler(tm).HandleCreate)
	return r, tm
}

func TestTeamHandlerCreateSuccess(t *testing.T) {
	r, tm := setupTeamRouter(t)

	w := doJSON(r, "POST", "/api/teams", map[string]any{
		"name":        "  night-farm  ",
		"description": "夜间挂机协同",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("create team: expected 200, got %d %s", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), `"success":true`) {
		t.Errorf("expected success:true, got %s", w.Body.String())
	}

	// 经 TeamManager 读回：创建者（admin，user_id=7）即 leader，名称已去除首尾空白
	var resp struct {
		Data map[string]any `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil || resp.Data["teamId"] == nil {
		t.Fatalf("decode create response: %v (%s)", err, w.Body.String())
	}
	teamID, _ := resp.Data["teamId"].(string)
	info, err := tm.GetTeamInfo(teamID)
	if err != nil {
		t.Fatal(err)
	}
	if info["name"] != "night-farm" {
		t.Errorf("name should be trimmed, got %v", info["name"])
	}
	if info["leaderId"] != "admin" {
		t.Errorf("creator should be leader, got %v", info["leaderId"])
	}
	if info["description"] != "夜间挂机协同" {
		t.Errorf("description mismatch: %v", info["description"])
	}
	if !strings.HasPrefix(teamID, "team_") {
		t.Errorf("teamId should have team_ prefix, got %s", teamID)
	}
}

func TestTeamHandlerCreateRejectsBlankName(t *testing.T) {
	r, _ := setupTeamRouter(t)

	w := doJSON(r, "POST", "/api/teams", map[string]any{"name": "   "})
	if w.Code != http.StatusBadRequest {
		t.Fatalf("blank name: expected 400, got %d %s", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), "team name is required") {
		t.Errorf("unexpected error message: %s", w.Body.String())
	}
}

func TestTeamHandlerCreateRejectsMissingBody(t *testing.T) {
	r, _ := setupTeamRouter(t)

	// 缺少必填 name 字段 → binding 失败
	w := doJSON(r, "POST", "/api/teams", map[string]any{"description": "no name"})
	if w.Code != http.StatusBadRequest {
		t.Fatalf("missing name: expected 400, got %d %s", w.Code, w.Body.String())
	}
	// 非法 JSON
	w = doJSON(r, "POST", "/api/teams", nil)
	// body 为 nil 时 doJSON 发送空 body，ShouldBindJSON 应报 EOF → 400
	if w.Code != http.StatusBadRequest {
		t.Fatalf("empty body: expected 400, got %d %s", w.Code, w.Body.String())
	}
}
