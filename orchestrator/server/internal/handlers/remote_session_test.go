package handlers

// 远程桌面会话审计报表 handler 测试（设计 §11 P1「审计报表呈现」）：
// 落库契约（UTC 归一/时长口径/终态枚举）、多维过滤、汇总与分组聚合、
// 时间分桶、分页口径、SQL 注入防护、失败路径 500。

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// newSessionEnv 构造报表路由（admin 身份直注入）。
func newSessionEnv(t *testing.T) (*gin.Engine, *gorm.DB) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	h := NewRemoteSessionHandler(db)
	r := gin.New()
	r.GET("/api/remote/sessions", asAdmin(1), h.HandleList)
	return r, db
}

// sessionListResp 列表响应（按报表契约解析）。
type sessionListResp struct {
	Success bool                    `json:"success"`
	Data    []remoteSessionResponse `json:"data"`
	Total   int64                   `json:"total"`
	Page    int                     `json:"page"`
	Size    int                     `json:"size"`
	Summary remoteSessionSummary    `json:"summary"`
	Groups  []remoteSessionGroup    `json:"groups"`
	Buckets []remoteSessionBucket   `json:"buckets"`
	Error   string                  `json:"error"`
}

func querySessions(t *testing.T, r *gin.Engine, query string) sessionListResp {
	t.Helper()
	w := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodGet, "/api/remote/sessions"+query, nil)
	r.ServeHTTP(w, req)
	if w.Code != http.StatusOK {
		t.Fatalf("sessions%s: got %d want 200 (%s)", query, w.Code, w.Body.String())
	}
	var resp sessionListResp
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode %s: %v", w.Body.String(), err)
	}
	return resp
}

// seedSession 落一行会话审计（绕开 RecordRemoteSession 以便构造任意时间）。
func seedSession(t *testing.T, db *gorm.DB, rec models.RemoteSessionAudit) {
	t.Helper()
	if err := db.Create(&rec).Error; err != nil {
		t.Fatalf("seed session: %v", err)
	}
}

func mustTime(t *testing.T, layout, value string) time.Time {
	t.Helper()
	parsed, err := time.Parse(time.RFC3339, value)
	if err != nil {
		t.Fatalf("parse time %q: %v", value, err)
	}
	return parsed
}

// ---------- 落库契约（RecordRemoteSession） ----------

func TestRecordRemoteSessionNormalizesAndComputesDuration(t *testing.T) {
	db := newDB(t)
	// 非 UTC 输入：应被归一到 UTC，且 durationMs 由起止重算
	zone := time.FixedZone("CST", 8*3600)
	localStart := time.Date(2026, 9, 25, 20, 0, 0, 0, zone)
	localEnd := localStart.Add(90 * time.Second)

	RecordRemoteSession(db, models.RemoteSessionAudit{
		SessionID: "sess-1", AgentID: "agent-a", Operator: "alice",
		Protocol: "rdp", Host: "10.0.0.5", Port: 3389, ReadOnly: false, Record: true,
		RecordingName: "agent-a-sess-1.mjs",
		StartedAt:     localStart,
		EndedAt:       &localEnd,
		DurationMs:    0, // 调用方没算，应由 helper 重算
	})

	var got models.RemoteSessionAudit
	if err := db.Where("session_id = ?", "sess-1").First(&got).Error; err != nil {
		t.Fatalf("load: %v", err)
	}
	if got.StartedAt.Location() != time.UTC {
		t.Errorf("StartedAt should be UTC, got %v", got.StartedAt.Location())
	}
	if got.EndedAt == nil || got.EndedAt.Location() != time.UTC {
		t.Errorf("EndedAt should be UTC non-nil, got %v", got.EndedAt)
	}
	if got.DurationMs != 90000 {
		t.Errorf("DurationMs: got %d want 90000", got.DurationMs)
	}
	if got.Status != models.RemoteSessionStatusClosed {
		t.Errorf("default status should be closed, got %q", got.Status)
	}
	if got.TableName() != "remote_session_audits" {
		t.Errorf("TableName: got %q", got.TableName())
	}
}

