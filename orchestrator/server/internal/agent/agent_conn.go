package agent

import (
	"encoding/json"
	"fmt"
	"io"
	"maps"
	"time"
)

// SendCommand sends a command and waits for the response via readLoop dispatch.
func (ac *agentConn) SendCommand(method string, data map[string]any) (map[string]any, error) {
	return ac.SendCommandWithTimeout(method, data, 0)
}

// SendCommandWithTimeout sends a command with a timeout. A timeout of 0 means wait indefinitely.
func (ac *agentConn) SendCommandWithTimeout(method string, data map[string]any, timeout time.Duration) (map[string]any, error) {
	ac.mu.Lock()

	seq := ac.nextSeq
	ac.nextSeq++

	// Register pending response BEFORE writing to prevent race: if runtime responds
	// immediately after write, readLoop must find the pending entry.
	p := &pendingResponse{ch: make(chan *messageOrError, 1)}
	ac.pending[seq] = p

	req := map[string]any{
		"type":     method,
		"method":   method,
		"sequence": float64(seq), // tell the runtime the sequence
	}
	maps.Copy(req, data)

	reqBytes, err := json.Marshal(req)
	if err != nil {
		delete(ac.pending, seq) // Clean up on early return
		ac.mu.Unlock()
		return nil, err
	}

	header := MessageHeader{
		Length:   uint32(len(reqBytes)),
		Type:     Request,
		Sequence: seq,
	}
	if err := ac.writeFrameLocked(&header, reqBytes, notifyWriteTimeout); err != nil {
		delete(ac.pending, seq) // Clean up on write failure
		ac.conn.Close()
		ac.mu.Unlock()
		return nil, err
	}

	ac.mu.Unlock()

	// Wait for readLoop to deliver the response, with optional timeout.
	var result *messageOrError
	if timeout > 0 {
		select {
		case result = <-p.ch:
			// Received response
		case <-time.After(timeout):
			result = &messageOrError{err: fmt.Errorf("command timeout after %v", timeout)}
		}
	} else {
		result = <-p.ch
	}

	// Clean up the pending entry.
	ac.mu.Lock()
	delete(ac.pending, seq)
	ac.mu.Unlock()

	return result.msg, result.err
}

// Close closes the connection.
func (ac *agentConn) Close() {
	ac.conn.Close()
}

func (ac *agentConn) getAgentID() string {
	ac.mu.Lock()
	defer ac.mu.Unlock()
	return ac.agentID
}

func (ac *agentConn) setAgentID(agentID string) {
	ac.mu.Lock()
	ac.agentID = agentID
	ac.mu.Unlock()
}

// agent 推 proxy.* 帧）。写入失败如实返回——中继以此判定拆链，静默会让
// guacd 侧挂到超时。
func (ac *agentConn) SendNotify(msgType string, data map[string]any) error {
	msg := map[string]any{
		"type": msgType,
	}
	maps.Copy(msg, data)

	body, err := json.Marshal(msg)
	if err != nil {
		return fmt.Errorf("marshal notify %s: %w", msgType, err)
	}

	ac.mu.Lock()
	defer ac.mu.Unlock()

	if err := ac.writeNotifyFrameLocked(body); err != nil {
		return err
	}
	return nil
}

// sendNotify sends a Notify message.
func (ac *agentConn) sendNotify(msgType string, data map[string]any) {
	msg := map[string]any{
		"type": msgType,
	}
	maps.Copy(msg, data)

	body, _ := json.Marshal(msg)

	ac.mu.Lock()
	defer ac.mu.Unlock()

	if err := ac.writeNotifyFrameLocked(body); err != nil {
		// 写失败：连接已死/窗口为零，主动关闭让 readLoop 走回收路径，
		// 避免持有锁（含 TeamManager.mu 的同步回调）的一方被无限阻塞。
		ac.conn.Close()
	}
}

// writeNotifyFrameLocked 写入一个 Notify 帧。调用方须持有 ac.mu。
// 写操作带有界超时：agent 停读或对端窗口为零时，5s 内返回错误而不是
// 按 TCP 重传语义挂到分钟级。超时即视为连接失效，由调用方决定拆链。
func (ac *agentConn) writeNotifyFrameLocked(body []byte) error {
	header := MessageHeader{
		Length: uint32(len(body)),
		Type:   Notify,
	}
	return ac.writeFrameLocked(&header, body, notifyWriteTimeout)
}

// writeFrameLocked writes one complete frame with a bounded deadline covering
// both header and body. Without this, either write can hold ac.mu indefinitely
// when the peer stops reading (TCP re-transmit backoff), stalling lock holders
// such as a TeamManager.mu-held sendNotify callback.
func (ac *agentConn) writeFrameLocked(header *MessageHeader, body []byte, timeout time.Duration) error {
	if err := ac.conn.SetWriteDeadline(time.Now().Add(timeout)); err != nil {
		return err
	}
	defer func() {
		_ = ac.conn.SetWriteDeadline(time.Time{})
	}()

	if err := ac.writeMsgHeaderLocked(header); err != nil {
		return err
	}
	_, err := ac.conn.Write(body)
	return err
}

// writeMsgHeaderLocked writes a message header. Caller must hold ac.mu.
func (ac *agentConn) writeMsgHeaderLocked(h *MessageHeader) error {
	buf := make([]byte, messageHeaderSize)
	listenerEndian.PutUint32(buf[0:4], h.Length)
	listenerEndian.PutUint32(buf[4:8], h.Sequence)
	buf[8] = byte(h.Type)
	listenerEndian.PutUint32(buf[12:16], h.Reserved)

	_, err := ac.conn.Write(buf)
	return err
}

// readMsgHeader reads a message header.
func (ac *agentConn) readMsgHeader() (*MessageHeader, error) {
	buf := make([]byte, messageHeaderSize)
	if _, err := io.ReadFull(ac.conn, buf); err != nil {
		return nil, err
	}

	h := &MessageHeader{
		Length:   listenerEndian.Uint32(buf[0:4]),
		Sequence: listenerEndian.Uint32(buf[4:8]),
		Type:     MessageType(buf[8]),
		Reserved: listenerEndian.Uint32(buf[12:16]),
	}
	return h, nil
}
