#pragma once

#include <string>

namespace wingman {

// ========== TOTP (RFC 6238) ==========

// Generate TOTP code from a Base32 secret key
std::string generateTOTP(const std::string& secret, int digits = 6, int period = 30);

// Verify a TOTP code against a Base32 secret key
bool verifyTOTP(const std::string& secret, const std::string& code, int digits = 6, int period = 30, int window = 1);

// Get remaining seconds before the current TOTP window expires
int getRemainingSeconds(int period = 30);

// ========== Steam Guard ==========

// Generate Steam Guard code from a Base64/Base32 secret key
std::string generateSteamGuard(const std::string& secret);

} // namespace wingman
