package agent

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/hex"
	"fmt"
	"log"
	"time"
)

// handleRegister processes agent registration.
func (ac *agentConn) handleRegister(msg map[string]any) {
	agentID, _ := msg["agentId"].(string)
	hostname, _ := msg["hostname"].(string)
	// 平台标识（android/desktop/...）：桌面 agent 旧版本不上报，缺省由
	// Registry 归一为 desktop。见 docs/android-agent-design.md §3.3。
	platform, _ := msg["platform"].(string)

	// 能力集（agent.register 上报，ADR: Capability System）：宽容解析，
	// 字段缺失/类型不符按空集处理，不因此拒绝注册；词汇表外的未知项
	// 照常存储（见 registry.ToJSON 的 unknownCapabilities）。
	capList := []string{}
	if rawCaps, ok := msg["capabilities"].([]any); ok {
		for _, rc := range rawCaps {
			if s, ok := rc.(string); ok && s != "" {
				capList = append(capList, s)
			}
		}
	}

	// 注册鉴权（docs/agent-token-auth-design.md §2/§6）三条路径：
	// ① challenge-response（register 带 challenge:true、不携明文 token，
	//    §6.1）：下发 auth.challenge{nonce}，agent 回 HMAC-SHA256
	//    （key = sha256hex(token)，与 DB 哈希列同值，服务端无需持有明文）。
	//    挑战在独立 goroutine 完成——handleRegister 由 readLoop 驱动，
	//    同步等 Response 会自锁。
	// ② 顶层 token 明文（env 白名单 ∪ DB 哈希，P1/P2.2，双源任一命中放行）。
	// ③ 关闭（env 空 + 无 DB 记录，向后兼容）。
	// 失败路径统一：ack success:false 后立即断连——不 set agentID、不入
	// Registry，未授权连接不允许停留在链路上（readLoop 退出时 agentID 为空
	// 自然跳过 Unregister）。
	if ac.listener.authRequired() {
		if challenge, _ := msg["challenge"].(bool); challenge {
			if !ac.challengeStarted.CompareAndSwap(false, true) {
				ac.rejectRegister(agentID)
				return
			}
			go func() {
				if ac.verifyChallengeHMAC(agentID) {
					ac.completeRegistration(agentID, hostname, platform, capList)
				} else {
					ac.rejectRegister(agentID)
				}
			}()
			return
		}
		token, _ := msg["token"].(string)
		envOK := ac.listener.tokenValid(token)
		storeOK := false
		if store := ac.listener.getTokenStore(); store != nil {
			_, storeOK = store.Verify(token, agentID)
		}
		if !envOK && !storeOK {
			ac.rejectRegister(agentID)
			return
		}
	}

	ac.completeRegistration(agentID, hostname, platform, capList)
}

// completeRegistration 落 Registry 并回注册成功 ack（challenge 异步路径与
// 同步路径共用收尾）。settled CAS 保证注册终态只生效一次。
func (ac *agentConn) completeRegistration(agentID, hostname, platform string, capList []string) {
	if !ac.settled.CompareAndSwap(false, true) {
		return
	}
	if agentID == "" {
		agentID = fmt.Sprintf("agent_%s", ac.conn.RemoteAddr().String())
	}

	ac.setAgentID(agentID)
	ac.listener.registry.Register(agentID, hostname, ac.conn.RemoteAddr().String(), ac)
	ac.listener.registry.UpdatePlatform(agentID, platform)
	ac.listener.registry.UpdateCapabilities(agentID, capList)
	ac.listener.registry.SetClient(agentID, ac)

	ac.sendNotify("agent.register_ack", map[string]any{
		"success": true,
		"agentId": agentID,
	})

	log.Printf("[FrameListener] Agent registered: %s (%s)", agentID, hostname)
}

// rejectRegister 注册拒绝统一路径：ack success:false + 断连 + 日志。
func (ac *agentConn) rejectRegister(agentID string) {
	if !ac.settled.CompareAndSwap(false, true) {
		return
	}
	ac.sendNotify("agent.register_ack", map[string]any{
		"success": false,
		"agentId": agentID,
		"error":   "invalid or missing token",
	})
	log.Printf("[Auth] register rejected from %s (agentId=%q)",
		ac.conn.RemoteAddr(), agentID)
	ac.conn.Close()
}

// challengeTimeout auth.challenge 等待应答的超时；var 供测试缩短。
var challengeTimeout = 5 * time.Second

// verifyChallengeHMAC challenge-response 校验（§6.1）：下发 32 字节 nonce，
// 等 agent 回 HMAC-SHA256(key, nonce)（key = sha256hex(token)，agent 侧
// 自行推导、明文不过网；服务端 env 走明文派生、DB 直接用哈希列）。
func (ac *agentConn) verifyChallengeHMAC(agentID string) bool {
	raw := make([]byte, 32)
	if _, err := rand.Read(raw); err != nil {
		log.Printf("[Auth] challenge nonce generation failed: %v", err)
		return false
	}
	nonceHex := hex.EncodeToString(raw)

	resp, err := ac.SendCommandWithTimeout("auth.challenge",
		map[string]any{"nonce": nonceHex}, challengeTimeout)
	if err != nil {
		log.Printf("[Auth] challenge failed from %s (agentId=%q): %v",
			ac.conn.RemoteAddr(), agentID, err)
		return false
	}
	macHex, _ := resp["hmac"].(string)
	if ac.listener.hmacMatchesEnv(macHex, nonceHex) {
		return true
	}
	if store := ac.listener.getTokenStore(); store != nil {
		if _, ok := store.VerifyHMAC(macHex, nonceHex, agentID); ok {
			return true
		}
	}
	return false
}

// tokenMAC 以 keyHex（token 的 sha256 hex 或 DB 哈希列）为 HMAC-SHA256
// 密钥、nonceHex 为消息，返回 hex 小写 MAC。
func tokenMAC(keyHex, nonceHex string) string {
	h := hmac.New(sha256.New, []byte(keyHex))
	h.Write([]byte(nonceHex))
	return hex.EncodeToString(h.Sum(nil))
}

// sha256Hex returns the lowercase hex sha256 of s.
func sha256Hex(s string) string {
	sum := sha256.Sum256([]byte(s))
	return hex.EncodeToString(sum[:])
}

// hmacMatchesEnv challenge 应答对 env 白名单的校验（明文派生密钥）。
func (l *FrameListener) hmacMatchesEnv(macHex, nonceHex string) bool {
	if macHex == "" {
		return false
	}
	l.mu.RLock()
	tokens := l.agentTokens
	l.mu.RUnlock()
	for _, t := range tokens {
		if subtle.ConstantTimeCompare([]byte(macHex), []byte(tokenMAC(sha256Hex(t), nonceHex))) == 1 {
			return true
		}
	}
	return false
}
