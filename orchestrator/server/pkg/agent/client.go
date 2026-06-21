// Package agent provides the TCP frame listener and registry for runtime
// communication. The runtime (C++ agent) connects outbound to the Go server's
// FrameListener; handlers obtain agent connections via the Registry.
//
// This file holds the shared wire-protocol primitive types used by the
// listener. The legacy dialing Client/Pool (which violated the "server does
// not dial runtime" constraint) have been removed — use FrameListener + Registry.
package agent

// C++ MessageHeader 结构（16 字节，小端）
//
//	struct MessageHeader {
//	    uint32_t length;      // 消息体长度 (4 bytes)
//	    uint32_t sequence;    // 序列号 (4 bytes)
//	    MessageType type;     // 消息类型 (1 byte)
//	    uint32_t reserved;    // 保留字段 (4 bytes) + 3 bytes padding
//	};
const messageHeaderSize = 16
const maxResponseSize = 16 * 1024 * 1024

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
