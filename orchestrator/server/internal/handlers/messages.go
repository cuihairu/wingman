package handlers

import (
	"net/http"
	"strings"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

type MessageHandler struct {
	db *gorm.DB
}

func NewMessageHandler(db *gorm.DB) *MessageHandler {
	return &MessageHandler{db: db}
}

type messageDTO struct {
	ID        uint   `json:"id"`
	Title     string `json:"title"`
	Content   string `json:"content"`
	Status    string `json:"status"`
	Category  string `json:"category"`
	Source    string `json:"source"`
	CreatedAt string `json:"createdAt"`
}

func toMessageDTO(message models.Message, read bool) messageDTO {
	createdAt := ""
	if !message.CreatedAt.IsZero() {
		createdAt = message.CreatedAt.Format("2006-01-02 15:04:05")
	}
	status := "unread"
	if read || strings.EqualFold(strings.TrimSpace(message.Status), "read") {
		status = "read"
	}
	return messageDTO{
		ID:        message.ID,
		Title:     message.Title,
		Content:   message.Content,
		Status:    status,
		Category:  message.Category,
		Source:    message.Source,
		CreatedAt: createdAt,
	}
}

// readExistsSubquery 返回针对当前用户在 message_reads 表的 EXISTS 子查询。
// 调用方将其嵌入 Where 子句以判定 per-user 已读状态。
func (h *MessageHandler) readExistsSubquery(userID uint) *gorm.DB {
	return h.db.Model(&models.MessageRead{}).
		Select("1").
		Where("message_id = messages.id AND user_id = ?", userID)
}

// visibleQuery 返回当前用户可见的消息基础查询（直发本人 + 广播；admin 看全部）。
func (h *MessageHandler) visibleQuery(userID uint, username, role string) *gorm.DB {
	query := h.db.Model(&models.Message{})
	if strings.EqualFold(role, "admin") {
		return query
	}
	return query.Where("recipient = ? OR recipient = ?", username, "*")
}

// @Summary      列出站内消息（per-user 已读状态）
// @Tags         messages
// @Produce      json
// @Security     BearerAuth
// @Param        page       query  int     false  "页码"                  default(1)
// @Param        pageSize   query  int     false  "每页条数（最大 100）"   default(20)
// @Param        status     query  string  false  "read|unread|all"
// @Success      200  {object}  map[string]interface{}
// @Router       /messages [get]
func (h *MessageHandler) HandleList(c *gin.Context) {
	userID, username, role := middleware.GetCurrentUser(c)
	page := parsePositiveInt(c.Query("page"), 1)
	size := parsePositiveInt(firstNonEmptyMessageParam(c.Query("pageSize"), c.Query("size")), 20)
	if size > 100 {
		size = 100
	}

	query := h.visibleQuery(userID, username, role).Order("created_at desc")

	// 按 per-user 已读状态过滤（覆盖广播消息共享单行的场景）。
	if status := strings.TrimSpace(c.Query("status")); status != "" && !strings.EqualFold(status, "all") {
		readExists := h.readExistsSubquery(userID)
		if strings.EqualFold(status, "read") {
			query = query.Where("EXISTS (?)", readExists)
		} else if strings.EqualFold(status, "unread") {
			query = query.Where("NOT EXISTS (?)", readExists)
		}
	}

	var total int64
	query.Count(&total)

	var rows []models.Message
	if err := query.Offset((page - 1) * size).Limit(size).Find(&rows).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to load messages"})
		return
	}

	readSet, err := h.loadReadSet(userID, rows)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to load message state"})
		return
	}

	items := make([]messageDTO, 0, len(rows))
	for _, row := range rows {
		_, read := readSet[row.ID]
		items = append(items, toMessageDTO(row, read))
	}
	c.JSON(http.StatusOK, gin.H{"items": items, "total": total})
}

// @Summary      未读消息数（per-user）
// @Tags         messages
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Router       /messages/unread-count [get]
func (h *MessageHandler) HandleUnreadCount(c *gin.Context) {
	userID, username, role := middleware.GetCurrentUser(c)
	query := h.visibleQuery(userID, username, role).
		Where("NOT EXISTS (?)", h.readExistsSubquery(userID))
	var count int64
	query.Count(&count)
	c.JSON(http.StatusOK, gin.H{"count": count})
}

func (h *MessageHandler) HandleMarkRead(c *gin.Context) {
	userID, _, _ := middleware.GetCurrentUser(c)
	id := parsePositiveInt(c.Param("id"), 0)
	if id == 0 {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid message id"})
		return
	}

	if err := h.markRead(userID, uint(id)); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to update message"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"success": true})
}

func (h *MessageHandler) HandleMarkAllRead(c *gin.Context) {
	userID, username, role := middleware.GetCurrentUser(c)

	// 找出当前用户所有未读（可见且无 message_reads 记录）的消息 ID。
	var ids []uint
	if err := h.visibleQuery(userID, username, role).
		Where("NOT EXISTS (?)", h.readExistsSubquery(userID)).
		Pluck("id", &ids).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to update messages"})
		return
	}

	for _, id := range ids {
	if err := h.markRead(userID, uint(id)); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to update messages"})
			return
		}
	}
	c.JSON(http.StatusOK, gin.H{"success": true})
}

// markRead 幂等地为 (userID, messageID) 写入一条 MessageRead 记录。
// 不再更新 Message.Status，避免广播消息单行被多用户共享时的已读状态污染。
func (h *MessageHandler) markRead(userID uint, messageID uint) error {
	if userID == 0 || messageID == 0 {
		return nil
	}
	record := models.MessageRead{UserID: userID, MessageID: messageID}
	return h.db.Where("user_id = ? AND message_id = ?", userID, messageID).
		FirstOrCreate(&record).Error
}

// loadReadSet 返回给定消息中当前用户已读的 messageID 集合。
func (h *MessageHandler) loadReadSet(userID uint, rows []models.Message) (map[uint]struct{}, error) {
	readSet := make(map[uint]struct{}, len(rows))
	if userID == 0 || len(rows) == 0 {
		return readSet, nil
	}
	ids := make([]uint, 0, len(rows))
	for _, r := range rows {
		ids = append(ids, r.ID)
	}
	var readRows []models.MessageRead
	if err := h.db.Where("user_id = ? AND message_id IN ?", userID, ids).Find(&readRows).Error; err != nil {
		return nil, err
	}
	for _, r := range readRows {
		readSet[r.MessageID] = struct{}{}
	}
	return readSet, nil
}

func firstNonEmptyMessageParam(values ...string) string {
	for _, value := range values {
		if strings.TrimSpace(value) != "" {
			return value
		}
	}
	return ""
}
