#include "wingman/auth.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>

namespace wingman {

static std::string simpleHash(const std::string& input) {
    // Simple hash for demonstration (NOT secure for production)
    // Use bcrypt or Argon2 in production
    unsigned int hash = 0;
    for (char c : input) {
        hash = hash * 31 + c;
    }
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

AuthManager::AuthManager(const std::string& dbPath) : db_(nullptr), dbPath_(dbPath) {}

AuthManager::~AuthManager() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool AuthManager::init() {
    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        spdlog::error("Cannot open database: {}", sqlite3_errmsg(db_));
        return false;
    }

    if (!createTables()) {
        return false;
    }

    // Check if default admin user exists
    const char* checkSql = "SELECT id FROM users WHERE username = 'admin';";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db_, checkSql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to check admin user: {}", sqlite3_errmsg(db_));
        return false;
    }

    bool adminExists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        adminExists = true;
    }
    sqlite3_finalize(stmt);

    // Create default admin user if not exists
    if (!adminExists) {
        spdlog::info("Creating default admin user (admin/admin)");
        return createUser("admin", "admin", "admin");
    }

    return true;
}

bool AuthManager::createTables() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE NOT NULL,"
        "password_hash TEXT NOT NULL,"
        "role TEXT NOT NULL DEFAULT 'user',"
        "created_at INTEGER NOT NULL"
        ");";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to create tables: {}", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

std::string AuthManager::hashPassword(const std::string& password) {
    return simpleHash("wingman_salt_" + password);
}

bool AuthManager::verifyPassword(const std::string& password, const std::string& hash) {
    return hashPassword(password) == hash;
}

std::optional<User> AuthManager::login(const std::string& username, const std::string& password) {
    const char* sql = "SELECT id, username, password_hash, role, created_at FROM users WHERE username = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare login statement: {}", sqlite3_errmsg(db_));
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    User user;
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        user.id = sqlite3_column_int(stmt, 0);
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string storedHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        user.createdAt = sqlite3_column_int64(stmt, 4);

        if (!verifyPassword(password, storedHash)) {
            sqlite3_finalize(stmt);
            spdlog::warn("Login failed for user: {} (wrong password)", username);
            return std::nullopt;
        }
        user.passwordHash = storedHash;
    }

    sqlite3_finalize(stmt);

    if (found) {
        spdlog::info("User logged in: {}", username);
        return user;
    }

    spdlog::warn("Login failed for user: {} (user not found)", username);
    return std::nullopt;
}

bool AuthManager::createUser(const std::string& username, const std::string& password, const std::string& role) {
    const char* sql = "INSERT INTO users (username, password_hash, role, created_at) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare create user statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    std::string hash = hashPassword(password);
    auto now = std::chrono::system_clock::now();
    long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, role.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, timestamp);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        spdlog::error("Failed to create user: {}", sqlite3_errmsg(db_));
        return false;
    }

    spdlog::info("User created: {}", username);
    return true;
}

bool AuthManager::changePassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword) {
    auto user = login(username, oldPassword);
    if (!user) {
        return false;
    }

    const char* sql = "UPDATE users SET password_hash = ? WHERE username = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare change password statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    std::string newHash = hashPassword(newPassword);
    sqlite3_bind_text(stmt, 1, newHash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        spdlog::error("Failed to change password: {}", sqlite3_errmsg(db_));
        return false;
    }

    spdlog::info("Password changed for user: {}", username);
    return true;
}

std::string AuthManager::generateToken(const User& user) {
    // Simplified token generation (NOT secure for production)
    // Use proper JWT in production
    std::stringstream ss;
    ss << user.id << ":" << user.username << ":" << user.role << ":"
       << std::chrono::system_clock::now().time_since_epoch().count();

    std::string tokenData = ss.str();
    std::string token = simpleHash(tokenData);

    // Store token in database for verification
    const char* sql = "CREATE TABLE IF NOT EXISTS tokens (token TEXT PRIMARY KEY, user_id INTEGER, expires_at INTEGER);";
    char* errMsg = nullptr;
    sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);

    auto now = std::chrono::system_clock::now();
    auto expires = now + std::chrono::hours(24);
    long long exp = std::chrono::duration_cast<std::chrono::seconds>(expires.time_since_epoch()).count();

    const char* insertSql = "INSERT INTO tokens (token, user_id, expires_at) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, user.id);
    sqlite3_bind_int64(stmt, 3, exp);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return token;
}

std::optional<User> AuthManager::verifyToken(const std::string& token) {
    const char* sql = "SELECT t.user_id, u.username, u.password_hash, u.role, u.created_at, t.expires_at "
                      "FROM tokens t JOIN users u ON t.user_id = u.id WHERE t.token = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

    User user;
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        user.id = sqlite3_column_int(stmt, 0);
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        user.createdAt = sqlite3_column_int64(stmt, 4);
        long long expiresAt = sqlite3_column_int64(stmt, 5);

        auto now = std::chrono::system_clock::now();
        long long nowTs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        if (nowTs > expiresAt) {
            sqlite3_finalize(stmt);
            // Clean up expired token
            const char* delSql = "DELETE FROM tokens WHERE token = ?;";
            sqlite3_stmt* delStmt;
            sqlite3_prepare_v2(db_, delSql, -1, &delStmt, nullptr);
            sqlite3_bind_text(delStmt, 1, token.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(delStmt);
            sqlite3_finalize(delStmt);
            return std::nullopt;
        }
    }

    sqlite3_finalize(stmt);
    return found ? std::optional<User>(user) : std::nullopt;
}

} // namespace wingman
