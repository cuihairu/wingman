package handlers

import (
	"net/http"
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

func hashOrFatal(t *testing.T, password string) string {
	t.Helper()
	hash, err := security.HashPassword(password)
	if err != nil {
		t.Fatalf("hash: %v", err)
	}
	return hash
}

func TestAuditLargeSizeAndLANRegion(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	ah := NewAuditHandler(db)
	r := gin.New()
	r.GET("/audit", ah.HandleList)

	// size > 200 → 截断
	w := doJSON(r, "GET", "/audit?page=1&size=9999", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("large size: %d", w.Code)
	}

	// WriteAuditLog 内网 IP → meta 带 ip_region=LAN
	WriteAuditLog(db, "alice", "login_lan", "auth.login", map[string]any{"ip": "192.168.0.5"})
	var rows []models.AuditLog
	if err := db.Where("kind = ?", "login_lan").Find(&rows).Error; err != nil || len(rows) != 1 {
		t.Fatalf("audit row: %v len=%d", err, len(rows))
	}
	if !strings.Contains(rows[0].Meta, `"ip_region":"LAN"`) {
		t.Errorf("expected ip_region LAN in meta: %s", rows[0].Meta)
	}
}

func TestLoginTokenGenerationFailure(t *testing.T) {
	// 不设置 WINGMAN_JWT_SECRET → GenerateToken 失败 → 500
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	user := models.User{Username: "notoken", Password: hashOrFatal(t, "Str0ng!pw"), Role: "viewer", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatal(err)
	}

	ah := NewAuthHandler(db)
	r := gin.New()
	r.POST("/login", ah.HandleLogin)
	w := doJSON(r, "POST", "/login", map[string]any{"username": "notoken", "password": "Str0ng!pw"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("token failure: expected 500, got %d %s", w.Code, w.Body.String())
	}
}

func TestMessagesPageSizeCap(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	user := models.User{Username: "capuser", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatal(err)
	}
	for i := 0; i < 3; i++ {
		if err := db.Create(&models.Message{Recipient: "capuser", Title: "m", Content: "c", Status: "unread"}).Error; err != nil {
			t.Fatal(err)
		}
	}
	mh := NewMessageHandler(db)
	r := gin.New()
	r.GET("/messages", asUser(user), mh.HandleList)
	w := doJSON(r, "GET", "/messages?pageSize=999", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("capped list: %d", w.Code)
	}
}

func TestScriptCreateNonLuaExtension(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	reg, _ := newRegistry(t)
	sh := NewScriptHandler(db, t.TempDir(), reg)
	r := gin.New()
	r.POST("/scripts", asAdmin(1), sh.HandleCreate)
	// name 自带非 .lua 扩展 → Resolve 拒绝 → 400
	w := doJSON(r, "POST", "/scripts", map[string]any{"name": "notes.txt"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("non-lua extension: expected 400, got %d %s", w.Code, w.Body.String())
	}
}

func TestUsersCreateWithExplicitActiveAndDBFailure(t *testing.T) {
	gin.SetMode(gin.TestMode)

	// 显式 active=false
	r, db, _ := setupHandlerRouter(t)
	w := doJSON(r, "POST", "/api/admin/users", map[string]any{
		"username": "inactive1", "password": "Str0ng!pw", "active": false,
	})
	if w.Code != http.StatusCreated {
		t.Fatalf("create inactive: %d %s", w.Code, w.Body.String())
	}
	var created struct {
		User models.SafeUser `json:"user"`
	}
	readJSON(t, w.Body.Bytes(), &created)
	if created.User.Active {
		t.Error("user should be created inactive")
	}
	var stored models.User
	db.First(&stored, "username = ?", "inactive1")
	if stored.Active {
		t.Error("active=false should persist")
	}

	// Create 返回非唯一错误 → 500（query 正常，仅 create 失败）
	failDB := newDB(t)
	if err := rbac.Seed(failDB); err != nil {
		t.Fatal(err)
	}
	failDB.Callback().Create().Before("gorm:create").Register("test:fail-create", func(tx *gorm.DB) {
		tx.AddError(errBoom)
	})
	uh := NewUserHandler(failDB)
	rFail := gin.New()
	rFail.POST("/users", asAdmin(1), uh.HandleCreate)
	w = doJSON(rFail, "POST", "/users", map[string]any{
		"username": "failcreate", "password": "Str0ng!pw", "role": "viewer",
	})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("create failure: expected 500, got %d %s", w.Code, w.Body.String())
	}
}

func TestWorkflowStepStatusRunningTimes(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	reg, hub := newRegistry(t)
	engine := workflow.NewEngine(db, reg, hub, t.TempDir())
	wh := NewWorkflowHandler(engine, db)
	r := gin.New()
	r.GET("/steps/:id/:stepId", asAdmin(1), wh.HandleGetStepStatus)

	wf := models.Workflow{Name: "step-times", Status: "running"}
	if err := db.Create(&wf).Error; err != nil {
		t.Fatal(err)
	}
	// 无 StepStatus 行 → 404
	w := doJSON(r, "GET", "/steps/1/s1", nil)
	if w.Code != http.StatusNotFound {
		t.Errorf("missing step status: expected 404, got %d", w.Code)
	}
}
