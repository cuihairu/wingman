package handlers

import (
	"net/http"
	"strings"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

type FeedbackHandler struct {
	db *gorm.DB
}

func NewFeedbackHandler(db *gorm.DB) *FeedbackHandler {
	return &FeedbackHandler{db: db}
}

type createFeedbackReq struct {
	Category string `json:"category"`
	Content  string `json:"content" binding:"required"`
	Priority string `json:"priority"`
	Source   string `json:"source"`
}

func (h *FeedbackHandler) HandleCreate(c *gin.Context) {
	_, username, _ := middleware.GetCurrentUser(c)
	var req createFeedbackReq
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "content is required"})
		return
	}

	category := strings.TrimSpace(req.Category)
	if category == "" {
		category = "general"
	}
	priority := strings.TrimSpace(req.Priority)
	if priority == "" {
		priority = "normal"
	}
	content := strings.TrimSpace(req.Content)
	if content == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "content is required"})
		return
	}

	feedback := models.Feedback{
		Actor:    username,
		Category: category,
		Content:  content,
		Priority: priority,
		Source:   strings.TrimSpace(req.Source),
		Status:   "open",
	}
	if err := h.db.Create(&feedback).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to create feedback"})
		return
	}

	title := "反馈已提交"
	if category == "permission_request" {
		title = "权限申请已提交"
	}
	h.db.Create(&models.Message{
		Recipient: username,
		Title:     title,
		Content:   "我们已收到你的提交，管理员可在审计与反馈记录中跟进。",
		Status:    "unread",
		Category:  category,
		Source:    "feedback",
	})
	h.db.Create(&models.Message{
		Recipient: "*",
		Title:     "新的用户反馈",
		Content:   username + " 提交了 " + category + " 反馈。",
		Status:    "unread",
		Category:  category,
		Source:    "feedback",
	})

	WriteAuditLog(h.db, username, "feedback.create", "feedback", map[string]any{
		"id":       feedback.ID,
		"category": category,
		"priority": priority,
		"source":   feedback.Source,
		"ip":       c.ClientIP(),
	})

	c.JSON(http.StatusCreated, gin.H{"success": true, "id": feedback.ID})
}
