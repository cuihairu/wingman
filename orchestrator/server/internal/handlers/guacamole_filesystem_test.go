package handlers

// 网关对 Guacamole 对象流（filesystem 通道）的透传契约：文件浏览器（设计
// §15 第一版，SSH/SFTP 树）全部走 guacd 的 filesystem/get/put/body/blob 指令，
// 网关必须逐字节原样双向转发——浏览器只讲协议、网关不解析（DG-6 一套不各写
// 一遍的半边：网关若开始理解对象流，两处文件面必然分叉）。
//
// 用 mock guacd 的 postConnect 钩子在真 TCP 连接上收发：guacd 侧发
// filesystem（对象上线）与 body/blob/end（目录内容），浏览器侧发 get（列目
// 录/下载请求）与 put（上传流），双向断言逐字节一致。ping 类内部指令
// （空 opcode）的回显语义由既有 wsToGuacd 注释与 uuid 首帧用例覆盖。

import (
	"bufio"
	"net"
	"strings"
	"testing"
	"time"

	"github.com/gorilla/websocket"
)

// TestGuacGatewayObjectStreamPassesThrough 验证 filesystem 对象通道双向逐字
// 节透传：guacd→浏览器（filesystem/body/blob/end）与浏览器→guacd（get/put/
// ack）。帧内容即 common-js requestInputStream/createObjectOutputStream/
// sendAck 的实际发码形状。
func TestGuacGatewayObjectStreamPassesThrough(t *testing.T) {
	// guacd → 浏览器的指令序列（真实 guacd SFTP 对象的形状：对象上线 +
	// 目录 body + 一块 blob + 流结束）
	guacdToClient := []string{
		guacEncodeForTest("filesystem", "0", "/"),        // 对象 0 上线，名为 /
		guacEncodeForTest("body", "0", "text/json", "/"), // get / 的目录体流
		guacEncodeForTest("blob", "0", "W10="),           // base64("[]")：空目录
		guacEncodeForTest("end", "0"),                    // 体流结束
	}
	// 浏览器 → guacd 的指令序列（common-js 实际发码形状）
	clientToGuacd := []string{
		guacEncodeForTest("get", "0", "/"),                                      // requestInputStream：列根目录
		guacEncodeForTest("ack", "0", "ok", "0x0"),                              // 收到 blob 逐块确认
		guacEncodeForTest("put", "0", "4", "application/octet-stream", "a.txt"), // createObjectOutputStream：上传
	}

	// mock guacd 侧：发 filesystem → 收 get → 发 body/blob/end → 收 ack →
	// 收 put → 逐字节记录浏览器发来的全部指令
	guacdSeen := make(chan string, len(clientToGuacd))
	m := startMockGuacd(t, guacRealSSHArgs)
	m.postConnect = func(conn net.Conn, rd *bufio.Reader) {
		for _, f := range guacdToClient[:1] {
			if _, err := conn.Write([]byte(f)); err != nil {
				return
			}
		}
		// 依次读浏览器指令（get → ack → put），遇错即结束（网关已关）
		for range clientToGuacd {
			s, err := rd.ReadString(';')
			if err != nil {
				return
			}
			guacdSeen <- strings.TrimSuffix(s, ";")
		}
		// 浏览器指令全部到齐后再发目录体序列，避免与 filesystem 抢序
		for _, f := range guacdToClient[1:] {
			if _, err := conn.Write([]byte(f)); err != nil {
				return
			}
		}
		_ = conn.SetDeadline(time.Now().Add(2 * time.Second))
	}
	env := newGuacMockEnv(t, m.ln.Addr().String(), "", "", t.TempDir())
	ticket := env.requestTicket(t, `{"agentId":"guac-agent","protocol":"ssh","username":"u","password":"p"}`)

	dialer := websocket.Dialer{HandshakeTimeout: 5 * time.Second}
	wsURL := "ws" + strings.TrimPrefix(env.srv.URL, "http") + "/api/remote/guacamole?ticket=" + ticket
	ws, _, err := dialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatalf("ws dial: %v", err)
	}
	defer ws.Close()

	readFrame := func() string {
		t.Helper()
		_ = ws.SetReadDeadline(time.Now().Add(5 * time.Second))
		_, data, err := ws.ReadMessage()
		if err != nil {
			t.Fatalf("read ws frame: %v", err)
		}
		return strings.TrimSuffix(string(data), ";")
	}

	// 首帧为隧道 uuid（空 opcode 内部指令）
	if first := readFrame(); !strings.HasPrefix(first, "0.,") {
		t.Fatalf("first frame should be tunnel uuid, got %q", first)
	}

	// 浏览器侧：看到 filesystem 对象上线后发 get，再逐块 ack，最后 put
	wsWrite := func(frame string) {
		t.Helper()
		_ = ws.SetWriteDeadline(time.Now().Add(5 * time.Second))
		if err := ws.WriteMessage(websocket.TextMessage, []byte(frame)); err != nil {
			t.Fatalf("write ws frame: %v", err)
		}
	}
	if got := readFrame(); got != strings.TrimSuffix(guacdToClient[0], ";") {
		t.Fatalf("filesystem instruction not passed through verbatim: %q", got)
	}
	for _, f := range clientToGuacd {
		wsWrite(f)
	}

	// guacd 侧按序收到 get/ack/put，逐字节一致
	for _, want := range clientToGuacd {
		select {
		case got := <-guacdSeen:
			if got != strings.TrimSuffix(want, ";") {
				t.Errorf("guacd got %q, want %q", got, strings.TrimSuffix(want, ";"))
			}
		case <-time.After(5 * time.Second):
			t.Fatalf("guacd did not receive %q", want)
		}
	}

	// 浏览器侧按序收到 body/blob/end，逐字节一致
	for _, want := range guacdToClient[1:] {
		if got := readFrame(); got != strings.TrimSuffix(want, ";") {
			t.Errorf("client got %q, want %q", got, strings.TrimSuffix(want, ";"))
		}
	}
}
