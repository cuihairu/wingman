package middleware

import (
	"net/http"
	"strconv"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
)

// RateLimiter protects endpoints from brute force attacks
type RateLimiter struct {
	mu      sync.Mutex
	clients map[string]*clientInfo
}

type clientInfo struct {
	attempts   int
	lastReset  time.Time
	blockUntil time.Time
}

// NewRateLimiter creates a new rate limiter
func NewRateLimiter() *RateLimiter {
	rl := &RateLimiter{
		clients: make(map[string]*clientInfo),
	}
	// Clean up expired entries every 5 minutes
	// 在 go 语句处同步捕获间隔：读取发生在构造内，与测试对 cleanupInterval
	// 的写入有清晰的 happens-before，避免后台 goroutine 延迟读取引发数据竞争。
	go rl.cleanup(cleanupInterval)
	return rl
}

// MaxAttempts and window duration for rate limiting
const (
	maxAttempts    = 5                // Max failed attempts per window
	windowDuration = 15 * time.Minute // Time window for attempts
	blockDuration  = 30 * time.Minute // How long to block after exceeding max attempts
)

// Check returns true if the request should be allowed
func (rl *RateLimiter) Check(clientID string) bool {
	rl.mu.Lock()
	defer rl.mu.Unlock()

	now := time.Now()
	info, exists := rl.clients[clientID]

	if !exists {
		rl.clients[clientID] = &clientInfo{
			attempts:  1,
			lastReset: now,
		}
		return true
	}

	// Check if client is currently blocked
	if now.Before(info.blockUntil) {
		return false
	}

	// Reset counter if window has expired
	if now.Sub(info.lastReset) > windowDuration {
		info.attempts = 1
		info.lastReset = now
		return true
	}

	// Increment attempts
	info.attempts++

	// Block if max attempts exceeded
	if info.attempts > maxAttempts {
		info.blockUntil = now.Add(blockDuration)
		return false
	}

	return true
}

// BlockRemaining returns how long clientID is still blocked (0 if not blocked).
// Used to answer 429 responses with a concrete retry hint.
func (rl *RateLimiter) BlockRemaining(clientID string) time.Duration {
	rl.mu.Lock()
	defer rl.mu.Unlock()

	info, exists := rl.clients[clientID]
	if !exists {
		return 0
	}
	if rem := time.Until(info.blockUntil); rem > 0 {
		return rem
	}
	return 0
}

// RecordSuccess resets the attempt counter for a successful login
func (rl *RateLimiter) RecordSuccess(clientID string) {
	rl.mu.Lock()
	defer rl.mu.Unlock()

	if info, exists := rl.clients[clientID]; exists {
		info.attempts = 0
		info.lastReset = time.Now()
	}
}

// cleanupInterval controls how often the background cleanup goroutine runs.
// Kept as a variable so tests can shorten it.
var cleanupInterval = 5 * time.Minute

// cleanup removes old entries to prevent memory leaks.
// interval 在启动时由调用方捕获传入。
func (rl *RateLimiter) cleanup(interval time.Duration) {
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for range ticker.C {
		rl.mu.Lock()
		now := time.Now()
		for id, info := range rl.clients {
			// Remove entries that haven't been used in over an hour
			if now.Sub(info.lastReset) > time.Hour && now.After(info.blockUntil) {
				delete(rl.clients, id)
			}
		}
		rl.mu.Unlock()
	}
}

// RateLimitMiddleware returns a middleware that enforces rate limiting
// Uses IP address as the client identifier
func RateLimitMiddleware(rl *RateLimiter) gin.HandlerFunc {
	return func(c *gin.Context) {
		clientID := c.ClientIP()

		if !rl.Check(clientID) {
			// 附带剩余封禁秒数：前端据此渲染「请 N 分钟后再试」，不再只弹通用文案
			retryAfter := int((rl.BlockRemaining(clientID) + time.Second - 1) / time.Second)
			if retryAfter < 1 {
				retryAfter = 1
			}
			c.Header("Retry-After", strconv.Itoa(retryAfter))
			c.JSON(http.StatusTooManyRequests, gin.H{
				"success":             false,
				"error":               "Too many failed attempts. Please try again later.",
				"retry_after_seconds": retryAfter,
			})
			c.Abort()
			return
		}

		c.Next()
	}
}

// Global rate limiter instance
var globalRateLimiter = NewRateLimiter()

// GetRateLimiter returns the global rate limiter instance
func GetRateLimiter() *RateLimiter {
	return globalRateLimiter
}
