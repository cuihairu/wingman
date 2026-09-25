package handlers

// 远程桌面会话审计报表（设计 §8 安全模型「审计」条 + §11 P1「审计报表呈现」）。
//
// 与 AuditLog 的分工（两者受众不同，缺一不可）：
//   - AuditLog：append-only 事件流水，一条事件一行，meta 是 JSON。回答
//     「某人在某时刻做了什么」，给合规逐条查（已有 /api/audit）。
//   - 本报表：结构化可聚合列。回答「这台机器这个月被谁接管了多久、录了
//     几段、失败几次」，给运维看趋势。聚合在 JSON meta 上做既慢又脆，故
//     独立专表（models.RemoteSessionAudit）。
//
// 落库时机：会话**结束时**一次性写入终态行（closed / failed）。进行中的
// 会话不落行——否则报表会把「还没结束的会话」算进时长与计数，且崩溃后
// 留下永远不闭合的脏行。代价是「当前有多少人在看」无法从本报表得到
// （那是实时态，属 registry 的职责，不属审计）。
//
// 权限：查询走 desktop:view（与录像检索同级——监看者能看报表，接管者不多
// 给权限，因为报表本身不含任何接管能力）。不新增 RBAC 码。

import (
	"log"
	"net/http"
	"strings"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// remoteSessionPageMax 单页上限（与 audit 端点的 200 对齐）。
const remoteSessionPageMax = 200

// RemoteSessionHandler 会话审计报表查询。
type RemoteSessionHandler struct {
	db *gorm.DB
}

// NewRemoteSessionHandler 构造。
func NewRemoteSessionHandler(db *gorm.DB) *RemoteSessionHandler {
	return &RemoteSessionHandler{db: db}
}

// remoteSessionResponse 列表行（列名对齐前端 camelCase 惯例）。
type remoteSessionResponse struct {
	ID            uint   `json:"id"`
	SessionID     string `json:"sessionId"`
	AgentID       string `json:"agentId"`
	Operator      string `json:"operator"`
	Protocol      string `json:"protocol"`
	Host          string `json:"host"`
	Port          int    `json:"port"`
	ReadOnly      bool   `json:"readOnly"`
	Record        bool   `json:"record"`
	RecordingName string `json:"recordingName,omitempty"`
	Status        string `json:"status"`
	FailReason    string `json:"failReason,omitempty"`
	StartedAt     string `json:"startedAt"`
	EndedAt       string `json:"endedAt,omitempty"`
	DurationMs    int64  `json:"durationMs"`
}

// remoteSessionSummary 单行汇总（报表首屏概览）。
type remoteSessionSummary struct {
	Total      int64 `json:"total"`
	Closed     int64 `json:"closed"`
	Failed     int64 `json:"failed"`
	Recorded   int64 `json:"recorded"`
	Control    int64 `json:"control"`    // 接管（readOnly=false）次数
	ViewOnly   int64 `json:"viewOnly"`   // 监看次数
	TotalMsSum int64 `json:"totalMsSum"` // 时长求和（仅 closed 计入）
}

// remoteSessionBucket 时间桶聚合（趋势图数据源）。
type remoteSessionBucket struct {
	// 桶起点（RFC3339 UTC）
	Bucket string `json:"bucket"`
	Count  int64  `json:"count"`
	Failed int64  `json:"failed"`
	MsSum  int64  `json:"msSum"`
}

// remoteSessionGroup 维度聚合（按协议/操作者/机器看分布）。
type remoteSessionGroup struct {
	Key    string `json:"key"`
	Count  int64  `json:"count"`
	MsSum  int64  `json:"msSum"`
	Failed int64  `json:"failed"`
}

// HandleList GET /api/remote/sessions：分页 + 多维过滤的会话审计列表。
// @Summary      远程桌面会话审计列表
// @Description  分页查询已结束的 Guacamole 远程桌面会话审计（按开始时间倒序），支持按 agent/协议/操作者/模式/录制/状态/时间范围过滤；返回带 total 与分组/汇总的报表数据
// @Tags         remote
// @Produce      json
// @Security     BearerAuth
// @Param        page      query  int     false  "页码"                    default(1)
// @Param        size      query  int     false  "每页条数（最大 200）"      default(20)
// @Param        agentId   query  string  false  "按目标 agent 过滤"
// @Param        protocol  query  string  false  "按协议过滤（rdp/vnc/ssh）"
// @Param        operator  query  string  false  "按操作者过滤"
// @Param        status    query  string  false  "按状态过滤（closed/failed）"
// @Param        mode      query  string  false  "按模式过滤：view=监看 / control=接管"
// @Param        record    query  bool    false  "只看有录像的会话"
// @Param        start     query  string  false  "起始时间（RFC3339）"
// @Param        end       query  string  false  "结束时间（RFC3339）"
// @Param        groupBy   query  string  false  "分组维度：protocol/operator/agentId（默认 protocol）"
// @Success      200  {object}  map[string]interface{}
// @Failure      401  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse
// @Router       /remote/sessions [get]
func (h *RemoteSessionHandler) HandleList(c *gin.Context) {
	page := parsePositiveInt(c.Query("page"), 1)
	size := parsePositiveInt(c.Query("size"), 20)
	if size > remoteSessionPageMax {
		size = remoteSessionPageMax
	}

	query := h.db.Model(&models.RemoteSessionAudit{}).Order("started_at desc, id desc")
	applyRemoteSessionFilters(query, c)

	var total int64
	if err := query.Count(&total).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to count remote sessions"})
		return
	}

	var rows []models.RemoteSessionAudit
	if err := query.Offset((page - 1) * size).Limit(size).Find(&rows).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to load remote sessions"})
		return
	}

	items := make([]remoteSessionResponse, 0, len(rows))
	for _, row := range rows {
		item := remoteSessionResponse{
			ID:            row.ID,
			SessionID:     row.SessionID,
			AgentID:       row.AgentID,
			Operator:      row.Operator,
			Protocol:      row.Protocol,
			Host:          row.Host,
			Port:          row.Port,
			ReadOnly:      row.ReadOnly,
			Record:        row.Record,
			RecordingName: row.RecordingName,
			Status:        row.Status,
			FailReason:    row.FailReason,
			StartedAt:     row.StartedAt.UTC().Format(time.RFC3339),
			DurationMs:    row.DurationMs,
		}
		if row.EndedAt != nil {
			item.EndedAt = row.EndedAt.UTC().Format(time.RFC3339)
		}
		items = append(items, item)
	}

	// 汇总与分组基于**全量过滤结果**（不受分页影响）——报表要的是区间
	// 总数，不是「这一页的总数」。
	summaryQuery := h.db.Model(&models.RemoteSessionAudit{})
	applyRemoteSessionFilters(summaryQuery, c)
	summary := remoteSessionSummary{}
	if err := remoteSessionAggregate(summaryQuery, &summary); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to summarize remote sessions"})
		return
	}

	groupQuery := h.db.Model(&models.RemoteSessionAudit{})
	applyRemoteSessionFilters(groupQuery, c)
	groups, err := remoteSessionGrouped(groupQuery, remoteSessionGroupColumn(c.Query("groupBy")))
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to group remote sessions"})
		return
	}

	bucketQuery := h.db.Model(&models.RemoteSessionAudit{})
	applyRemoteSessionFilters(bucketQuery, c)
	buckets, err := remoteSessionBuckets(bucketQuery, remoteSessionBucketWidth(c.Query("bucket")))
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to bucket remote sessions"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    items,
		"total":   total,
		"page":    page,
		"size":    size,
		"summary": summary,
		"groups":  groups,
		"buckets": buckets,
	})
}