func TestRecordRemoteSessionDefaultsAndNilDB(t *testing.T) {
	db := newDB(t)
	// StartedAt 缺省 → 用当前时刻；EndedAt nil（failed 场景）；
	// 显式 DurationMs 为正时不被覆盖
	RecordRemoteSession(db, models.RemoteSessionAudit{
		SessionID: "sess-fail", AgentID: "agent-b", Operator: "bob",
		Protocol: "ssh", Status: models.RemoteSessionStatusFailed,
		FailReason: "guacd unreachable", DurationMs: 42,
	})
	var got models.RemoteSessionAudit
	if err := db.Where("session_id = ?", "sess-fail").First(&got).Error; err != nil {
		t.Fatalf("load: %v", err)
	}
	if got.StartedAt.IsZero() {
		t.Error("StartedAt should default to now")
	}
	if got.EndedAt != nil {
		t.Errorf("EndedAt should stay nil, got %v", got.EndedAt)
	}
	if got.DurationMs != 42 {
		t.Errorf("explicit DurationMs must be kept, got %d", got.DurationMs)
	}
	if got.Status != models.RemoteSessionStatusFailed {
		t.Errorf("Status: got %q", got.Status)
	}

	// nil db 不 panic（best-effort 语义）
	RecordRemoteSession(nil, models.RemoteSessionAudit{SessionID: "x"})
}

func TestRecordRemoteSessionWriteFailureIsNonFatal(t *testing.T) {
	db := newDB(t)
	// 删表制造写失败：审计写不进去不得让用户的会话流程中断（best-effort）
	if err := db.Migrator().DropTable(&models.RemoteSessionAudit{}); err != nil {
		t.Fatalf("drop table: %v", err)
	}
	RecordRemoteSession(db, models.RemoteSessionAudit{
		SessionID: "sess-x", AgentID: "a", Operator: "u", Protocol: "vnc", Status: "closed",
	})
	// 落库确实失败了，但没有 panic 传播到调用方
	var count int64
	db.Model(&models.RemoteSessionAudit{}).Count(&count)
	if count != 0 {
		t.Errorf("expected no row after failed write, got %d", count)
	}
}

// ---------- 列表与过滤 ----------

func TestSessionListEmptyIsClean(t *testing.T) {
	r, _ := newSessionEnv(t)
	resp := querySessions(t, r, "")
	if resp.Total != 0 || len(resp.Data) != 0 {
		t.Fatalf("expected empty list, got total=%d data=%d", resp.Total, len(resp.Data))
	}
	// 空集时聚合返回空数组而非 null（前端可直接 map）
	if resp.Groups == nil || resp.Buckets == nil {
		t.Errorf("groups/buckets must be [] not null: %+v", resp)
	}
	if resp.Page != 1 || resp.Size != 20 {
		t.Errorf("default paging: page=%d size=%d", resp.Page, resp.Size)
	}
}

func TestSessionListSortsByStartedAtDesc(t *testing.T) {
	r, db := newSessionEnv(t)
	base := mustTime(t, time.RFC3339, "2026-09-25T10:00:00Z")
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "old", AgentID: "a", Operator: "u", Protocol: "rdp", Status: "closed", StartedAt: base, DurationMs: 1000})
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "new", AgentID: "a", Operator: "u", Protocol: "rdp", Status: "closed", StartedAt: base.Add(time.Hour), DurationMs: 2000})

	resp := querySessions(t, r, "")
	if resp.Total != 2 {
		t.Fatalf("total: got %d want 2", resp.Total)
	}
	if resp.Data[0].SessionID != "new" || resp.Data[1].SessionID != "old" {
		t.Errorf("must sort started_at desc, got %s then %s", resp.Data[0].SessionID, resp.Data[1].SessionID)
	}
}

