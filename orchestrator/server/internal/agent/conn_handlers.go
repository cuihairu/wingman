package agent

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"strings"
)

// readLoop is the sole reader from the TCP connection.
// It dispatches Response frames to the matching SendCommand caller;
// all other frames are handled inline.
func (ac *agentConn) readLoop() {
	defer func() {
		ac.conn.Close()

		// Fail any pending commands so their goroutines don't hang.
		ac.mu.Lock()
		for seq, p := range ac.pending {
			p.ch <- &messageOrError{err: fmt.Errorf("connection closed")}
			delete(ac.pending, seq)
		}
		ac.mu.Unlock()

		ac.listener.mu.Lock()
		delete(ac.listener.conns, ac.id)
		ac.listener.mu.Unlock()

		if agentID := ac.getAgentID(); agentID != "" {
			ac.listener.registry.Unregister(agentID)
			// 清理该 agent 的收件箱缓冲与团队成员关系（消息未送达即断连，不应残留）。
			// 仅当没有其他活跃连接仍以同一 agentID 注册时才清理，
			// 避免重连竞态下旧连接的 defer 清掉新会话的状态。
			if !ac.listener.hasOtherConnForAgent(agentID, ac.id) {
				if tm := ac.listener.GetTeamManager(); tm != nil {
					tm.RemoveAgent(agentID)
				}
			}
		}
		log.Printf("[FrameListener] Connection closed: %s", ac.id)
	}()

	for {
		select {
		case <-ac.listener.stopCh:
			return
		default:
		}

		header, err := ac.readMsgHeader()
		if err != nil {
			if err != io.EOF {
				log.Printf("[FrameListener] Read header error: %v", err)
			}
			return
		}

		if header.Length > maxResponseSize {
			log.Printf("[FrameListener] Message too large: %d", header.Length)
			return
		}

		body := make([]byte, header.Length)
		if header.Length > 0 {
			if _, err := io.ReadFull(ac.conn, body); err != nil {
				log.Printf("[FrameListener] Read body error: %v", err)
				return
			}
		}

		// If this is a Response with a sequence number, dispatch to the caller.
		// Note: Sequence can be 0 for the first command, so check for Response type only.
		if header.Type == Response {
			ac.dispatchResponse(header.Sequence, body)
			continue
		}

		ac.handleMessage(header, body)
	}
}

// dispatchResponse delivers a response frame to the waiting SendCommand.
func (ac *agentConn) dispatchResponse(seq uint32, body []byte) {
	var msg map[string]any
	if err := json.Unmarshal(body, &msg); err != nil {
		// Still deliver the error so the caller doesn't hang.
		ac.mu.Lock()
		if p, ok := ac.pending[seq]; ok {
			p.ch <- &messageOrError{err: fmt.Errorf("json parse error: %w", err)}
		}
		ac.mu.Unlock()
		return
	}

	ac.mu.Lock()
	p, ok := ac.pending[seq]
	ac.mu.Unlock()

	if ok {
		p.ch <- &messageOrError{msg: msg}
	} else {
		log.Printf("[FrameListener] Orphan response for seq %d", seq)
	}
}

// handleMessage processes inbound messages that are not command responses.
func (ac *agentConn) handleMessage(header *MessageHeader, body []byte) {
	switch header.Type {
	case Notify:
		ac.handleNotify(body)
	case Request:
		ac.handleRequest(header, body)
	default:
		log.Printf("[FrameListener] Unknown message type: %d", header.Type)
	}
}

// handleNotify processes Notify messages.
func (ac *agentConn) handleNotify(body []byte) {
	if len(body) == 4 && string(body) == "PING" {
		ac.sendPong()
		if agentID := ac.getAgentID(); agentID != "" {
			ac.listener.registry.UpdateHeartbeat(agentID)
		}
		return
	}

	var msg map[string]any
	if err := json.Unmarshal(body, &msg); err != nil {
		log.Printf("[FrameListener] JSON parse error: %v", err)
		return
	}

	msgType, _ := msg["type"].(string)

	switch msgType {
	case "agent.register":
		ac.handleRegister(msg)
	case "agent.heartbeat":
		ac.handleHeartbeat(msg)
	case "agent.event":
		ac.handleEvent(msg)
	case "inbox.register":
		ac.handleInboxRegister(msg)
	case "inbox.heartbeat":
		ac.handleInboxHeartbeat(msg)
	case "inbox.ack":
		ac.handleInboxAck(msg)
	case "inbox.report":
		ac.handleInboxReport(msg)
	case "team.join":
		ac.handleTeamJoin(msg)
	case "team.leave":
		ac.handleTeamLeave(msg)
	case "team.vote_create":
		ac.handleTeamVoteCreate(msg)
	case "team.vote_cast":
		ac.handleTeamVoteCast(msg)
	case "team.status_report":
		ac.handleTeamStatusReport(msg)
	case "team.broadcast":
		ac.handleTeamBroadcast(msg)
	case "proxy.data":
		ac.handleProxyData(msg)
	case "proxy.close":
		ac.handleProxyClose(msg)
	case "proxy.error":
		ac.handleProxyError(msg)
	default:
		log.Printf("[FrameListener] Unknown notify type: %s", msgType)
	}
}