// applyRemoteSessionFilters 把查询参数翻译成 where 条件（列表/汇总/分组/
// 分桶四条查询共用，保证「报表各处口径一致」——口径分叉是报表最常见的
// 错误来源）。
func applyRemoteSessionFilters(query *gorm.DB, c *gin.Context) {
	if v := strings.TrimSpace(c.Query("agentId")); v != "" {
		query = query.Where("agent_id = ?", v)
	}
	if v := strings.ToLower(strings.TrimSpace(c.Query("protocol"))); v != "" {
		query = query.Where("protocol = ?", v)
	}
	if v := strings.TrimSpace(c.Query("operator")); v != "" {
		query = query.Where("operator = ?", v)
	}
	if v := strings.ToLower(strings.TrimSpace(c.Query("status"))); v != "" {
		query = query.Where("status = ?", v)
	}
	// mode=view/control 是 UI 语义（监看/接管），落到 read_only 布尔列；
	// 非法值忽略而非报错——报表参数不该把整页查询打挂。
	switch strings.ToLower(strings.TrimSpace(c.Query("mode"))) {
	case "view":
		query = query.Where("read_only = ?", true)
	case "control":
		query = query.Where("read_only = ?", false)
	}
	if v := strings.ToLower(strings.TrimSpace(c.Query("record"))); v == "true" || v == "1" {
		query = query.Where("record = ?", true)
	}
	if start := parseRFC3339(c.Query("start")); !start.IsZero() {
		query = query.Where("started_at >= ?", start)
	}
	if end := parseRFC3339(c.Query("end")); !end.IsZero() {
		query = query.Where("started_at <= ?", end)
	}
}