func TestSessionListFilters(t *testing.T) {
	r, db := newSessionEnv(t)
	base := mustTime(t, time.RFC3339, "2026-09-25T10:00:00Z")
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "s1", AgentID: "agent-1", Operator: "alice", Protocol: "rdp", ReadOnly: false, Record: true, Status: "closed", StartedAt: base, DurationMs: 1000})
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "s2", AgentID: "agent-2", Operator: "bob", Protocol: "vnc", ReadOnly: true, Status: "failed", FailReason: "guacd unreachable", StartedAt: base.Add(time.Hour), DurationMs: 0})
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "s3", AgentID: "agent-1", Operator: "bob", Protocol: "ssh", ReadOnly: true, Record: true, Status: "closed", StartedAt: base.Add(2 * time.Hour), DurationMs: 3000})

	for _, tc := range []struct {
		name  string
		query string
		want  []string
	}{
		{"by agentId", "?agentId=agent-1", []string{"s1", "s3"}},
		{"by protocol", "?protocol=vnc", []string{"s2"}},
		{"by operator", "?operator=bob", []string{"s2", "s3"}},
		{"by status", "?status=failed", []string{"s2"}},
		{"by mode view", "?mode=view", []string{"s2", "s3"}},
		{"by mode control", "?mode=control", []string{"s1"}},
		{"by record", "?record=true", []string{"s1", "s3"}},
		{"combined", "?agentId=agent-1&mode=view&record=true", []string{"s3"}},
		{"start bound", "?start=2026-09-25T11:00:00Z", []string{"s2", "s3"}},
		{"end bound", "?end=2026-09-25T10:30:00Z", []string{"s1"}},
		{"start+end range", "?start=2026-09-25T10:30:00Z&end=2026-09-25T11:30:00Z", []string{"s2"}},
		{"no match", "?agentId=nope", nil},
		{"illegal mode ignored", "?mode=bogus", []string{"s1", "s2", "s3"}},
		{"record=1 aliases true", "?record=1", []string{"s1", "s3"}},
		{"record=false is not a filter", "?record=false", []string{"s1", "s2", "s3"}},
		{"illegal protocol just matches nothing", "?protocol=ftp", nil},
		{"blank values ignored", "?agentId=&operator=&status=", []string{"s1", "s2", "s3"}},
		{"illegal time ignored", "?start=not-a-time", []string{"s1", "s2", "s3"}},
	} {
		t.Run(tc.name, func(t *testing.T) {
			resp := querySessions(t, r, tc.query)
			if len(resp.Data) != len(tc.want) {
				t.Fatalf("got %d rows want %d: %+v", len(resp.Data), len(tc.want), resp.Data)
			}
			for _, want := range tc.want {
				found := false
				for _, item := range resp.Data {
					if item.SessionID == want {
						found = true
					}
				}
				if !found {
					t.Errorf("missing session %s in %+v", want, resp.Data)
				}
			}
			// 过滤口径必须与 total/汇总一致（报表最常见的错误就是分叉）
			if int64(len(resp.Data)) != resp.Total {
				t.Errorf("total %d != rows %d", resp.Total, len(resp.Data))
			}
		})
	}
}

func TestSessionListMapsAllColumns(t *testing.T) {
	r, db := newSessionEnv(t)
	started := mustTime(t, time.RFC3339, "2026-09-25T10:00:00Z")
	ended := started.Add(2 * time.Minute)
	seedSession(t, db, models.RemoteSessionAudit{
		SessionID: "sess-full", AgentID: "agent-x", Operator: "carol",
		Protocol: "ssh", Host: "10.0.0.9", Port: 22,
		ReadOnly: true, Record: true, RecordingName: "agent-x-sess-full.mjs",
		Status: "closed", StartedAt: started, EndedAt: &ended, DurationMs: 120000,
	})
	seedSession(t, db, models.RemoteSessionAudit{
		SessionID: "sess-fail", AgentID: "agent-y", Operator: "dave",
		Protocol: "rdp", Host: "10.0.0.8", Port: 3389,
		Status: "failed", FailReason: "guacd unreachable", StartedAt: started,
	})

	resp := querySessions(t, r, "")
	byID := map[string]remoteSessionResponse{}
	for _, item := range resp.Data {
		byID[item.SessionID] = item
	}
	full := byID["sess-full"]
	if full.ID == 0 || full.AgentID != "agent-x" || full.Operator != "carol" ||
		full.Protocol != "ssh" || full.Host != "10.0.0.9" || full.Port != 22 ||
		!full.ReadOnly || !full.Record || full.RecordingName != "agent-x-sess-full.mjs" ||
		full.DurationMs != 120000 {
		t.Errorf("column mapping incomplete: %+v", full)
	}
	if full.StartedAt != "2026-09-25T10:00:00Z" {
		t.Errorf("StartedAt RFC3339: %q", full.StartedAt)
	}
	if full.EndedAt != "2026-09-25T10:02:00Z" {
		t.Errorf("EndedAt RFC3339: %q", full.EndedAt)
	}
	// failed 行无 EndedAt / 无 recordingName（omitempty）
	failed := byID["sess-fail"]
	if failed.EndedAt != "" || failed.RecordingName != "" || failed.FailReason != "guacd unreachable" {
		t.Errorf("failed row mapping: %+v", failed)
	}
}

