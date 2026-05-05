#pragma once

#include <string>
#include <optional>
#include <sqlite3.h>

namespace wingman {

struct User {
    int id;
    std::string username;
    std::string passwordHash;
    std::string role;
    long long createdAt;
};

class AuthManager {
public:
    explicit AuthManager(const std::string& dbPath = "wingman.db");
    ~AuthManager();

    // Initialize database and create default user if not exists
    bool init();

    // Verify login credentials
    std::optional<User> login(const std::string& username, const std::string& password);

    // Create new user
    bool createUser(const std::string& username, const std::string& password, const std::string& role = "user");

    // Change password
    bool changePassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword);

    // Generate JWT-like token (simplified)
    std::string generateToken(const User& user);

    // Verify token
    std::optional<User> verifyToken(const std::string& token);

private:
    sqlite3* db_;
    std::string dbPath_;

    bool createTables();
    std::string hashPassword(const std::string& password);
    bool verifyPassword(const std::string& password, const std::string& hash);
};

} // namespace wingman