// remoteSessionGroupColumn 分组维度白名单（列名来自用户输入，必须白名单化
// 才能安全拼进 SQL；非法值落 protocol）。
func remoteSessionGroupColumn(raw string) string {
	switch strings.ToLower(strings.TrimSpace(raw)) {
	case "operator":
		return "operator"
	case "agentid", "agent_id", "agent":
		return "agent_id"
	default:
		return "protocol"
	}
}

// remoteSessionBucketWidth 时间桶粒度：hour/day/week/month。非法值落 day。
// 用 strftime 在 SQLite 里按 UTC 截断（started_at 统一存 UTC）。
func remoteSessionBucketWidth(raw string) string {
	switch strings.ToLower(strings.TrimSpace(raw)) {
	case "hour":
		return "hour"
	case "week":
		return "week"
	case "month":
		return "month"
	default:
		return "day"
	}
}

// remoteSessionBucketExpr 各粒度对应的 strftime 格式。
func remoteSessionBucketExpr(width string) string {
	switch width {
	case "hour":
		return "%Y-%m-%dT%H:00:00Z"
	case "week":
		// SQLite 的 %W 是「周一为首日」的周序号；strftime 本身不提供 ISO 周
		// 起点，故用「年+周号」拼一个稳定可比的键（仅用于分组，不用于展示）。
		return "%Y-W%W"
	case "month":
		return "%Y-%m-01T00:00:00Z"
	default:
		return "%Y-%m-%dT00:00:00Z"
	}
}

// remoteSessionAggregate 汇总（单表扫描 + 条件计数，避免 N 次查询）。
func remoteSessionAggregate(query *gorm.DB, out *remoteSessionSummary) error {
	type aggRow struct {
		Total      int64
		Closed     int64
		Failed     int64
		Recorded   int64
		Control    int64
		ViewOnly   int64
		TotalMsSum int64
	}
	var row aggRow
	err := query.Session(&gorm.Session{}).
		Select(`COUNT(*) AS total,
			COALESCE(SUM(CASE WHEN status = 'closed' THEN 1 ELSE 0 END), 0) AS closed,
			COALESCE(SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END), 0) AS failed,
			COALESCE(SUM(CASE WHEN record = 1 THEN 1 ELSE 0 END), 0) AS recorded,
			COALESCE(SUM(CASE WHEN read_only = 0 THEN 1 ELSE 0 END), 0) AS control,
			COALESCE(SUM(CASE WHEN read_only = 1 THEN 1 ELSE 0 END), 0) AS view_only,
			COALESCE(SUM(CASE WHEN status = 'closed' THEN duration_ms ELSE 0 END), 0) AS total_ms_sum`).
		Scan(&row).Error
	if err != nil {
		return err
	}
	*out = remoteSessionSummary{
		Total:      row.Total,
		Closed:     row.Closed,
		Failed:     row.Failed,
		Recorded:   row.Recorded,
		Control:    row.Control,
		ViewOnly:   row.ViewOnly,
		TotalMsSum: row.TotalMsSum,
	}
	return nil
}

