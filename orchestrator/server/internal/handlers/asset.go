package handlers

import (
	"encoding/base64"
	"errors"
	"net/http"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/assets"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// AssetHandler 资产（模板图片等脚本配套文件）管理：上传/列表/内容/删除 +
// 经 agent 通道 asset.sync 下发（Dispatcher Reuse；服务端存储 → 目标 agent
// 落盘其本地资产目录，响应落盘路径供脚本引用，见 docs/protocols.md ② 与
// docs/architecture-decisions.md「Agent Groups & Batch Operations」）。
type AssetHandler struct {
	db       *gorm.DB
	store    *assets.Store
	registry *agent.Registry
}

// NewAssetHandler 创建资产处理器。
func NewAssetHandler(db *gorm.DB, store *assets.Store, registry *agent.Registry) *AssetHandler {
	return &AssetHandler{db: db, store: store, registry: registry}
}

// AssetUploadRequest 上传资产请求（内联 base64，1MB 上限同 MaxInlineScriptSize）。
// @Description 资产名白名单 [A-Za-z0-9][A-Za-z0-9._-]{0,127}（不含路径分隔符，防穿越）。
type AssetUploadRequest struct {
	// Name 资产名（落盘文件名，扩展名保留）
	Name string `json:"name" binding:"required" example:"login_button.png"`
	// ContentBase64 内容 base64（base64.StdEncoding）
	ContentBase64 string `json:"contentBase64" binding:"required" example:"iVBORw0KGgo="`
}

// AssetSyncRequest 资产下发请求（asset.sync 命令语义）。
type AssetSyncRequest struct {
	BatchAgentSelector
	// Name 服务端资产名（须在服务端存储中存在）
	Name string `json:"name" binding:"required" example:"login_button.png"`
}

// HandleUpload 上传资产（服务端存储）
// @Summary      上传资产
// @Description  上传模板图片等脚本配套文件到服务端资产存储（1MB 上限；同名覆盖，
// @Description  sha256 随内容更新即隐式版本号）。
// @Tags         assets
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request body AssetUploadRequest true "资产名 + base64 内容"
// @Success      200  {object}  AssetUploadResponse
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Router       /assets [post]
func (h *AssetHandler) HandleUpload(c *gin.Context) {
	var req AssetUploadRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	if _, err := assets.SanitizeName(req.Name); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	content, err := base64.StdEncoding.DecodeString(req.ContentBase64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "contentBase64 is not valid base64"})
		return
	}
	meta, err := h.store.Save(req.Name, content)
	if err != nil {
		switch {
		case errors.Is(err, assets.ErrAssetTooLarge):
			c.JSON(http.StatusRequestEntityTooLarge, gin.H{"success": false, "error": err.Error()})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to save asset"})
		}
		return
	}
	c.JSON(http.StatusOK, gin.H{"success": true, "data": meta})
}

// HandleList 列出服务端资产（元数据，按名称排序）。
// @Summary      资产列表
// @Description  返回服务端资产存储中的全部资产（name/size/sha256/uploadedAt）。
// @Tags         assets
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  AssetListResponse
// @Router       /assets [get]
func (h *AssetHandler) HandleList(c *gin.Context) {
	metas, err := h.store.List()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to list assets"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"success": true, "data": metas})
}

// HandleContent 下载资产原始字节。
// @Summary      资产内容
// @Description  按资产名返回原始内容字节（Content-Type: application/octet-stream）。
// @Tags         assets
// @Produce      application/octet-stream
// @Security     BearerAuth
// @Param        name query string true "资产名"
// @Success      200  "资产内容"
// @Failure      400  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Router       /assets/content [get]
func (h *AssetHandler) HandleContent(c *gin.Context) {
	name := c.Query("name")
	if _, err := assets.SanitizeName(name); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	content, _, err := h.store.Get(name)
	if err != nil {
		if errors.Is(err, assets.ErrAssetNotFound) {
			c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "asset not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to read asset"})
		return
	}
	c.Data(http.StatusOK, "application/octet-stream", content)
}

// HandleDelete 删除服务端资产。
// @Summary      删除资产
// @Description  删除服务端资产内容与元数据；不级联删除已下发的 agent 副本。
// @Tags         assets
// @Produce      json
// @Security     BearerAuth
// @Param        name query string true "资产名"
// @Success      200  {object}  AssetDeleteResponse
// @Failure      400  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Router       /assets [delete]
func (h *AssetHandler) HandleDelete(c *gin.Context) {
	name := c.Query("name")
	if _, err := assets.SanitizeName(name); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	if err := h.store.Delete(name); err != nil {
		if errors.Is(err, assets.ErrAssetNotFound) {
			c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "asset not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to delete asset"})
		return
	}
	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "asset.delete", name, nil)
	c.JSON(http.StatusOK, gin.H{"success": true, "data": map[string]any{"name": name}})
}

// HandleSync 按选择器下发资产到 agent（asset.sync 命令 fan-out，10s/台）
// @Summary      下发资产
// @Description  按选择器（agentIds/tags 并集）对多台 agent 并发下发 asset.sync：
// @Description  服务端资产内容内联 base64（与 run_script 内联同口径），agent 落盘其
// @Description  本地资产目录，响应 data 含各台落盘路径。部分失败不算整体失败：
// @Description  一律 200，逐台结果在 results 标注（离线 agent 记 "agent offline"）。
// @Tags         assets
// @Produce      json
// @Security     BearerAuth
// @Param        request body AssetSyncRequest true "选择器 + 资产名"
// @Success      200  {object}  BatchSummaryResponse
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Router       /assets/sync [post]
func (h *AssetHandler) HandleSync(c *gin.Context) {
	var req AssetSyncRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	if _, err := assets.SanitizeName(req.Name); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	content, meta, err := h.store.Get(req.Name)
	if err != nil {
		if errors.Is(err, assets.ErrAssetNotFound) {
			c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "asset not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to read asset"})
		return
	}

	ids, tagList, err := parseBatchSelector(req.AgentIDs, req.Tags)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	targets := resolveBatchTargets(h.registry, ids, tagList)
	encoded := base64.StdEncoding.EncodeToString(content)
	summary := runBatch(targets, func(info *agent.AgentInfo) (bool, string) {
		if info.Client == nil || info.Status != agent.StatusOnline {
			return false, "agent offline"
		}
		resp, err := info.Client.SendCommandWithTimeout("asset.sync", map[string]any{
			"name":          req.Name,
			"contentBase64": encoded,
			"sha256":        meta.Sha256,
		}, 10*time.Second)
		if err != nil {
			return false, err.Error()
		}
		if success, ok := resp["success"].(bool); ok && !success {
			return false, commandErrorText(resp, nil)
		}
		return true, ""
	})

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "asset.sync", req.Name, map[string]any{
		"sha256": meta.Sha256,
		"size":   meta.Size,
		"ids":    ids,
		"tags":   tagList,
	})

	c.JSON(http.StatusOK, gin.H{"success": true, "data": summary})
}
