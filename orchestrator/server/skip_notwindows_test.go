//go:build !windows

package main

import "testing"

// skipOnWindows 在非 Windows 平台上为 no-op。
func skipOnWindows(_ *testing.T) {}