// ---------- 汇总 ----------

func TestSessionSummaryAggregates(t *testing.T) {
	r, db := newSessionEnv(t)
	base := mustTime(t, time.RFC3339, "2026-09-25T10:00:00Z")
	// 2 closed（监看+接管，其中一段有录像）+ 1 failed
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "c1", AgentID: "a", Operator: "alice", Protocol: "rdp", ReadOnly: false, Record: true, Status: "closed", StartedAt: base, DurationMs: 5000})
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "c2", AgentID: "a", Operator: "alice", Protocol: "rdp", ReadOnly: true, Status: "closed", StartedAt: base, DurationMs: 3000})
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "f1", AgentID: "b", Operator: "bob", Protocol: "vnc", ReadOnly: false, Status: "failed", StartedAt: base, DurationMs: 0})

	resp := querySessions(t, r, "")
	want := remoteSessionSummary{Total: 3, Closed: 2, Failed: 1, Recorded: 1, Control: 2, ViewOnly: 1, TotalMsSum: 8000}
	if resp.Summary != want {
		t.Errorf("summary: got %+v want %+v", resp.Summary, want)
	}
	// failed 时长不计入（连 guacd 都拨不通，无像素面时长）
	if resp.Summary.TotalMsSum != 8000 {
		t.Errorf("failed duration must not count: %d", resp.Summary.TotalMsSum)
	}
}

func TestSessionSummaryEmptyOnNoRows(t *testing.T) {
	r, _ := newSessionEnv(t)
	if got := querySessions(t, r, "").Summary; got != (remoteSessionSummary{}) {
		t.Errorf("empty summary should be zero value, got %+v", got)
	}
}

// ---------- 分组 ----------

func TestSessionGroupsByDimension(t *testing.T) {
	r, db := newSessionEnv(t)
	base := mustTime(t, time.RFC3339, "2026-09-25T10:00:00Z")
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "1", AgentID: "a1", Operator: "alice", Protocol: "rdp", Status: "closed", StartedAt: base, DurationMs: 1000})
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "2", AgentID: "a1", Operator: "alice", Protocol: "rdp", Status: "closed", StartedAt: base, DurationMs: 2000})
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "3", AgentID: "a2", Operator: "bob", Protocol: "vnc", Status: "failed", StartedAt: base})

	// 默认 protocol
	groups := querySessions(t, r, "").Groups
	if len(groups) != 2 {
		t.Fatalf("protocol groups: got %d want 2 (%+v)", len(groups), groups)
	}
	// count 倒序
	if groups[0].Key != "rdp" || groups[0].Count != 2 || groups[0].MsSum != 3000 || groups[0].Failed != 0 {
		t.Errorf("rdp group: %+v", groups[0])
	}
	if groups[1].Key != "vnc" || groups[1].Failed != 1 || groups[1].MsSum != 0 {
		t.Errorf("vnc group: %+v", groups[1])
	}

	// operator
	opGroups := querySessions(t, r, "?groupBy=operator").Groups
	if len(opGroups) != 2 || opGroups[0].Key != "alice" || opGroups[0].Count != 2 {
		t.Errorf("operator groups: %+v", opGroups)
	}

	// agentId（含 agent_id / agent 两种别名）
	for _, q := range []string{"?groupBy=agentId", "?groupBy=agent_id", "?groupBy=AGENT"} {
		ag := querySessions(t, r, q).Groups
		if len(ag) != 2 || ag[0].Key != "a1" || ag[0].Count != 2 {
			t.Errorf("agentId groups (%s): %+v", q, ag)
		}
	}

	// 非法 groupBy 落回 protocol（绝不把用户输入拼进 SQL）
	for _, q := range []string{"?groupBy=bogus", "?groupBy=", "?groupBy=id%3B+DROP+TABLE+remote_session_audits"} {
		if g := querySessions(t, r, q).Groups; len(g) != 2 || g[0].Key != "rdp" {
			t.Errorf("illegal groupBy (%s) must fall back to protocol: %+v", q, g)
		}
	}
}

// ---------- 时间分桶 ----------

