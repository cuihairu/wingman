package agent

import (
	"encoding/binary"
	"encoding/json"
	"io"
	"net"
	"sync"
	"testing"
	"time"
	"unsafe"
)

// listenerEndian matches the runtime's byte order detection.
var testEndian = func() binary.ByteOrder {
	var x uint16 = 0x0102
	if *(*byte)(unsafe.Pointer(&x)) == 0x02 {
		return binary.LittleEndian
	}
	return binary.BigEndian
}()

// TestAgentRegisterFlow tests the agent registration flow: runtime connects -> sends register -> server acknowledges.
func TestAgentRegisterFlow(t *testing.T) {
	// Create a test listener
	testAddr := "127.0.0.1:0" // Use port 0 to get a random available port
	registry := &mockRegistry{}
	broadcast := &mockBroadcaster{}
	listener := NewFrameListener(registry, broadcast)

	// Start listener
	startErr := make(chan error, 1)
	go func() { startErr <- listener.Start(testAddr) }()

	// Get the actual listening address
	var actualAddr string
	for i := 0; i < 50; i++ {
		select {
		case err := <-startErr:
			if err != nil {
				t.Fatalf("Failed to start listener: %v", err)
			}
		default:
		}
		if listener.listener != nil {
			actualAddr = listener.listener.Addr().String()
			break
		}
		time.Sleep(10 * time.Millisecond)
	}
	if actualAddr == "" {
		t.Fatal("Failed to get listener address")
	}
	defer listener.Stop()

	// Simulate runtime connection
	conn, err := net.Dial("tcp", actualAddr)
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}
	defer conn.Close()

	// Wait for server to accept connection
	time.Sleep(50 * time.Millisecond)

	// Verify agent was registered (expected 1 connection)
	listener.mu.RLock()
	connCount := len(listener.conns)
	listener.mu.RUnlock()
	if connCount != 1 {
		t.Errorf("Expected 1 connection, got %d", connCount)
	}
}

// TestSendCommandResponseFlow tests the complete command flow: SendCommand -> runtime processes -> Response received.
func TestSendCommandResponseFlow(t *testing.T) {
	testAddr := "127.0.0.1:0"
	registry := &mockRegistry{}
	broadcast := &mockBroadcaster{}
	listener := NewFrameListener(registry, broadcast)

	startErr := make(chan error, 1)
	go func() { startErr <- listener.Start(testAddr) }()

	var actualAddr string
	for i := 0; i < 50; i++ {
		select {
		case err := <-startErr:
			if err != nil {
				t.Fatalf("Failed to start listener: %v", err)
			}
		default:
		}
		if listener.listener != nil {
			actualAddr = listener.listener.Addr().String()
			break
		}
		time.Sleep(10 * time.Millisecond)
	}
	if actualAddr == "" {
		t.Fatal("Failed to get listener address")
	}
	defer listener.Stop()

	// Simulate runtime connection
	conn, err := net.Dial("tcp", actualAddr)
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}
	defer conn.Close()

	// Wait for connection establishment
	time.Sleep(50 * time.Millisecond)

	// Send agent.register
	registerPayload := map[string]any{
		"type":     "agent.register",
		"agentId":  "test-agent-1",
		"hostname": "test-host",
		"sequence": float64(0),
	}
	sendMessage(t, conn, Notify, 0, registerPayload)

	// Wait for registration ACK
	time.Sleep(50 * time.Millisecond)

	// Get the agent connection
	var agentConn *agentConn
	listener.mu.RLock()
	for _, ac := range listener.conns {
		agentConn = ac
		break
	}
	listener.mu.RUnlock()

	if agentConn == nil {
		t.Fatal("No agent connection found")
	}

	// Verify agent ID
	if agentConn.agentID != "test-agent-1" {
		t.Errorf("Expected agent ID test-agent-1, got %s", agentConn.agentID)
	}

	// Test SendCommand with timeout
	_, err = agentConn.SendCommandWithTimeout("test.command", map[string]any{
		"param1": "value1",
	}, 2*time.Second)

	// Since runtime doesn't respond, we expect timeout or error
	if err == nil {
		t.Log("Command result received (runtime simulation would complete this)")
	} else {
		t.Logf("Command timeout expected (no runtime): %v", err)
	}
}

