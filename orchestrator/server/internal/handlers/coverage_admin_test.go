package handlers

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"github.com/gin-gonic/gin"
)

// httptestBody 构造原始请求（用于非法 JSON body 测试）。
func httptestBody(method, path, body string) *http.Request {
	req := httptest.NewRequest(method, path, strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	return req
}

func requestRaw(r *gin.Engine, req *http.Request) *httptest.ResponseRecorder {
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	return w
}

// ---------- Users 剩余分支 ----------

func TestUsersListFiltersAndPagination(t *testing.T) {
	r, db, adminID := setupHandlerRouter(t)

	// 准备多个用户
	for _, name := range []string{"carol", "chris", "dave"} {
		if err := db.Create(&models.User{Username: name, Password: "x", Role: "viewer", Active: true}).Error; err != nil {
			t.Fatal(err)
		}
	}

	totalOf := func(path string) int64 {
		w := doJSON(r, "GET", path, nil)
		if w.Code != http.StatusOK {
			t.Fatalf("list %s: %d %s", path, w.Code, w.Body.String())
		}
		var resp struct {
			Total int64 `json:"total"`
		}
		json.Unmarshal(w.Body.Bytes(), &resp)
		return resp.Total
	}

	if got := totalOf("/api/admin/users"); got != 4 {
		t.Errorf("expected 4 users, got %d", got)
	}
	if got := totalOf("/api/admin/users?role=viewer"); got != 3 {
		t.Errorf("role filter: expected 3, got %d", got)
	}
	if got := totalOf("/api/admin/users?keyword=car"); got != 1 {
		t.Errorf("keyword filter: expected 1, got %d", got)
	}
	// 无效 page/size → 默认值；超大 size → 截断为 200
	if got := totalOf("/api/admin/users?page=abc&size=xyz"); got != 4 {
		t.Errorf("invalid paging params should fall back, got %d", got)
	}
	if got := totalOf("/api/admin/users?size=9999&page=1"); got != 4 {
		t.Errorf("oversized size should still work, got %d", got)
	}

	_ = adminID
}

func TestUsersGetAndParamValidation(t *testing.T) {
	r, db, _ := setupHandlerRouter(t)
	target := models.User{Username: "gettyme", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&target).Error; err != nil {
		t.Fatal(err)
	}

	// 成功
	w := doJSON(r, "GET", "/api/admin/users/"+itoa(target.ID), nil)
	if w.Code != http.StatusOK {
		t.Fatalf("get user: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Username string `json:"username"`
	}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp.Username != "gettyme" {
		t.Errorf("unexpected user: %s", resp.Username)
	}

	// 非数字 id → 400
	w = doJSON(r, "GET", "/api/admin/users/abc", nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("non-numeric id: expected 400, got %d", w.Code)
	}
	// 0 → 400
	w = doJSON(r, "GET", "/api/admin/users/0", nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("zero id: expected 400, got %d", w.Code)
	}
	// 未知 id → 404
	w = doJSON(r, "GET", "/api/admin/users/424242", nil)
	if w.Code != http.StatusNotFound {
		t.Errorf("unknown id: expected 404, got %d", w.Code)
	}
}

func TestUsersCreateValidationBranches(t *testing.T) {
	r, _, _ := setupHandlerRouter(t)

	cases := []struct {
		name string
		body map[string]any
		code int
	}{
		{"missing fields", map[string]any{"username": "only"}, http.StatusBadRequest},
		{"bad username", map[string]any{"username": "x!", "password": "Str0ng!pw"}, http.StatusBadRequest},
		{"weak password", map[string]any{"username": "validname", "password": "123"}, http.StatusBadRequest},
		{"unknown role", map[string]any{"username": "validname", "password": "Str0ng!pw", "role": "wizard"}, http.StatusBadRequest},
	}
	for _, tc := range cases {
		w := doJSON(r, "POST", "/api/admin/users", tc.body)
		if w.Code != tc.code {
			t.Errorf("%s: expected %d, got %d %s", tc.name, tc.code, w.Code, w.Body.String())
		}
	}
}

func TestUsersCreateDuplicateUsername(t *testing.T) {
	r, db, _ := setupHandlerRouter(t)
	if err := db.Create(&models.User{Username: "dupuser", Password: "x", Role: "viewer", Active: true}).Error; err != nil {
		t.Fatal(err)
	}
	w := doJSON(r, "POST", "/api/admin/users", map[string]any{
		"username": "dupuser", "password": "Str0ng!pw",
	})
	if w.Code != http.StatusConflict {
		t.Errorf("duplicate username: expected 409, got %d %s", w.Code, w.Body.String())
	}
}

func TestUsersUpdateBranches(t *testing.T) {
	r, db, adminID := setupHandlerRouter(t)
	target := models.User{Username: "updateme", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&target).Error; err != nil {
		t.Fatal(err)
	}
	base := "/api/admin/users/" + itoa(target.ID)

	// 非法 JSON → 400
	req := httptestBody("PUT", base, "not-json")
	w := requestRaw(r, req)
	if w.Code != http.StatusBadRequest {
		t.Errorf("invalid body: expected 400, got %d", w.Code)
	}

	// 未知角色 → 400
	w = doJSON(r, "PUT", base, map[string]any{"role": "wizard"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("unknown role: expected 400, got %d", w.Code)
	}

	// 空 updates → 200 原样返回
	w = doJSON(r, "PUT", base, map[string]any{})
	if w.Code != http.StatusOK {
		t.Errorf("empty updates: expected 200, got %d", w.Code)
	}

	// 正常更新 role + active
	w = doJSON(r, "PUT", base, map[string]any{"role": "operator", "active": false})
	if w.Code != http.StatusOK {
		t.Fatalf("update: %d %s", w.Code, w.Body.String())
	}
	var stored models.User
	db.First(&stored, target.ID)
	if stored.Role != "operator" || stored.Active {
		t.Errorf("update not persisted: %+v", stored)
	}

	// self 标记：更新自己
	w = doJSON(r, "PUT", "/api/admin/users/"+itoa(adminID), map[string]any{"active": true})
	if w.Code != http.StatusOK {
		t.Errorf("self update: expected 200, got %d", w.Code)
	}
}

func TestUsersResetPasswordBranches(t *testing.T) {
	r, db, _ := setupHandlerRouter(t)
	target := models.User{Username: "resetpw", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&target).Error; err != nil {
		t.Fatal(err)
	}
	base := "/api/admin/users/" + itoa(target.ID) + "/reset-password"

	// 缺字段 → 400
	w := doJSON(r, "POST", base, map[string]any{})
	if w.Code != http.StatusBadRequest {
		t.Errorf("missing password: expected 400, got %d", w.Code)
	}
	// 弱密码 → 400
	w = doJSON(r, "POST", base, map[string]any{"newPassword": "weak"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("weak password: expected 400, got %d", w.Code)
	}
	// 成功
	w = doJSON(r, "POST", base, map[string]any{"newPassword": "Str0ng!pw2"})
	if w.Code != http.StatusOK {
		t.Fatalf("reset: %d %s", w.Code, w.Body.String())
	}
	var stored models.User
	db.First(&stored, target.ID)
	if !security.VerifyPassword(stored.Password, "Str0ng!pw2") {
		t.Error("new password should verify")
	}
}

func TestUsersDeleteGuards(t *testing.T) {
	r, db, adminID := setupHandlerRouter(t)

	// 删除自己 → 400
	w := doJSON(r, "DELETE", "/api/admin/users/"+itoa(adminID), nil)
	if w.Code != http.StatusBadRequest || !strings.Contains(w.Body.String(), "yourself") {
		t.Errorf("self delete: %d %s", w.Code, w.Body.String())
	}

	// 删除内置 admin（username=admin）→ 400。adminID 就是 admin 本身，已被 self 分支拦截，
	// 构造另一个名为 admin 的用户不可行（唯一约束），用大小写变体验证 EqualFold 分支。
	lowerAdmin := models.User{Username: "Admin", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&lowerAdmin).Error; err != nil {
		t.Fatal(err)
	}
	// 用一个非 self 的 admin 身份删除 lowerAdmin
	otherAdmin := models.User{Username: "root2", Password: "x", Role: "admin", Active: true}
	if err := db.Create(&otherAdmin).Error; err != nil {
		t.Fatal(err)
	}
	uh := NewUserHandler(db)
	r2 := gin.New()
	r2.DELETE("/users/:id", asAdmin(otherAdmin.ID), uh.HandleDelete)
	w = doJSON(r2, "DELETE", "/users/"+itoa(lowerAdmin.ID), nil)
	if w.Code != http.StatusBadRequest || !strings.Contains(w.Body.String(), "builtin admin") {
		t.Errorf("builtin admin guard: %d %s", w.Code, w.Body.String())
	}

	// 正常删除 → 200
	victim := models.User{Username: "victim", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&victim).Error; err != nil {
		t.Fatal(err)
	}
	w = doJSON(r2, "DELETE", "/users/"+itoa(victim.ID), nil)
	if w.Code != http.StatusOK {
		t.Fatalf("delete: %d %s", w.Code, w.Body.String())
	}
	var count int64
	db.Model(&models.User{}).Where("username = ?", "victim").Count(&count)
	if count != 0 {
		t.Error("victim should be deleted")
	}
}

// ---------- Roles 剩余分支 ----------

func TestRolesGetAndFindByCode(t *testing.T) {
	r, _, _ := setupHandlerRouter(t)

	// 成功获取内置角色
	w := doJSON(r, "GET", "/api/admin/roles/viewer", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("get role: %d %s", w.Code, w.Body.String())
	}

	// 未知角色 → 404
	w = doJSON(r, "GET", "/api/admin/roles/wizard", nil)
	if w.Code != http.StatusNotFound {
		t.Errorf("unknown role: expected 404, got %d", w.Code)
	}
}

func TestRolesCreateValidationBranches(t *testing.T) {
	r, _, _ := setupHandlerRouter(t)

	cases := []struct {
		name string
		body map[string]any
		code int
	}{
		{"missing code", map[string]any{"name": "n"}, http.StatusBadRequest},
		{"invalid code", map[string]any{"code": "x"}, http.StatusBadRequest},
		{"reserved admin", map[string]any{"code": "admin"}, http.StatusBadRequest},
		{"unknown permission", map[string]any{"code": "custom1", "permissions": []string{"no:perm"}}, http.StatusBadRequest},
	}
	for _, tc := range cases {
		w := doJSON(r, "POST", "/api/admin/roles", tc.body)
		if w.Code != tc.code {
			t.Errorf("%s: expected %d, got %d %s", tc.name, tc.code, w.Code, w.Body.String())
		}
	}

	// 合法创建带权限
	w := doJSON(r, "POST", "/api/admin/roles", map[string]any{
		"code": "custom2", "name": "Custom", "permissions": []string{" scripts:run "},
	})
	if w.Code != http.StatusCreated {
		t.Fatalf("create role: %d %s", w.Code, w.Body.String())
	}

	// 重复创建 → 409
	w = doJSON(r, "POST", "/api/admin/roles", map[string]any{"code": "custom2"})
	if w.Code != http.StatusConflict {
		t.Errorf("duplicate role: expected 409, got %d", w.Code)
	}
}

func TestRolesUpdateBranches(t *testing.T) {
	r, db, _ := setupHandlerRouter(t)

	// 创建自定义角色
	w := doJSON(r, "POST", "/api/admin/roles", map[string]any{"code": "upd1", "name": "Old"})
	if w.Code != http.StatusCreated {
		t.Fatalf("create: %d", w.Code)
	}

	// 自定义角色：改名 + 描述
	w = doJSON(r, "PUT", "/api/admin/roles/upd1", map[string]any{"name": "New", "description": "d"})
	if w.Code != http.StatusOK {
		t.Fatalf("update: %d %s", w.Code, w.Body.String())
	}
	var role models.Role
	db.Where("code = ?", "upd1").First(&role)
	if role.Name != "New" {
		t.Errorf("role name not updated: %+v", role)
	}

	// 非法 body → 400
	req := httptestBody("PUT", "/api/admin/roles/upd1", "not-json")
	w = requestRaw(r, req)
	if w.Code != http.StatusBadRequest {
		t.Errorf("invalid body: expected 400, got %d", w.Code)
	}

	// admin 角色提交权限变更：应被忽略（不报错、不替换为具体权限）
	w = doJSON(r, "PUT", "/api/admin/roles/admin", map[string]any{"permissions": []string{"scripts:run"}})
	if w.Code != http.StatusOK {
		t.Fatalf("admin role update: %d %s", w.Code, w.Body.String())
	}

	// 未知权限 → 400
	w = doJSON(r, "PUT", "/api/admin/roles/upd1", map[string]any{"permissions": []string{"nope:nothing"}})
	if w.Code != http.StatusBadRequest {
		t.Errorf("unknown permission: expected 400, got %d", w.Code)
	}

	// 清空权限（空数组 → Clear）
	w = doJSON(r, "PUT", "/api/admin/roles/upd1", map[string]any{"permissions": []string{}})
	if w.Code != http.StatusOK {
		t.Fatalf("clear permissions: %d %s", w.Code, w.Body.String())
	}
	var permCount int64
	db.Model(&models.Permission{}).
		Joins("JOIN role_permissions ON role_permissions.permission_id = permissions.id").
		Where("role_permissions.role_id = ?", role.ID).Count(&permCount)
	if permCount != 0 {
		t.Errorf("permissions should be cleared, got %d", permCount)
	}
}

func TestRolesListPermissionsCategoryFilter(t *testing.T) {
	r, _, _ := setupHandlerRouter(t)

	w := doJSON(r, "GET", "/api/admin/permissions", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("permissions: %d", w.Code)
	}
	var all struct {
		Total int `json:"total"`
	}
	json.Unmarshal(w.Body.Bytes(), &all)

	// category 过滤
	w = doJSON(r, "GET", "/api/admin/permissions?category=script", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("permissions filtered: %d", w.Code)
	}
	var filtered struct {
		Items []models.Permission `json:"items"`
		Total int                 `json:"total"`
	}
	json.Unmarshal(w.Body.Bytes(), &filtered)
	if filtered.Total == 0 || filtered.Total >= all.Total {
		t.Errorf("category filter should narrow results: filtered=%d all=%d", filtered.Total, all.Total)
	}
	for _, p := range filtered.Items {
		if p.Category != "script" {
			t.Errorf("non-script item leaked: %+v", p)
		}
	}
}

// ---------- Profile 剩余分支 ----------

func TestProfileGamesAndPermissions(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	if err := db.Create(&models.User{Username: "pviewer", Password: "x", Role: "viewer", Active: true}).Error; err != nil {
		t.Fatal(err)
	}
	var viewer models.User
	db.First(&viewer, "username = ?", "pviewer")

	ph := NewProfileHandler(db)
	r := gin.New()
	r.GET("/games", asUser(viewer), ph.HandleGetGames)
	r.GET("/perms", asUser(viewer), ph.HandleGetPermissions)

	// games
	w := doJSON(r, "GET", "/games", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("games: %d", w.Code)
	}

	// viewer 权限
	w = doJSON(r, "GET", "/perms", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("viewer perms: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Permissions   []gin.H  `json:"permissions"`
		Admin         bool     `json:"admin"`
		PermissionIDs []string `json:"permissionIDs"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatal(err)
	}
	if resp.Admin {
		t.Error("viewer should not be admin")
	}
	for _, id := range resp.PermissionIDs {
		if id == "*" {
			t.Error("viewer must not receive wildcard")
		}
	}

	// admin 权限：应带 wildcard
	var admin models.User
	db.First(&admin, "username = ?", "admin")
	if admin.ID == 0 {
		if err := db.Create(&models.User{Username: "admin2", Password: "x", Role: "admin", Active: true}).Error; err != nil {
			t.Fatal(err)
		}
		db.First(&admin, "username = ?", "admin2")
	}
	rAdmin := gin.New()
	rAdmin.GET("/perms", asUser(admin), ph.HandleGetPermissions)
	w = doJSON(rAdmin, "GET", "/perms", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("admin perms: %d", w.Code)
	}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if !resp.Admin {
		t.Error("admin flag expected")
	}
	if len(resp.PermissionIDs) == 0 || resp.PermissionIDs[0] != "*" {
		t.Errorf("admin permissionIDs should start with wildcard: %v", resp.PermissionIDs)
	}
}

func TestProfileUpdatePasswordBranches(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	hash, err := security.HashPassword("Str0ng!pw")
	if err != nil {
		t.Fatal(err)
	}
	user := models.User{Username: "pwuser", Password: hash, Role: "viewer", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatal(err)
	}

	ph := NewProfileHandler(db)
	r := gin.New()
	r.PUT("/password", asUser(user), ph.HandleUpdatePassword)

	// 缺字段 → 400
	w := doJSON(r, "PUT", "/password", map[string]any{"oldPassword": "Str0ng!pw"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("missing new: expected 400, got %d", w.Code)
	}
	// 旧密码错误 → 401
	w = doJSON(r, "PUT", "/password", map[string]any{"oldPassword": "WrongPass9!", "newPassword": "Str0ng!new"})
	if w.Code != http.StatusUnauthorized {
		t.Errorf("wrong old: expected 401, got %d", w.Code)
	}
	// 新密码弱 → 400
	w = doJSON(r, "PUT", "/password", map[string]any{"oldPassword": "Str0ng!pw", "newPassword": "weak"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("weak new: expected 400, got %d", w.Code)
	}
	// 成功 → 200
	w = doJSON(r, "PUT", "/password", map[string]any{"oldPassword": "Str0ng!pw", "newPassword": "Str0ng!new"})
	if w.Code != http.StatusOK {
		t.Fatalf("update password: %d %s", w.Code, w.Body.String())
	}
	var stored models.User
	db.First(&stored, user.ID)
	if !security.VerifyPassword(stored.Password, "Str0ng!new") {
		t.Error("password should be re-hashed")
	}
}

func TestProfileCurrentUserMissing(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	ph := NewProfileHandler(db)
	r := gin.New()
	// 无 user_id 注入 → 401
	r.GET("/profile", ph.HandleGetProfile)
	r.PUT("/profile", ph.HandleUpdateProfile)
	r.GET("/perms", ph.HandleGetPermissions)
	r.PUT("/password", ph.HandleUpdatePassword)

	for _, tc := range []struct{ method, path string }{
		{"GET", "/profile"}, {"PUT", "/profile"},
		{"GET", "/perms"}, {"PUT", "/password"},
	} {
		w := doJSON(r, tc.method, tc.path, map[string]any{})
		if w.Code != http.StatusUnauthorized {
			t.Errorf("%s %s without identity: expected 401, got %d", tc.method, tc.path, w.Code)
		}
	}

	// update profile 非法 JSON → 400
	user := models.User{Username: "u1", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatal(err)
	}
	rAuth := gin.New()
	rAuth.PUT("/profile", asUser(user), ph.HandleUpdateProfile)
	req := httptestBody("PUT", "/profile", "not-json")
	w := requestRaw(rAuth, req)
	if w.Code != http.StatusBadRequest {
		t.Errorf("invalid body: expected 400, got %d", w.Code)
	}
}

// ---------- Messages 剩余分支 ----------

func TestMessagesMarkAllReadAndListFilters(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	user := models.User{Username: "msguser", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatal(err)
	}
	mh := NewMessageHandler(db)
	r := gin.New()
	r.GET("/messages", asUser(user), mh.HandleList)
	r.GET("/unread", asUser(user), mh.HandleUnreadCount)
	r.POST("/read-all", asUser(user), mh.HandleMarkAllRead)
	r.POST("/read/:id", asUser(user), mh.HandleMarkRead)

	for _, title := range []string{"m1", "m2", "m3"} {
		if err := db.Create(&models.Message{Recipient: "msguser", Title: title, Content: "c", Status: "unread"}).Error; err != nil {
			t.Fatal(err)
		}
	}
	// 他人消息不可见
	if err := db.Create(&models.Message{Recipient: "other", Title: "secret", Content: "c", Status: "unread"}).Error; err != nil {
		t.Fatal(err)
	}

	// invalid message id → 400
	w := doJSON(r, "POST", "/read/abc", nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("invalid id: expected 400, got %d", w.Code)
	}

	// status=read 过滤（先读一条）
	var first models.Message
	db.First(&first, "title = ?", "m1")
	w = doJSON(r, "POST", "/read/"+itoa(first.ID), nil)
	if w.Code != http.StatusOK {
		t.Fatalf("mark read: %d", w.Code)
	}
	w = doJSON(r, "GET", "/messages?status=read", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("list read: %d", w.Code)
	}
	var resp struct {
		Items []map[string]any `json:"items"`
		Total int64            `json:"total"`
	}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp.Total != 1 || resp.Items[0]["title"] != "m1" {
		t.Errorf("read filter: %+v", resp)
	}

	// size 参数截断（pageSize 优先，单独传 size 也生效）
	w = doJSON(r, "GET", "/messages?size=2", nil)
	json.Unmarshal(w.Body.Bytes(), &resp)
	if len(resp.Items) != 2 {
		t.Errorf("size param should cap items, got %d", len(resp.Items))
	}
	w = doJSON(r, "GET", "/messages?pageSize=1", nil)
	json.Unmarshal(w.Body.Bytes(), &resp)
	if len(resp.Items) != 1 {
		t.Errorf("pageSize param should cap items, got %d", len(resp.Items))
	}

	// 全部已读
	w = doJSON(r, "POST", "/read-all", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("mark all: %d %s", w.Code, w.Body.String())
	}
	w = doJSON(r, "GET", "/unread", nil)
	var cnt struct {
		Count int64 `json:"count"`
	}
	json.Unmarshal(w.Body.Bytes(), &cnt)
	if cnt.Count != 0 {
		t.Errorf("all should be read, got %d", cnt.Count)
	}
}

func TestMessagesAdminVisibility(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	admin := models.User{Username: "msgadmin", Password: "x", Role: "admin", Active: true}
	if err := db.Create(&admin).Error; err != nil {
		t.Fatal(err)
	}
	if err := db.Create(&models.Message{Recipient: "someone-else", Title: "hidden", Content: "c", Status: "unread"}).Error; err != nil {
		t.Fatal(err)
	}

	mh := NewMessageHandler(db)
	r := gin.New()
	r.GET("/messages", asUser(admin), mh.HandleList)

	w := doJSON(r, "GET", "/messages", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("admin list: %d", w.Code)
	}
	var resp struct {
		Total int64 `json:"total"`
	}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp.Total != 1 {
		t.Errorf("admin should see all messages, got %d", resp.Total)
	}
}