func TestSessionBuckets(t *testing.T) {
	r, db := newSessionEnv(t)
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "d1", AgentID: "a", Operator: "u", Protocol: "rdp", Status: "closed", StartedAt: mustTime(t, time.RFC3339, "2026-09-25T10:10:00Z"), DurationMs: 1000})
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "d2", AgentID: "a", Operator: "u", Protocol: "rdp", Status: "closed", StartedAt: mustTime(t, time.RFC3339, "2026-09-25T22:40:00Z"), DurationMs: 3000})
	seedSession(t, db, models.RemoteSessionAudit{SessionID: "d3", AgentID: "a", Operator: "u", Protocol: "rdp", Status: "failed", StartedAt: mustTime(t, time.RFC3339, "2026-09-26T22:00:00Z")})

	days := querySessions(t, r, "").Buckets
	if len(days) != 2 {
		t.Fatalf("day buckets: got %d want 2 (%+v)", len(days), days)
	}
	if days[0].Bucket != "2026-09-25T00:00:00Z" || days[0].Count != 2 || days[0].MsSum != 4000 {
		t.Errorf("day bucket 0: %+v", days[0])
	}
	if days[1].Bucket != "2026-09-26T00:00:00Z" || days[1].Count != 1 || days[1].Failed != 1 {
		t.Errorf("day bucket 1: %+v", days[1])
	}

	// hour 粒度：09/25 10 点与 22 点、09/26 22 点 → 3 桶
	hours := querySessions(t, r, "?bucket=hour").Buckets
	if len(hours) != 3 {
		t.Fatalf("hour buckets: got %d want 3 (%+v)", len(hours), hours)
	}
	if hours[0].Bucket != "2026-09-25T10:00:00Z" || hours[0].Count != 1 || hours[0].MsSum != 1000 {
		t.Errorf("hour bucket 0: %+v", hours[0])
	}
	// month 粒度：三条都在 2026-09 → 单桶
	months := querySessions(t, r, "?bucket=month").Buckets
	if len(months) != 1 {
		t.Fatalf("month buckets: got %d want 1 (%+v)", len(months), months)
	}
	if months[0].Bucket != "2026-09-01T00:00:00Z" || months[0].Count != 3 || months[0].Failed != 1 {
		t.Errorf("month bucket: %+v", months[0])
	}
	// week 粒度：同周内合成（%Y-W%W 键）
	weeks := querySessions(t, r, "?bucket=week").Buckets
	if len(weeks) != 1 || weeks[0].Count != 3 || weeks[0].Failed != 1 {
		t.Errorf("week buckets: %+v", weeks)
	}
	// 非法粒度落 day
	if g := querySessions(t, r, "?bucket=fortnight").Buckets; len(g) != 2 {
		t.Errorf("illegal bucket must fall back to day: %+v", g)
	}
}

func TestRemoteSessionBucketExprWidths(t *testing.T) {
	// 粒度白名单函数独立断言（报表表达式拼进 SQL，必须锁死取值域）
	for _, tc := range []struct{ raw, want string }{
		{"hour", "hour"}, {"HOUR", "hour"}, {" day ", "day"},
		{"week", "week"}, {"month", "month"}, {"", "day"}, {"nope", "day"},
	} {
		if got := remoteSessionBucketWidth(tc.raw); got != tc.want {
			t.Errorf("remoteSessionBucketWidth(%q) = %q want %q", tc.raw, got, tc.want)
		}
	}
	// 各粒度表达式互不相同（否则分桶退化为单桶）
	exprs := map[string]bool{}
	for _, w := range []string{"hour", "day", "week", "month"} {
		e := remoteSessionBucketExpr(w)
		if e == "" {
			t.Fatalf("empty expr for %q", w)
		}
		if exprs[e] {
			t.Errorf("duplicate expr %q for width %q", e, w)
		}
		exprs[e] = true
	}
}

// ---------- 分页 ----------