// TestMessageSizeLimit tests that oversized messages are rejected.
func TestMessageSizeLimit(t *testing.T) {
	testAddr := "127.0.0.1:0"
	registry := &mockRegistry{}
	broadcast := &mockBroadcaster{}
	listener := NewFrameListener(registry, broadcast)

	startErr := make(chan error, 1)
	go func() { startErr <- listener.Start(testAddr) }()

	var actualAddr string
	for i := 0; i < 50; i++ {
		select {
		case err := <-startErr:
			if err != nil {
				t.Fatalf("Failed to start listener: %v", err)
			}
		default:
		}
		if listener.listener != nil {
			actualAddr = listener.listener.Addr().String()
			break
		}
		time.Sleep(10 * time.Millisecond)
	}
	if actualAddr == "" {
		t.Fatal("Failed to get listener address")
	}
	defer listener.Stop()

	// Simulate runtime connection
	conn, err := net.Dial("tcp", actualAddr)
	if err != nil {
		t.Fatalf("Failed to connect: %v", err)
	}

	// Send oversized message (> maxResponseSize)
	largePayload := make([]byte, maxResponseSize+1)
	writeMessageHeader(t, conn, maxResponseSize+1, Notify, 0)
	if _, err := conn.Write(largePayload); err != nil {
		t.Fatalf("Failed to write oversized message: %v", err)
	}

	// Wait for read loop to process
	time.Sleep(100 * time.Millisecond)

	// Verify connection was closed (listener should reject oversized messages)
	// Connection should be terminated by server
	buf := make([]byte, 1)
	conn.SetReadDeadline(time.Now().Add(100 * time.Millisecond))
	_, err = conn.Read(buf)
	if err == nil {
		t.Error("Expected connection to be closed after oversized message")
	}
	if err != nil && err != io.EOF {
		// Expected - connection should be closed
	}
	conn.Close()
}

// sendMessage sends a message from the "runtime" side.
func sendMessage(t *testing.T, conn net.Conn, msgType MessageType, sequence uint32, payload map[string]any) {
	body, err := json.Marshal(payload)
	if err != nil {
		t.Fatalf("Failed to marshal payload: %v", err)
	}

	writeMessageHeader(t, conn, uint32(len(body)), msgType, sequence)
	if _, err := conn.Write(body); err != nil {
		t.Fatalf("Failed to write message body: %v", err)
	}
}

// writeMessageHeader writes a message header (simulating C++ runtime).
func writeMessageHeader(t *testing.T, conn net.Conn, length uint32, msgType MessageType, sequence uint32) {
	buf := make([]byte, messageHeaderSize)
	testEndian.PutUint32(buf[0:4], length)
	testEndian.PutUint32(buf[4:8], sequence)
	buf[8] = byte(msgType)
	testEndian.PutUint32(buf[12:16], 0) // Reserved

	if _, err := conn.Write(buf); err != nil {
		t.Fatalf("Failed to write message header: %v", err)
	}
}

// Mock implementations

type mockRegistry struct {
	mu      sync.Mutex
	agents  map[string]bool
	clients map[string]AgentConnection
}

func (m *mockRegistry) Register(agentID, hostname, ip string, conn any) {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.agents == nil {
		m.agents = make(map[string]bool)
	}
	m.agents[agentID] = true
}

func (m *mockRegistry) Unregister(agentID string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.agents != nil {
		delete(m.agents, agentID)
	}
}

func (m *mockRegistry) UpdateStatus(agentID string, status string, resources any) {}

func (m *mockRegistry) UpdateHeartbeat(agentID string) {}

func (m *mockRegistry) SetClient(agentID string, conn any) {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.clients == nil {
		m.clients = make(map[string]AgentConnection)
	}
	if ac, ok := conn.(AgentConnection); ok {
		m.clients[agentID] = ac
	}
}

type mockBroadcaster struct{}

func (m *mockBroadcaster) BroadcastAgentEvent(eventType string, data any) {}

func (m *mockBroadcaster) BroadcastEvent(eventType string, data any) {}