// remoteSessionGrouped 维度聚合：count / 时长求和 / 失败数，按 count 倒序。
func remoteSessionGrouped(query *gorm.DB, column string) ([]remoteSessionGroup, error) {
	var rows []remoteSessionGroup
	err := query.Session(&gorm.Session{}).
		Select(column + ` AS key,
			COUNT(*) AS count,
			COALESCE(SUM(CASE WHEN status = 'closed' THEN duration_ms ELSE 0 END), 0) AS ms_sum,
			COALESCE(SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END), 0) AS failed`).
		Group(column).
		Order("count DESC, key ASC").
		Scan(&rows).Error
	if err != nil {
		return nil, err
	}
	if rows == nil {
		rows = []remoteSessionGroup{}
	}
	return rows, nil
}

// remoteSessionBuckets 时间趋势分桶。
func remoteSessionBuckets(query *gorm.DB, width string) ([]remoteSessionBucket, error) {
	expr := remoteSessionBucketExpr(width)
	var rows []remoteSessionBucket
	err := query.Session(&gorm.Session{}).
		Select(`strftime('` + expr + `', started_at) AS bucket,
			COUNT(*) AS count,
			COALESCE(SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END), 0) AS failed,
			COALESCE(SUM(CASE WHEN status = 'closed' THEN duration_ms ELSE 0 END), 0) AS ms_sum`).
		Group("bucket").
		Order("bucket ASC").
		Scan(&rows).Error
	if err != nil {
		return nil, err
	}
	if rows == nil {
		rows = []remoteSessionBucket{}
	}
	return rows, nil
}

// RecordRemoteSession 落一条会话审计终态行（网关在会话结束时调用）。
//
// 单点入口而非让调用方直接 db.Create：字段填充规则（UTC 归一、时长
// 口径、终态枚举）是审计契约的一部分，散在调用点迟早分叉。
// 落库失败只记日志不阻断会话关闭——审计写不进去不该让用户的会话卡住。
func RecordRemoteSession(db *gorm.DB, rec models.RemoteSessionAudit) {
	if db == nil {
		return
	}
	if rec.StartedAt.IsZero() {
		rec.StartedAt = time.Now().UTC()
	} else {
		rec.StartedAt = rec.StartedAt.UTC()
	}
	if rec.EndedAt != nil {
		ended := rec.EndedAt.UTC()
		rec.EndedAt = &ended
	}
	// 时长以结束时刻为准重算：调用方传的 durationMs 允许为 0（failed 场景
	// 本就没有像素面时长），这里保证 closed 行一定有可聚合的时长。
	if rec.EndedAt != nil && rec.DurationMs <= 0 {
		rec.DurationMs = rec.EndedAt.Sub(rec.StartedAt).Milliseconds()
	}
	if rec.Status == "" {
		rec.Status = models.RemoteSessionStatusClosed
	}
	if err := db.Create(&rec).Error; err != nil {
		// 不吞：审计缺失必须可发现。日志与既有 WriteAuditLog 的 best-effort
		// 语义一致（不阻断主流程），但显式留痕。
		log.Printf("RemoteSession: audit write failed (session=%s): %v", rec.SessionID, err)
	}
}
