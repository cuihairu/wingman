package middleware

import (
	"net/http"
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
	attempts  int
	lastReset time.Time
	blockUntil time.Time
}

// NewRateLimiter creates a new rate limiter
func NewRateLimiter() *RateLimiter {
	rl := &RateLimiter{
		clients: make(map[string]*clientInfo),
	}
	// Clean up expired entries every 5 minutes
	go rl.cleanup()
	return rl
}

// MaxAttempts and window duration for rate limiting
const (
	maxAttempts = 5               // Max failed attempts per window
	windowDuration = 15 * time.Minute  // Time window for attempts
	blockDuration = 30 * time.Minute   // How long to block after exceeding max attempts
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

// RecordSuccess resets the attempt counter for a successful login
func (rl *RateLimiter) RecordSuccess(clientID string) {
	rl.mu.Lock()
	defer rl.mu.Unlock()

	if info, exists := rl.clients[clientID]; exists {
		info.attempts = 0
		info.lastReset = time.Now()
	}
}

// cleanup removes old entries to prevent memory leaks
func (rl *RateLimiter) cleanup() {
	ticker := time.NewTicker(5 * time.Minute)
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
			c.JSON(http.StatusTooManyRequests, gin.H{
				"success": false,
				"error": "Too many failed attempts. Please try again later.",
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
