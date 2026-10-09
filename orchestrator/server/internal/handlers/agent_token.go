package handlers

import (
	"net/http"
	"strconv"
	"strings"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// AgentTokenHandler per-agent 注册 token 管理面（A3-P2，
// docs/agent-token-auth-design.md §6.2）：签发/吊销/列表 + 审计落库。
// 校验点在 agent.FrameListener（双源并存，env 白名单兜底），本 handler
// 只管 token 生命周期。
type AgentTokenHandler struct {
	store *agent.TokenStore
	db    *gorm.DB
}

func NewAgentTokenHandler(store *agent.TokenStore, db *gorm.DB) *AgentTokenHandler {
	return &AgentTokenHandler{store: store, db: db}
}

// HandleList 注册 token 列表
// @Summary      注册 token 列表
// @Description  返回全部 agent 注册 token（只含前缀，不含哈希与明文）
// @Tags         agent-tokens
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Failure      401  {object}  ErrorResponse
// @Router       /agent-tokens [get]
func (h *AgentTokenHandler) HandleList(c *gin.Context) {
	list, err := h.store.List()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"error":   "failed to list tokens: " + err.Error(),
		})
		return
	}
	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    list,
	})
}

// HandleCreate 签发注册 token
// @Summary      签发注册 token
// @Description  生成新 token；明文只在本次响应返回一次，库内仅存哈希。agentId 非空时与该 agent 绑定
// @Tags         agent-tokens
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        body  body  object  true  "{label, agentId?}"
// @Success      200   {object}  map[string]interface{}
// @Failure      400   {object}  ErrorResponse
// @Router       /agent-tokens [post]
func (h *AgentTokenHandler) HandleCreate(c *gin.Context) {
	var req struct {
		Label   string `json:"label"`
		AgentID string `json:"agentId"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"success": false,
			"error":   "invalid request body: " + err.Error(),
		})
		return
	}
	req.Label = strings.TrimSpace(req.Label)
	if req.Label == "" {
		c.JSON(http.StatusBadRequest, gin.H{
			"success": false,
			"error":   "label is required",
		})
		return
	}

	_, actor, _ := middleware.GetCurrentUser(c)
	plain, rec, err := h.store.Create(req.Label, req.AgentID, actor)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"error":   "failed to create token: " + err.Error(),
		})
		return
	}

	WriteAuditLog(h.db, actor, "agenttoken.create", rec.Prefix, map[string]any{
		"token_id": rec.ID,
		"label":    rec.Label,
		"agent_id": rec.AgentID,
	})

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		// 明文只出现这一次；关掉页面即不可再取（轮换=重新签发）
		"data": gin.H{
			"token":  plain,
			"record": rec,
		},
	})
}

// HandleRevoke 吊销注册 token
// @Summary      吊销注册 token
// @Description  吊销后该 token 立即失效（已吊销返回 success:true 幂等）
// @Tags         agent-tokens
// @Produce      json
// @Security     BearerAuth
// @Param        id  path  int  true  "Token ID"
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  ErrorResponse
// @Router       /agent-tokens/{id} [delete]
func (h *AgentTokenHandler) HandleRevoke(c *gin.Context) {
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil || id == 0 {
		c.JSON(http.StatusBadRequest, gin.H{
			"success": false,
			"error":   "invalid token id",
		})
		return
	}

	revoked, err := h.store.Revoke(uint(id))
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"error":   "failed to revoke token: " + err.Error(),
		})
		return
	}

	if revoked {
		_, actor, _ := middleware.GetCurrentUser(c)
		WriteAuditLog(h.db, actor, "agenttoken.revoke", strconv.FormatUint(id, 10), map[string]any{
			"token_id": id,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data": gin.H{
			"revoked": revoked,
		},
	})
}
