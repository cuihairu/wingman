package handlers

import (
	"crypto/rand"
	"encoding/hex"
	"errors"
	"fmt"
	"net/http"
	"strconv"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// ExecutionHandler 执行记录查询（ADR: Execution as the Platform Core Object）。
// v1 为只读收敛：列表 + 详情。执行记录由 run_script/batch/workflow 等执行路径
// 产生（当前已接线 run_script），本 handler 不改动执行语义。
type ExecutionHandler struct {
	db *gorm.DB
}

// NewExecutionHandler 构造执行记录查询 handler。
func NewExecutionHandler(db *gorm.DB) *ExecutionHandler {
	return &ExecutionHandler{db: db}
}

// newExecutionID 生成全局唯一执行 ID（16 字节随机数 hex，碰撞概率可忽略）。
// 执行记录以 executionId 为唯一索引，是日志/审计/Artifacts 的挂载点。
func newExecutionID() string {
	buf := make([]byte, 16)
	if _, err := rand.Read(buf); err != nil {
		return fmt.Sprintf("exec_%d", time.Now().UnixNano())
	}
	return hex.EncodeToString(buf)
}

// HandleList GET /api/executions?page=&pageSize=&status=&agentId=
// 按创建时间倒序返回分页列表（ExecutionView 展开 Result/Artifacts JSON）。
// @Summary      执行记录列表
// @Description  按时间倒序分页返回执行记录，支持 status/agentId 过滤；登录即可访问
// @Tags         executions
// @Produce      json
// @Security     BearerAuth
// @Param        page       query int    false "页码（默认 1）"
// @Param        pageSize   query int    false "每页数量（默认 20，上限 100）"
// @Param        status     query string false "按状态过滤（queued/running/succeeded/failed/cancelled/timeout/lost）"
// @Param        agentId    query string false "按执行 agent 过滤"
// @Success      200  {object}  map[string]interface{}
// @Failure      401  {object}  ErrorResponse
// @Router       /executions [get]
func (h *ExecutionHandler) HandleList(c *gin.Context) {
	page, _ := strconv.Atoi(c.DefaultQuery("page", "1"))
	pageSize, _ := strconv.Atoi(c.DefaultQuery("pageSize", "20"))
	if page < 1 {
		page = 1
	}
	if pageSize < 1 || pageSize > 100 {
		pageSize = 20
	}

	q := h.db.Model(&models.Execution{})
	if status := c.Query("status"); status != "" {
		q = q.Where("status = ?", status)
	}
	if agentID := c.Query("agentId"); agentID != "" {
		q = q.Where("agent_id = ?", agentID)
	}

	var total int64
	q.Count(&total)

	var records []models.Execution
	q.Order("id DESC").Offset((page - 1) * pageSize).Limit(pageSize).Find(&records)

	items := make([]models.ExecutionView, 0, len(records))
	for _, r := range records {
		items = append(items, r.ToView())
	}

	c.JSON(http.StatusOK, gin.H{
		"success":  true,
		"data":     items,
		"total":    total,
		"page":     page,
		"pageSize": pageSize,
	})
}

// HandleGet GET /api/executions/:id —— 单条执行记录详情。
// @Summary      执行记录详情
// @Description  按数据库自增 id 返回单条执行记录（ExecutionView）；登录即可访问
// @Tags         executions
// @Produce      json
// @Security     BearerAuth
// @Param        id  path  uint  true  "执行记录自增 id"
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  ErrorResponse "id 非法"
// @Failure      401  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse "记录不存在"
// @Failure      500  {object}  ErrorResponse
// @Router       /executions/{id} [get]
func (h *ExecutionHandler) HandleGet(c *gin.Context) {
	id, err := strconv.Atoi(c.Param("id"))
	if err != nil || id <= 0 {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid execution id"})
		return
	}

	var rec models.Execution
	if err := h.db.First(&rec, id).Error; err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "execution not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "query execution: " + err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    rec.ToView(),
	})
}
