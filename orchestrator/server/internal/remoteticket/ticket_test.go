package remoteticket

import (
	"errors"
	"sync"
	"testing"
	"time"
)

func newTestManager(sweep time.Duration) *Manager {
	return newManagerWithSweep(sweep)
}

func TestGenerateAndValidateOnce(t *testing.T) {
	m := newTestManager(time.Hour)
	defer m.Stop()

	tk, err := m.GenerateTicket(7, "alice", map[string]string{"protocol": "vnc", "host": "10.0.0.5"})
	if err != nil {
		t.Fatalf("generate: %v", err)
	}
	if tk.ID == "" || len(tk.ID) != 32 {
		t.Fatalf("ticket id should be 16-byte hex, got %q", tk.ID)
	}
	if tk.Params["protocol"] != "vnc" || tk.UserID != 7 || tk.Username != "alice" {
		t.Fatalf("ticket fields not carried: %+v", tk)
	}

	got, err := m.ValidateTicket(tk.ID)
	if err != nil {
		t.Fatalf("validate: %v", err)
	}
	if got.Params["host"] != "10.0.0.5" {
		t.Fatalf("params lost on validate: %+v", got)
	}

	// 一次性：二次校验必须失败
	if _, err := m.ValidateTicket(tk.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("second validate should be ErrNotFound, got %v", err)
	}
}

func TestValidateUnknownTicket(t *testing.T) {
	m := newTestManager(time.Hour)
	defer m.Stop()
	if _, err := m.ValidateTicket("nonexistent"); !errors.Is(err, ErrNotFound) {
		t.Fatalf("want ErrNotFound, got %v", err)
	}
}

func TestExpiredTicketRejected(t *testing.T) {
	m := newTestManager(time.Hour)
	defer m.Stop()

	tk, err := m.GenerateTicket(1, "bob", nil)
	if err != nil {
		t.Fatalf("generate: %v", err)
	}
	// 直接拨快过期时间，避免测试真实等待 TTL
	m.mu.Lock()
	tk.ExpiresAt = time.Now().Add(-time.Second)
	m.mu.Unlock()

	if _, err := m.ValidateTicket(tk.ID); !errors.Is(err, ErrExpired) {
		t.Fatalf("want ErrExpired, got %v", err)
	}
}

func TestSweepRemovesExpired(t *testing.T) {
	m := newTestManager(10 * time.Millisecond)
	defer m.Stop()

	tk, _ := m.GenerateTicket(1, "carol", nil)
	m.mu.Lock()
	tk.ExpiresAt = time.Now().Add(-time.Second)
	m.mu.Unlock()

	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		m.mu.Lock()
		_, exists := m.tickets[tk.ID]
		m.mu.Unlock()
		if !exists {
			return // 清扫已回收
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatalf("expired ticket not swept within 2s")
}

func TestConcurrentGenerateValidate(t *testing.T) {
	m := newTestManager(time.Hour)
	defer m.Stop()

	const n = 50
	var wg sync.WaitGroup
	errs := make(chan error, n)
	for i := 0; i < n; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			tk, err := m.GenerateTicket(uint(i+1), "load", nil)
			if err != nil {
				errs <- err
				return
			}
			if _, err := m.ValidateTicket(tk.ID); err != nil {
				errs <- err
			}
		}(i)
	}
	wg.Wait()
	close(errs)
	for err := range errs {
		t.Fatalf("concurrent use: %v", err)
	}
}
