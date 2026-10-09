#include "deps/httplib.h"
#include "deps/json.hpp"
#include "deps/sqlite3.h"
#include "utils.h"


#include <string>
#include <cstdint>
#include <sstream>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

// ==================== SQLite DB ====================

sqlite3* db = nullptr;
std::mutex db_mutex;

void init_db() {
    sqlite3_open("users.db", &db);
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            email TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            confirmed INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE IF NOT EXISTS sessions (
            token TEXT PRIMARY KEY,
            user_id INTEGER NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(user_id) REFERENCES users(id)
        );
    )";
    char* err = nullptr;
    sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (err) { std::cerr << "DB error: " << err << std::endl; sqlite3_free(err); }
}

// ==================== Helpers ====================
// (bitmix_hash / sha256_simple / generate_code / generate_token / url_encode
//  / extract_bearer_token zyja teraz w utils.h - czysta, testowalna logika)

int get_user_id_from_token(const std::string& token) {
    std::lock_guard<std::mutex> lock(db_mutex);
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, "SELECT user_id FROM sessions WHERE token = ?", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
    int user_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        user_id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return user_id;
}

std::string get_token_from_request(const httplib::Request& req) {
    auto it = req.headers.find("Authorization");
    if (it == req.headers.end()) return "";
    return extract_bearer_token(it->second);
}

bool check_auth(const httplib::Request& req, httplib::Response& res) {
    if (get_user_id_from_token(get_token_from_request(req)) < 0) {
        res.status = 401;
        res.set_content(R"({"error":"Unauthorized"})", "application/json");
        return false;
    }
    return true;
}

// ==================== Pending confirmations ====================

std::mutex pending_mutex;
struct PendingUser { std::string username, email, password_hash, confirm_code; };
std::unordered_map<std::string, PendingUser> pending_confirmations;

#include "weather.h"

// ==================== Auth routes ====================

void setup_auth_routes(httplib::Server& svr) {

    svr.Post("/api/register", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string email = body.value("email", "");
            std::string password = body.value("password", "");

            if (username.empty() || email.empty() || password.size() < 4) {
                res.status = 400;
                res.set_content(R"({"error":"Username, email required, password min 4 chars"})", "application/json");
                return;
            }

            {
                std::lock_guard<std::mutex> lock(db_mutex);
                sqlite3_stmt* stmt;
                sqlite3_prepare_v2(db, "SELECT id FROM users WHERE username = ? OR email = ?", -1, &stmt, nullptr);
                sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);
                bool exists = sqlite3_step(stmt) == SQLITE_ROW;
                sqlite3_finalize(stmt);
                if (exists) {
                    res.status = 409;
                    res.set_content(R"({"error":"User already exists"})", "application/json");
                    return;
                }
            }

            std::string code = generate_code();
            std::string hash = sha256_simple(password);
            {
                std::lock_guard<std::mutex> lock(pending_mutex);
                pending_confirmations[code] = {username, email, hash, code};
            }

            json out;
            out["message"] = "Kod potwierdzajacy wygenerowany. W produkcji bylby wyslany na email.";
            out["confirm_code"] = code;
            out["email"] = email;
            res.set_content(out.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON"})", "application/json");
        }
    });

    svr.Post("/api/confirm", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string code = body.value("code", "");

            PendingUser user;
            {
                std::lock_guard<std::mutex> lock(pending_mutex);
                auto it = pending_confirmations.find(code);
                if (it == pending_confirmations.end()) {
                    res.status = 400;
                    res.set_content(R"({"error":"Invalid confirmation code"})", "application/json");
                    return;
                }
                user = it->second;
                pending_confirmations.erase(it);
            }
            {
                std::lock_guard<std::mutex> lock(db_mutex);
                sqlite3_stmt* stmt;
                sqlite3_prepare_v2(db,
                    "INSERT INTO users (username, email, password_hash, confirmed) VALUES (?, ?, ?, 1)",
                    -1, &stmt, nullptr);
                sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, user.email.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 3, user.password_hash.c_str(), -1, SQLITE_TRANSIENT);
                int rc = sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                if (rc != SQLITE_DONE) {
                    res.status = 500;
                    res.set_content(R"({"error":"Failed to create user"})", "application/json");
                    return;
                }
            }
            json out;
            out["message"] = "Konto potwierdzone! Mozesz sie zalogowac.";
            res.set_content(out.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON"})", "application/json");
        }
    });

    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");
            std::string hash = sha256_simple(password);

            std::lock_guard<std::mutex> lock(db_mutex);
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db,
                "SELECT id FROM users WHERE username = ? AND password_hash = ? AND confirmed = 1",
                -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) != SQLITE_ROW) {
                sqlite3_finalize(stmt);
                res.status = 401;
                res.set_content(R"({"error":"Bledne dane lub konto niepotwierdzone"})", "application/json");
                return;
            }
            int user_id = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);

            std::string token = generate_token();
            sqlite3_prepare_v2(db, "INSERT INTO sessions (token, user_id) VALUES (?, ?)", -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, user_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            json out;
            out["token"] = token;
            out["message"] = "Zalogowano!";
            res.set_content(out.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON"})", "application/json");
        }
    });

    svr.Post("/api/logout", [](const httplib::Request& req, httplib::Response& res) {
        std::string token = get_token_from_request(req);
        if (!token.empty()) {
            std::lock_guard<std::mutex> lock(db_mutex);
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db, "DELETE FROM sessions WHERE token = ?", -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        res.set_content(R"({"message":"Wylogowano"})", "application/json");
    });
}

// ==================== Main ====================

int main() {
    init_db();

    httplib::Server svr;
    svr.set_mount_point("/", "./static");

    setup_auth_routes(svr);
    setup_api_routes(svr);

    int port = 8080;
    if (const char* configured = std::getenv("PORT")) {
        try { port = std::stoi(configured); } catch (...) { return 1; }
        if (port < 1 || port > 65535) return 1;
    }
    std::cout << "Serwer dziala na http://localhost:" << port << std::endl;
    if (!svr.listen("0.0.0.0", port)) { sqlite3_close(db); return 1; }

    sqlite3_close(db);
    return 0;
}