// handleProxyData agent → server 方向的代理数据帧。按 proxyId 前缀路由到
// 对应中继（guac: → Guacamole 目标转发）；data 为 base64。未知前缀记日志
// 丢弃——后续端口转发等消费方在此扩路由，与 cockpit proxyMgr 同位。
func (ac *agentConn) handleProxyData(msg map[string]any) {
	proxyID, _ := msg["proxyId"].(string)
	connID, _ := msg["connId"].(string)
	dataB64, _ := msg["data"].(string)
	if proxyID == "" || connID == "" {
		log.Printf("[FrameListener] proxy.data missing proxyId/connId")
		return
	}
	if !strings.HasPrefix(proxyID, GuacRelayPrefix) {
		log.Printf("[FrameListener] proxy.data for unknown prefix: %s", proxyID)
		return
	}
	data, err := base64.StdEncoding.DecodeString(dataB64)
	if err != nil {
		log.Printf("[FrameListener] proxy.data bad base64 for %s: %v", connID, err)
		return
	}
	if err := guacRelayDeliver(proxyID, connID, data); err != nil {
		log.Printf("[FrameListener] proxy.data deliver %s: %v", connID, err)
	}
}

// handleProxyClose agent 侧关闭（目标断开/写错误）。
func (ac *agentConn) handleProxyClose(msg map[string]any) {
	proxyID, _ := msg["proxyId"].(string)
	connID, _ := msg["connId"].(string)
	reason, _ := msg["reason"].(string)
	if !strings.HasPrefix(proxyID, GuacRelayPrefix) {
		return // 非中继会话的关闭通知无需日志噪声
	}
	guacRelayHandleClose(proxyID, connID, reason)
}

// handleProxyError agent 拨目标失败等错误上报 → 拆 guacd 侧连接，让 connect
// 立即失败而非挂到超时。
func (ac *agentConn) handleProxyError(msg map[string]any) {
	proxyID, _ := msg["proxyId"].(string)
	connID, _ := msg["connId"].(string)
	errMsg, _ := msg["error"].(string)
	if !strings.HasPrefix(proxyID, GuacRelayPrefix) {
		log.Printf("[FrameListener] proxy.error for unknown prefix: %s", proxyID)
		return
	}
	guacRelayHandleError(proxyID, connID, errMsg)
}

// handleRequest 应答 runtime 发来的 Request 帧（runtime 主动向服务器取数）。
// 当前 runtime 的 agent 链路（inbox/team 等模块）只发送 Notify 帧，并不发起 Request；
// 此处理为协议防御性兜底：对未知 Request 回空 success:true，保持协议向后兼容
// （runtime 侧对该响应不解析额外字段）。后续若 runtime 依赖具体 Request 方法取数，
// 应在此按 method 分发实现，而不是移除该兜底。
func (ac *agentConn) handleRequest(header *MessageHeader, _ []byte) {
	resp := map[string]any{
		"success": true,
	}
	respBytes, _ := json.Marshal(resp)

	respHeader := MessageHeader{
		Length:   uint32(len(respBytes)),
		Sequence: header.Sequence,
		Type:     Response,
	}

	ac.mu.Lock()
	if err := ac.writeFrameLocked(&respHeader, respBytes, notifyWriteTimeout); err != nil {
		ac.conn.Close()
	}
	ac.mu.Unlock()
}

// handleHeartbeat processes heartbeat reports.
func (ac *agentConn) handleHeartbeat(msg map[string]any) {
	agentID := ac.getAgentID()
	if agentID == "" {
		return
	}

	status := "online"
	if s, ok := msg["status"].(string); ok && s != "" {
		status = s
	}

	resources := msg["resources"]
	// 链路质量统计（reconnects/dropped/outboxPending/lastDisconnectReason/sessionUptimeMs）：
	// 先于 UpdateStatus 写入，随 status_changed 广播携带给 Dashboard。
	if link, ok := msg["link"].(map[string]any); ok {
		ac.listener.registry.UpdateLinkHealth(agentID, link)
	}
	ac.listener.registry.UpdateStatus(agentID, status, resources)
}

// handleEvent processes agent events.
func (ac *agentConn) handleEvent(msg map[string]any) {
	event, _ := msg["event"].(string)
	data := msg["data"]

	switch event {
	case "script_output":
		if ac.listener.onScript != nil {
			dataMap, _ := data.(map[string]any)
			if dataMap == nil {
				dataMap = map[string]any{}
			}
			ac.listener.onScript(ac.getAgentID(), dataMap)
		}
		ac.listener.broadcast.BroadcastEvent("script", map[string]any{
			"event": "output",
			"data":  data,
		})
	case "trigger_fired":
		ac.listener.broadcast.BroadcastAgentEvent("trigger_fired", map[string]any{
			"agentId": ac.getAgentID(),
			"data":    data,
		})
	case "script_state":
		// runtime 推送的脚本状态变更（running/paused/stopped/error），转发给 Dashboard。
		ac.listener.broadcast.BroadcastEvent("script", map[string]any{
			"event":   "state_changed",
			"agentId": ac.getAgentID(),
			"data":    data,
		})
	default:
		log.Printf("[FrameListener] Unknown event: %s", event)
	}
}

// sendPong sends a PONG reply.
func (ac *agentConn) sendPong() {
	ac.mu.Lock()
	defer ac.mu.Unlock()

	header := MessageHeader{
		Length: 4,
		Type:   Notify,
	}
	if err := ac.writeFrameLocked(&header, []byte("PONG"), notifyWriteTimeout); err != nil {
		ac.conn.Close()
	}
}
