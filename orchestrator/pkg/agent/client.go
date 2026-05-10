package agent

import (
	"encoding/binary"
	"encoding/json"
	"fmt"
	"net"
	"sync"
	"time"
)

// C++ MessageHeader 结构（16 字节）
// struct MessageHeader {
//     uint32_t length;      // 消息体长度 (4 bytes)
//     uint32_t sequence;    // 序列号 (4 bytes)
//     MessageType type;     // 消息类型 (1 byte)
//     uint32_t reserved;    // 保留字段 (4 bytes) + 3 bytes padding
// };
const messageHeaderSize = 16

// MessageType 匹配 C++ MessageType 枚举
type MessageType uint8

const (
	Request  MessageType = 1
	Response MessageType = 2
	Notify   MessageType = 3
	Error    MessageType = 4
)

// MessageHeader C++ 兼容的消息头
type MessageHeader struct {
	Length   uint32
	Sequence uint32
	Type     MessageType
	Reserved uint32
}

// Message 消息结构
type Message struct {
	Header MessageHeader
	Body   []byte
}

// Client TCP 客户端，连接到 C++ Client
type Client struct {
	address  string
	conn     net.Conn
	mu       sync.Mutex
	timeout  time.Duration
	sequence uint32
}

// NewClient 创建 Agent 客户端
func NewClient(address string) *Client {
	return &Client{
		address: address,
		timeout: 30 * time.Second,
	}
}

// Connect 连接到 Agent
func (c *Client) Connect() error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.conn != nil {
		return nil
	}

	conn, err := net.DialTimeout("tcp", c.address, c.timeout)
	if err != nil {
		return fmt.Errorf("failed to connect to agent: %w", err)
	}

	c.conn = conn
	return nil
}

// Close 关闭连接
func (c *Client) Close() error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.conn != nil {
		err := c.conn.Close()
		c.conn = nil
		return err
	}
	return nil
}

// Send 发送消息并接收响应
func (c *Client) Send(msgType string, data map[string]interface{}) (map[string]interface{}, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	// 确保已连接
	if c.conn == nil {
		if err := c.Connect(); err != nil {
			return nil, err
		}
	}

	// 构造请求
	req := map[string]interface{}{
		"type": msgType,
	}
	for k, v := range data {
		req[k] = v
	}

	reqBytes, err := json.Marshal(req)
	if err != nil {
		return nil, err
	}

	// 构造 C++ 兼容消息
	c.sequence++
	msg := Message{
		Header: MessageHeader{
			Length:   uint32(len(reqBytes)),
			Sequence: c.sequence,
			Type:     Request,
			Reserved: 0,
		},
		Body: reqBytes,
	}

	// 发送消息头（16 字节）
	headerBuf := make([]byte, messageHeaderSize)
	binary.BigEndian.PutUint32(headerBuf[0:4], msg.Header.Length)
	binary.BigEndian.PutUint32(headerBuf[4:8], msg.Header.Sequence)
	headerBuf[8] = byte(msg.Header.Type)
	binary.BigEndian.PutUint32(headerBuf[12:16], msg.Header.Reserved)

	_, err = c.conn.Write(headerBuf)
	if err != nil {
		return nil, fmt.Errorf("failed to write header: %w", err)
	}

	// 发送消息体
	_, err = c.conn.Write(msg.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to write body: %w", err)
	}

	// 读取响应头（16 字节）
	respHeaderBuf := make([]byte, messageHeaderSize)
	_, err = c.conn.Read(respHeaderBuf)
	if err != nil {
		return nil, fmt.Errorf("failed to read header: %w", err)
	}

	respLength := binary.BigEndian.Uint32(respHeaderBuf[0:4])

	// 读取响应体
	respBytes := make([]byte, respLength)
	_, err = c.conn.Read(respBytes)
	if err != nil {
		return nil, fmt.Errorf("failed to read body: %w", err)
	}

	// 解析响应
	var resp map[string]interface{}
	if err := json.Unmarshal(respBytes, &resp); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	return resp, nil
}

// Ping 健康检查
func (c *Client) Ping() (map[string]interface{}, error) {
	return c.Send("ping", nil)
}

// GetStatus 获取状态
func (c *Client) GetStatus() (map[string]interface{}, error) {
	return c.Send("get_status", nil)
}

// ListScripts 获取脚本列表
func (c *Client) ListScripts() (map[string]interface{}, error) {
	return c.Send("list_scripts", nil)
}

// RunScript 运行脚本
func (c *Client) RunScript(path string) (map[string]interface{}, error) {
	return c.Send("run_script", map[string]interface{}{
		"path": path,
	})
}

// StopScript 停止脚本
func (c *Client) StopScript(scriptId string) (map[string]interface{}, error) {
	return c.Send("stop_script", map[string]interface{}{
		"script_id": scriptId,
	})
}

// GetScriptStatus 获取脚本状态
func (c *Client) GetScriptStatus(scriptId string) (map[string]interface{}, error) {
	return c.Send("get_script_status", map[string]interface{}{
		"script_id": scriptId,
	})
}

// Pool 客户端连接池
type Pool struct {
	clients map[string]*Client
	mu      sync.RWMutex
}

// NewPool 创建连接池
func NewPool() *Pool {
	return &Pool{
		clients: make(map[string]*Client),
	}
}

// Get 获取或创建客户端
func (p *Pool) Get(address string) (*Client, error) {
	p.mu.RLock()
	client, ok := p.clients[address]
	p.mu.RUnlock()

	if ok && client.conn != nil {
		return client, nil
	}

	p.mu.Lock()
	defer p.mu.Unlock()

	// 双重检查
	if client, ok := p.clients[address]; ok && client.conn != nil {
		return client, nil
	}

	client = NewClient(address)
	if err := client.Connect(); err != nil {
		return nil, err
	}

	p.clients[address] = client
	return client, nil
}

// CloseAll 关闭所有连接
func (p *Pool) CloseAll() {
	p.mu.Lock()
	defer p.mu.Unlock()

	for _, client := range p.clients {
		client.Close()
	}
	p.clients = make(map[string]*Client)
}