func TestSessionListPaging(t *testing.T) {
	r, db := newSessionEnv(t)
	base := mustTime(t, time.RFC3339, "2026-09-25T10:00:00Z")
	for i := 0; i < 5; i++ {
		seedSession(t, db, models.RemoteSessionAudit{
			SessionID: fmt.Sprintf("p%d", i), AgentID: "a", Operator: "u", Protocol: "rdp",
			Status: "closed", StartedAt: base.Add(time.Duration(i) * time.Minute), DurationMs: 1000,
		})
	}

	page1 := querySessions(t, r, "?page=1&size=2")
	if len(page1.Data) != 2 || page1.Total != 5 || page1.Page != 1 || page1.Size != 2 {
		t.Fatalf("page1: %+v", page1)
	}
	// 汇总/分组基于全量而非当页
	if page1.Summary.Total != 5 {
		t.Errorf("summary must cover all rows, got %d", page1.Summary.Total)
	}
	page3 := querySessions(t, r, "?page=3&size=2")
	if len(page3.Data) != 1 {
		t.Fatalf("page3 should have 1 row, got %d", len(page3.Data))
	}
	// 越界页返回空数组而非报错
	if out := querySessions(t, r, "?page=99&size=2"); len(out.Data) != 0 || out.Total != 5 {
		t.Errorf("out-of-range page: %+v", out)
	}
	// size 上限 200
	if got := querySessions(t, r, "?size=9999").Size; got != remoteSessionPageMax {
		t.Errorf("size clamp: got %d want %d", got, remoteSessionPageMax)
	}
	// 非法分页参数回退默认
	bad := querySessions(t, r, "?page=-1&size=abc")
	if bad.Page != 1 || bad.Size != 20 {
		t.Errorf("illegal paging should fall back: page=%d size=%d", bad.Page, bad.Size)
	}
}

// ---------- 失败路径（500） ----------

// failNthQuery 注册 GORM 回调：第 n 次查询时注入错误，返回已见次数。
//
// 用途：让 HandleList 里 count/find/汇总/分组/分桶 五条失败分支**逐条**可达。
// 删表只能让第一条（count）失败——后面四条永远走不到，覆盖率上就是死代码。
//
// 两条回调链都要挂：实测 gorm 里 Count/Find 走 query 链，而三条 Scan 型聚合
// 走 row 链（本文件底部的链路探针用例锁定该事实）。HandleList 的查询顺序固定
// （count → find → 汇总 → 分组 → 分桶），故按序号定位即可精确定位每一阶段。
func failNthQuery(t *testing.T, db *gorm.DB, name string, n int) *int {
	t.Helper()
	seen := 0
	inject := func(tx *gorm.DB) {
		seen++
		if seen == n {
			tx.AddError(errors.New("injected query failure"))
		}
	}
	if err := db.Callback().Query().After("gorm:query").Register(name, inject); err != nil {
		t.Fatalf("register query callback %s: %v", name, err)
	}
	if err := db.Callback().Row().After("gorm:row").Register(name, inject); err != nil {
		t.Fatalf("register row callback %s: %v", name, err)
	}
	t.Cleanup(func() {
		db.Callback().Query().Remove(name)
		db.Callback().Row().Remove(name)
	})
	return &seen
}

// seedOneSession 造一行数据（失败注入用；表还在、行能读，只让某条查询报错）。
func seedOneSession(t *testing.T, db *gorm.DB) {
	t.Helper()
	seedSession(t, db, models.RemoteSessionAudit{
		SessionID: "only", AgentID: "a", Operator: "u", Protocol: "rdp",
		Status: "closed", StartedAt: mustTime(t, time.RFC3339, "2026-09-25T10:00:00Z"), DurationMs: 1000,
	})
}

func TestSessionListReturns500WhenTableMissing(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	// 删表 → count 即失败
	if err := db.Migrator().DropTable(&models.RemoteSessionAudit{}); err != nil {
		t.Fatalf("drop table: %v", err)
	}
	h := NewRemoteSessionHandler(db)
	r := gin.New()
	r.GET("/api/remote/sessions", h.HandleList)

	w := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodGet, "/api/remote/sessions", nil)
	r.ServeHTTP(w, req)
	if w.Code != http.StatusInternalServerError {
		t.Fatalf("got %d want 500 (%s)", w.Code, w.Body.String())
	}
	var resp sessionListResp
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatalf("body must be valid json: %s", w.Body.String())
	}
	if resp.Success || resp.Error == "" {
		t.Errorf("error response shape wrong: %+v", resp)
	}
}

