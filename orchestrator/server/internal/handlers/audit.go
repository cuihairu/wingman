package handlers

import (
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"strings"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

type AuditHandler struct {
	db *gorm.DB
}

type AuditEventResponse struct {
	Hash   string         `json:"hash"`
	Time   string         `json:"time"`
	Kind   string         `json:"kind"`
	Actor  string         `json:"actor,omitempty"`
	Target string         `json:"target,omitempty"`
	Meta   map[string]any `json:"meta,omitempty"`
}

func NewAuditHandler(db *gorm.DB) *AuditHandler {
	return &AuditHandler{db: db}
}

func (h *AuditHandler) HandleList(c *gin.Context) {
	page := parsePositiveInt(c.Query("page"), 1)
	size := parsePositiveInt(c.Query("size"), 20)
	if size > 200 {
		size = 200
	}

	query := h.db.Model(&models.AuditLog{}).Order("created_at desc")
	if actor := strings.TrimSpace(c.Query("actor")); actor != "" {
		query = query.Where("actor = ?", actor)
	}

	kinds := splitCSV(c.Query("kinds"))
	if len(kinds) > 0 {
		query = query.Where("kind IN ?", kinds)
	}

	if start := parseRFC3339(c.Query("start")); !start.IsZero() {
		query = query.Where("created_at >= ?", start)
	}
	if end := parseRFC3339(c.Query("end")); !end.IsZero() {
		query = query.Where("created_at <= ?", end)
	}

	var total int64
	query.Count(&total)

	var rows []models.AuditLog
	if err := query.Offset((page - 1) * size).Limit(size).Find(&rows).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to load audit logs"})
		return
	}

	events := make([]AuditEventResponse, 0, len(rows))
	for _, row := range rows {
		meta := map[string]any{}
		if strings.TrimSpace(row.Meta) != "" {
			_ = json.Unmarshal([]byte(row.Meta), &meta)
		}
		events = append(events, AuditEventResponse{
			Hash:   fmt.Sprintf("%d", row.ID),
			Time:   row.CreatedAt.Format(time.RFC3339),
			Kind:   row.Kind,
			Actor:  row.Actor,
			Target: row.Target,
			Meta:   meta,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"events": events,
		"total":  total,
	})
}

func WriteAuditLog(db *gorm.DB, actor, kind, target string, meta map[string]any) {
	if db == nil || strings.TrimSpace(kind) == "" {
		return
	}

	if meta != nil {
		if region, ok := deriveIPRegion(meta); ok {
			meta["ip_region"] = region
		}
	}

	payload := "{}"
	if len(meta) > 0 {
		if encoded, err := json.Marshal(meta); err == nil {
			payload = string(encoded)
		}
	}

	db.Create(&models.AuditLog{
		Actor:  actor,
		Kind:   kind,
		Target: target,
		Meta:   payload,
	})
}

// deriveIPRegion tags loopback/private/link-local IPs as "LAN" so the dashboard
// can show a meaningful Region column without a GeoIP dependency. Public IPs
// are left for a future GeoIP integration to fill in.
func deriveIPRegion(meta map[string]any) (string, bool) {
	if _, exists := meta["ip_region"]; exists {
		return "", false
	}
	raw, ok := meta["ip"]
	if !ok {
		return "", false
	}
	ipStr, ok := raw.(string)
	if !ok {
		return "", false
	}
	ipStr = strings.TrimSpace(ipStr)
	if ipStr == "" {
		return "", false
	}
	parsed := net.ParseIP(ipStr)
	if parsed == nil {
		return "", false
	}
	if parsed.IsLoopback() || parsed.IsPrivate() || parsed.IsLinkLocalUnicast() || parsed.IsInterfaceLocalMulticast() {
		return "LAN", true
	}
	return "", false
}

func parsePositiveInt(value string, fallback int) int {
	if strings.TrimSpace(value) == "" {
		return fallback
	}
	var result int
	_, err := fmt.Sscanf(value, "%d", &result)
	if err != nil || result <= 0 {
		return fallback
	}
	return result
}

func splitCSV(value string) []string {
	if strings.TrimSpace(value) == "" {
		return nil
	}
	parts := strings.Split(value, ",")
	items := make([]string, 0, len(parts))
	for _, part := range parts {
		trimmed := strings.TrimSpace(part)
		if trimmed != "" {
			items = append(items, trimmed)
		}
	}
	return items
}

func parseRFC3339(value string) time.Time {
	if strings.TrimSpace(value) == "" {
		return time.Time{}
	}
	parsed, err := time.Parse(time.RFC3339, value)
	if err != nil {
		return time.Time{}
	}
	return parsed
}