func TestSessionListReturns500OnEachQueryStage(t *testing.T) {
	// HandleList 固定 5 条查询：1=count 2=find 3=汇总 4=分组 5=分桶。
	// 逐条注入 → 五条 500 分支全部可达。
	const stageCount = 5
	for n := 1; n <= stageCount; n++ {
		t.Run(fmt.Sprintf("query stage %d", n), func(t *testing.T) {
			gin.SetMode(gin.TestMode)
			db := newDB(t)
			seedOneSession(t, db)
			seen := failNthQuery(t, db, fmt.Sprintf("fail_n%d", n), n)

			h := NewRemoteSessionHandler(db)
			r := gin.New()
			r.GET("/api/remote/sessions", asAdmin(1), h.HandleList)

			w := httptest.NewRecorder()
			req := httptest.NewRequest(http.MethodGet, "/api/remote/sessions", nil)
			r.ServeHTTP(w, req)
			if w.Code != http.StatusInternalServerError {
				t.Fatalf("stage %d: got %d want 500 (%s)", n, w.Code, w.Body.String())
			}
			if !json.Valid(w.Body.Bytes()) {
				t.Errorf("body must be valid json: %s", w.Body.String())
			}
			if n == stageCount && *seen != stageCount {
				t.Errorf("expected %d queries to run, saw %d — query order changed?", stageCount, *seen)
			}
		})
	}
}

// TestSessionQueryCallbackChains 锁定 gorm 回调链事实：Count/Find 走 query
// 链，三条 Scan 型聚合走 row 链。
//
// 这条不是测产品行为，是给 failNthQuery 的注入点兜底——若 gorm 升级后换了
// 链，上面那条逐阶段注入会静默变成「只覆盖前两阶段」，失败分支重新变回死
// 代码，而测试仍会全绿。锁死链路形状才能让偏差立刻暴露。
func TestSessionQueryCallbackChains(t *testing.T) {
	db := newDB(t)
	seedOneSession(t, db)
	var order []string
	// 两条链必须用**不同**回调名注册，否则同名回调在每次查询都被两条链各跑
	// 一遍，探针就分不出是哪条链
	if err := db.Callback().Query().After("gorm:query").Register("probe_q", func(tx *gorm.DB) {
		order = append(order, "query")
	}); err != nil {
		t.Fatalf("register query probe: %v", err)
	}
	if err := db.Callback().Row().After("gorm:row").Register("probe_r", func(tx *gorm.DB) {
		order = append(order, "row")
	}); err != nil {
		t.Fatalf("register row probe: %v", err)
	}
	t.Cleanup(func() {
		db.Callback().Query().Remove("probe_q")
		db.Callback().Row().Remove("probe_r")
	})

	var n int64
	db.Model(&models.RemoteSessionAudit{}).Count(&n)
	var rows []models.RemoteSessionAudit
	db.Model(&models.RemoteSessionAudit{}).Find(&rows)
	var agg struct{ Total int64 }
	db.Model(&models.RemoteSessionAudit{}).Select("COUNT(*) AS total").Scan(&agg)
	var groups []remoteSessionGroup
	db.Model(&models.RemoteSessionAudit{}).Select("protocol AS key, COUNT(*) AS count").Group("protocol").Scan(&groups)
	var buckets []remoteSessionBucket
	db.Model(&models.RemoteSessionAudit{}).
		Select("strftime('%Y-%m-%d', started_at) AS bucket, COUNT(*) AS count").Group("bucket").Scan(&buckets)

	want := []string{"query", "query", "row", "row", "row"}
	if fmt.Sprint(order) != fmt.Sprint(want) {
		t.Errorf("gorm callback chains changed: got %v want %v — failNthQuery 的注入点需同步", order, want)
	}
}

// ---------- 路由装配（nil 依赖不注册） ----------

func TestRemoteSessionsRouteRegistration(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	// 装了 handler → 路由存在
	r := gin.New()
	RegisterRoutes(r, RouterDeps{DB: db, RemoteSessions: NewRemoteSessionHandler(db)})
	found := false
	for _, route := range r.Routes() {
		if route.Path == "/api/remote/sessions" && route.Method == http.MethodGet {
			found = true
		}
	}
	if !found {
		t.Error("GET /api/remote/sessions should be registered when RemoteSessions is set")
	}

	// nil → 不注册（与 Guacamole/Recordings 同款可选装配）
	r2 := gin.New()
	RegisterRoutes(r2, RouterDeps{DB: db})
	for _, route := range r2.Routes() {
		if route.Path == "/api/remote/sessions" {
			t.Error("GET /api/remote/sessions must not be registered when RemoteSessions is nil")
		}
	}
}
