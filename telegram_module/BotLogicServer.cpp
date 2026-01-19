#include <cstdlib>
#include <cctype>
#include <iostream>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <hiredis/hiredis.h>

#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

// ============================================================
// Utils
// ============================================================

static std::string getenv_or(const char* key, const std::string& def) {
    if (const char* v = std::getenv(key)) return std::string(v);
    return def;
}

static long long to_ll(const std::string& s, long long def = 0) {
    try { return std::stoll(s); } catch (...) { return def; }
}

static std::vector<std::string> split_ws(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string w;
    while (iss >> w) out.push_back(w);
    return out;
}

static std::string random_hex_token(size_t bytes = 16) {
    std::random_device rd;
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes * 2);
    for (size_t i = 0; i < bytes; ++i) {
        unsigned int b = rd() & 0xFF;
        out.push_back(hex[(b >> 4) & 0xF]);
        out.push_back(hex[b & 0xF]);
    }
    return out;
}

static json wrap_messages(const std::vector<std::pair<long long, std::string>>& msgs) {
    json out;
    out["messages"] = json::array();
    for (auto& [cid, text] : msgs) {
        out["messages"].push_back({{"chat_id", cid}, {"text", text}});
    }
    return out;
}

// ============================================================
// Redis store
// ============================================================

struct UserState {
    std::string status;        // "anonymous" | "authorized"
    std::string login_token;   // if anonymous
    std::string access_token;  // if authorized
    std::string refresh_token; // if authorized
};

static std::string redis_key_chat(long long chat_id) {
    return "tg:chat:" + std::to_string(chat_id);
}

static std::optional<UserState> parse_state(const std::string& s) {
    try {
        auto j = json::parse(s);
        UserState st;
        st.status = j.value("status", "");
        st.login_token = j.value("login_token", "");
        st.access_token = j.value("access_token", "");
        st.refresh_token = j.value("refresh_token", "");
        if (st.status.empty()) return std::nullopt;
        return st;
    } catch (...) {
        return std::nullopt;
    }
}

static std::string dump_state(const UserState& st) {
    json j = {
        {"status", st.status},
        {"login_token", st.login_token},
        {"access_token", st.access_token},
        {"refresh_token", st.refresh_token}
    };
    return j.dump();
}

class RedisStore {
public:
    RedisStore(const std::string& host, int port) : host_(host), port_(port) {
        reconnect();
    }

    std::optional<UserState> get(long long chat_id) {
        std::lock_guard<std::mutex> lk(m_);
        ensure();
        auto key = redis_key_chat(chat_id);

        redisReply* r = (redisReply*)redisCommand(ctx_, "GET %s", key.c_str());
        if (!r) { reconnect(); return std::nullopt; }

        std::optional<UserState> out;
        if (r->type == REDIS_REPLY_STRING && r->str) {
            out = parse_state(std::string(r->str, r->len));
        }
        freeReplyObject(r);
        return out;
    }

    void set_anonymous(long long chat_id, const std::string& login_token) {
        UserState st;
        st.status = "anonymous";
        st.login_token = login_token;

        std::lock_guard<std::mutex> lk(m_);
        ensure();

        auto key = redis_key_chat(chat_id);
        auto val = dump_state(st);

        redisReply* r1 = (redisReply*)redisCommand(ctx_, "SET %s %b", key.c_str(), val.data(), (size_t)val.size());
        if (r1) freeReplyObject(r1);

        redisReply* r2 = (redisReply*)redisCommand(ctx_, "SREM tg:auth %lld", chat_id);
        if (r2) freeReplyObject(r2);
        redisReply* r3 = (redisReply*)redisCommand(ctx_, "SADD tg:anon %lld", chat_id);
        if (r3) freeReplyObject(r3);
    }

    void set_authorized(long long chat_id, const std::string& access, const std::string& refresh) {
        UserState st;
        st.status = "authorized";
        st.access_token = access;
        st.refresh_token = refresh;

        std::lock_guard<std::mutex> lk(m_);
        ensure();

        auto key = redis_key_chat(chat_id);
        auto val = dump_state(st);

        redisReply* r1 = (redisReply*)redisCommand(ctx_, "SET %s %b", key.c_str(), val.data(), (size_t)val.size());
        if (r1) freeReplyObject(r1);

        redisReply* r2 = (redisReply*)redisCommand(ctx_, "SREM tg:anon %lld", chat_id);
        if (r2) freeReplyObject(r2);
        redisReply* r3 = (redisReply*)redisCommand(ctx_, "SADD tg:auth %lld", chat_id);
        if (r3) freeReplyObject(r3);
    }

    void del(long long chat_id) {
        std::lock_guard<std::mutex> lk(m_);
        ensure();

        auto key = redis_key_chat(chat_id);

        redisReply* r1 = (redisReply*)redisCommand(ctx_, "DEL %s", key.c_str());
        if (r1) freeReplyObject(r1);

        redisReply* r2 = (redisReply*)redisCommand(ctx_, "SREM tg:anon %lld", chat_id);
        if (r2) freeReplyObject(r2);
        redisReply* r3 = (redisReply*)redisCommand(ctx_, "SREM tg:auth %lld", chat_id);
        if (r3) freeReplyObject(r3);
    }

    std::vector<long long> all_anonymous() { return smembers_ll("tg:anon"); }
    std::vector<long long> all_authorized() { return smembers_ll("tg:auth"); }

private:
    std::vector<long long> smembers_ll(const std::string& set_key) {
        std::lock_guard<std::mutex> lk(m_);
        ensure();

        redisReply* r = (redisReply*)redisCommand(ctx_, "SMEMBERS %s", set_key.c_str());
        std::vector<long long> out;
        if (!r) { reconnect(); return out; }

        if (r->type == REDIS_REPLY_ARRAY) {
            for (size_t i = 0; i < r->elements; ++i) {
                auto* e = r->element[i];
                if (!e || e->type != REDIS_REPLY_STRING || !e->str) continue;
                try { out.push_back(std::stoll(std::string(e->str, e->len))); }
                catch (...) {}
            }
        }

        freeReplyObject(r);
        return out;
    }

    void ensure() {
        if (!ctx_ || ctx_->err) reconnect();
    }

    void reconnect() {
        if (ctx_) {
            redisFree(ctx_);
            ctx_ = nullptr;
        }
        ctx_ = redisConnect(host_.c_str(), port_);
        if (!ctx_ || ctx_->err) {
            std::cerr << "[Redis] connect failed\n";
        }
    }

    std::string host_;
    int port_;
    redisContext* ctx_{nullptr};
    std::mutex m_;
};

// ============================================================
// HTTP client helpers
// ============================================================

struct BaseUrl {
    std::string host;
    int port{80};
};

static std::optional<BaseUrl> parse_http_base(const std::string& base) {
    std::string b = base;
    if (b.rfind("http://", 0) == 0) b = b.substr(7);

    auto slash = b.find('/');
    if (slash != std::string::npos) b = b.substr(0, slash);

    BaseUrl out;
    auto colon = b.find(':');
    if (colon == std::string::npos) {
        out.host = b;
        out.port = 80;
        return out;
    }

    out.host = b.substr(0, colon);
    out.port = (int)to_ll(b.substr(colon + 1), 80);
    if (out.host.empty()) return std::nullopt;
    return out;
}

struct HttpResp {
    int status{0};
    std::string body;
};

static std::optional<HttpResp> http_get(
    const std::string& base,
    const std::string& path,
    const std::vector<std::pair<std::string,std::string>>& headers = {}
) {
    auto bu = parse_http_base(base);
    if (!bu) return std::nullopt;

    httplib::Client cli(bu->host, bu->port);
    cli.set_read_timeout(10, 0);

    httplib::Headers hh;
    for (auto& [k,v] : headers) hh.emplace(k, v);

    auto res = cli.Get(path.c_str(), hh);
    if (!res) return std::nullopt;
    return HttpResp{res->status, res->body};
}

static std::optional<HttpResp> http_post_json(
    const std::string& base,
    const std::string& path,
    const json& body,
    const std::vector<std::pair<std::string,std::string>>& headers = {}
) {
    auto bu = parse_http_base(base);
    if (!bu) return std::nullopt;

    httplib::Client cli(bu->host, bu->port);
    cli.set_read_timeout(10, 0);

    httplib::Headers hh;
    for (auto& [k,v] : headers) hh.emplace(k, v);

    auto res = cli.Post(path.c_str(), hh, body.dump(), "application/json");
    if (!res) return std::nullopt;
    return HttpResp{res->status, res->body};
}

// ============================================================
// Auth client
// ============================================================

struct LoginCheckResult {
    enum class Kind { Pending, Granted, Denied, ExpiredOrUnknown, Error } kind{Kind::Error};
    std::string access_token;
    std::string refresh_token;
};

class AuthClient {
public:
    AuthClient() {
        base_ = getenv_or("AUTH_BASE", "http://localhost:8081");
        path_login_   = getenv_or("AUTH_LOGIN_PATH",   "/auth/start");
        path_check_   = getenv_or("AUTH_CHECK_PATH",   "/auth/check");
        path_refresh_ = getenv_or("AUTH_REFRESH_PATH", "/refresh");
        path_logout_  = getenv_or("AUTH_LOGOUT_PATH",  "/logout");
    }

    std::optional<std::string> start_login(const std::string& type, const std::string& login_token) {
        json body = {{"login_token", login_token}, {"type", type}};
        auto r = http_post_json(base_, path_login_, body);
        if (!r) return std::nullopt;

        try {
            auto j = json::parse(r->body);
            if (j.contains("message") && j["message"].is_string()) {
                return j["message"].get<std::string>();
            }
        } catch (...) {}
        return r->body;
    }

    LoginCheckResult check_login(const std::string& login_token) {
        json body = {{"login_token", login_token}};
        auto r = http_post_json(base_, path_check_, body);
        if (!r) return {LoginCheckResult::Kind::Error, {}, {}};

        // Если auth отдаёт 404/410 или специальный статус — можно трактовать как ExpiredOrUnknown
        if (r->status == 404 || r->status == 410) {
            return {LoginCheckResult::Kind::ExpiredOrUnknown, {}, {}};
        }

        try {
            auto j = json::parse(r->body);
            bool authorized = j.value("authorized", false);
            std::string access  = j.value("access_token", "");
            std::string refresh = j.value("refresh_token", "");

            if (authorized && !access.empty() && !refresh.empty()) {
                return {LoginCheckResult::Kind::Granted, access, refresh};
            }
            return {LoginCheckResult::Kind::Pending, {}, {}};
        } catch (...) {
            return {LoginCheckResult::Kind::Error, {}, {}};
        }
    }

    std::optional<std::pair<std::string,std::string>> refresh_tokens(const std::string& refresh_token) {
        auto r = http_post_json(base_, path_refresh_, json{{"refresh_token", refresh_token}});
        if (!r) return std::nullopt;
        if (r->status == 401) return std::nullopt;

        try {
            auto j = json::parse(r->body);
            std::string access = j.value("access_token", "");
            std::string refresh = j.value("refresh_token", "");
            if (!access.empty() && !refresh.empty()) return std::make_pair(access, refresh);
        } catch (...) {}
        return std::nullopt;
    }

    void logout_all(const std::string& refresh_token) {
        (void)http_post_json(base_, path_logout_, json{{"refresh_token", refresh_token}});
    }

private:
    std::string base_;
    std::string path_login_;
    std::string path_check_;
    std::string path_refresh_;
    std::string path_logout_;
};

// ============================================================
// Main client
// ============================================================

class MainClient {
public:
    MainClient() {
        base_ = getenv_or("MAIN_BASE", "http://localhost:8082");
        path_notification_       = getenv_or("MAIN_NOTIFICATION_PATH",       "/notification");
        path_notification_clear_ = getenv_or("MAIN_NOTIFICATION_CLEAR_PATH", "/notification/clear");
        path_command_            = getenv_or("MAIN_COMMAND_PATH",            "/telegram/command");
    }

    std::optional<HttpResp> get_notifications(const std::string& access_token) {
        return http_get(base_, path_notification_, {{"Authorization", "Bearer " + access_token}});
    }

    std::optional<HttpResp> clear_notifications(const std::string& access_token) {
        return http_post_json(base_, path_notification_clear_, json::object(),
                              {{"Authorization", "Bearer " + access_token}});
    }

    std::optional<HttpResp> forward_command(const std::string& access_token, long long chat_id, const std::string& text) {
        json body = {{"chat_id", chat_id}, {"text", text}};
        return http_post_json(base_, path_command_, body, {{"Authorization", "Bearer " + access_token}});
    }

private:
    std::string base_;
    std::string path_notification_;
    std::string path_notification_clear_;
    std::string path_command_;
};

// ============================================================
// Command helpers
// ============================================================

static std::string parse_login_type(const std::vector<std::string>& parts) {
    if (parts.size() < 2) return "";
    std::string t = parts[1];
    if (t.rfind("type=", 0) == 0) return t.substr(5);
    return t;
}

static bool parse_logout_all(const std::vector<std::string>& parts) {
    for (const auto& p : parts) {
        if (p == "all=true" || p == "all=1") return true;
    }
    return false;
}

static bool parse_peek_notifications(const std::vector<std::string>& parts) {
    for (const auto& p : parts) {
        if (p == "peek=true" || p == "peek=1") return true;
    }
    return false;
}

// ============================================================
// Texts
// ============================================================

static std::string auth_hint_text() {
    return
        "Вы не авторизованы.\n"
        "Варианты входа:\n"
        "/login github\n"
        "/login yandex\n"
        "/login code";
}

static std::string already_auth_text() { return "Вы уже авторизованы."; }
static std::string denied_text() { return "Неудачная авторизация."; }

// ============================================================
// Helpers: notifications flow (shared by /notification and cron)
// ============================================================

// Returns formatted notifications text if there are any, otherwise nullopt.
// Expected main response is a JSON array of strings, e.g. ["n1","n2"].
// If format is unexpected, we fall back to returning the raw body.
static std::optional<std::string> normalize_notifications_body(const std::string& body) {
    auto trim = [](std::string s) {
        auto is_ws = [](unsigned char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; };
        while (!s.empty() && is_ws((unsigned char)s.front())) s.erase(s.begin());
        while (!s.empty() && is_ws((unsigned char)s.back())) s.pop_back();
        return s;
    };

    std::string b = trim(body);
    if (b.empty()) return std::nullopt;

    // Fast path for common empty payloads
    if (b == "[]" || b == "{}" || b == "null") return std::nullopt;

    // Try to parse JSON
    try {
        auto j = json::parse(b);

        // Main mock returns an array of strings
        if (j.is_array()) {
            if (j.empty()) return std::nullopt;

            // If array of strings -> join nicely
            bool all_strings = true;
            for (auto& it : j) {
                if (!it.is_string()) { all_strings = false; break; }
            }

            if (all_strings) {
                // Human-friendly formatting for Telegram
                std::ostringstream oss;
                for (size_t i = 0; i < j.size(); ++i) {
                    oss << "- " << j[i].get<std::string>();
                    if (i + 1 < j.size()) oss << "\n";
                }
                return oss.str();
            }

            // Non-string array -> pretty dump
            return j.dump(2);
        }

        // Some servers may wrap notifications
        if (j.is_object()) {
            if (j.contains("notifications") && j["notifications"].is_array()) {
                auto arr = j["notifications"];
                if (arr.empty()) return std::nullopt;
                bool all_strings = true;
                for (auto& it : arr) {
                    if (!it.is_string()) { all_strings = false; break; }
                }
                if (all_strings) {
                    std::ostringstream oss;
                    for (size_t i = 0; i < arr.size(); ++i) {
                        oss << "- " << arr[i].get<std::string>();
                        if (i + 1 < arr.size()) oss << "\n";
                    }
                    return oss.str();
                }
                return arr.dump(2);
            }

            // Generic object: treat as non-empty payload
            return j.dump(2);
        }

        // Primitive JSON (string/number/bool): treat as a notification
        return j.dump();
    } catch (...) {
        // Not JSON: if it isn't one of empty sentinels, pass through
        return b;
    }
}

static bool status_2xx(int s) { return s >= 200 && s < 300; }

// Toggle cron processing (useful for deterministic manual E2E tests)
// Set CRON_ENABLED=0 to disable both cron endpoints.
static bool cron_enabled() {
    static bool enabled = []() {
        std::string v = getenv_or("CRON_ENABLED", "1");
        // treat "0", "false", "off" (case-insensitive) as disabled
        for (auto& c : v) c = (char)std::tolower((unsigned char)c);
        return !(v == "0" || v == "false" || v == "off");
    }();
    return enabled;
}

// ============================================================
// main()
// ============================================================

int main() {
    const std::string redis_host = getenv_or("REDIS_HOST", "127.0.0.1");
    const int redis_port = (int)to_ll(getenv_or("REDIS_PORT", "6379"), 6379);
    const int port = (int)to_ll(getenv_or("BOT_LOGIC_PORT", "8080"), 8080);

    RedisStore redis(redis_host, redis_port);
    AuthClient auth;
    MainClient mainc;

    httplib::Server srv;

    // ----------------------------
    // POST /bot/handle
    // ----------------------------
    srv.Post("/bot/handle", [&](const httplib::Request& req, httplib::Response& res) {
        long long chat_id = 0;
        std::string text;

        try {
            auto j = json::parse(req.body);
            chat_id = j.value("chat_id", 0LL);
            text = j.value("text", "");
        } catch (...) {
            res.status = 400;
            res.set_content(wrap_messages({{0, "Bad request"}}).dump(), "application/json");
            return;
        }

        if (chat_id == 0 || text.empty()) {
            res.status = 400;
            res.set_content(wrap_messages({{chat_id, "Bad request"}}).dump(), "application/json");
            return;
        }

        auto parts = split_ws(text);
        std::string cmd = parts.empty() ? "" : parts[0];
        if (!cmd.empty() && cmd[0] == '/') cmd = cmd.substr(1);

        auto st = redis.get(chat_id);
        std::vector<std::pair<long long, std::string>> out;

        // login
        if (cmd == "login") {
            if (st && st->status == "authorized") {
                out.push_back({chat_id, already_auth_text()});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            std::string type = parse_login_type(parts);
            if (type.empty()) {
                out.push_back({chat_id, auth_hint_text()});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            std::string login_token = random_hex_token(16);
            redis.set_anonymous(chat_id, login_token);

            auto ans = auth.start_login(type, login_token);
            if (!ans) {
                out.push_back({chat_id, "Ошибка: модуль авторизации недоступен."});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            out.push_back({chat_id, *ans});
            res.set_content(wrap_messages(out).dump(), "application/json");
            return;
        }

        // Unknown user
        if (!st) {
            out.push_back({chat_id, auth_hint_text()});
            res.set_content(wrap_messages(out).dump(), "application/json");
            return;
        }

        // Anonymous user
        if (st->status == "anonymous") {
            auto chk = auth.check_login(st->login_token);

            if (chk.kind == LoginCheckResult::Kind::ExpiredOrUnknown) {
                redis.del(chat_id);
                out.push_back({chat_id, auth_hint_text()});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            if (chk.kind == LoginCheckResult::Kind::Denied) {
                redis.del(chat_id);
                out.push_back({chat_id, denied_text()});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            if (chk.kind == LoginCheckResult::Kind::Granted) {
                redis.set_authorized(chat_id, chk.access_token, chk.refresh_token);
                st = UserState{"authorized", "", chk.access_token, chk.refresh_token};
            } else {
                out.push_back({chat_id, "Ожидаю подтверждения входа. Если не начинали вход — /login <type>."});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }
        }

        // Authorized user
        if (st->status == "authorized") {
            if (cmd == "logout") {
                bool all = parse_logout_all(parts);
                std::string refresh = st->refresh_token;

                redis.del(chat_id);

                if (all && !refresh.empty()) {
                    auth.logout_all(refresh);
                    out.push_back({chat_id, "Сеанс завершён на всех устройствах."});
                } else {
                    out.push_back({chat_id, "Сеанс завершён."});
                }

                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            if (cmd == "start") {
                out.push_back({chat_id, "Вы авторизованы. Используйте /help."});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            if (cmd == "help") {
                out.push_back({chat_id,
                    "Команды:\n"
                    "/login (подсказка входа)\n"
                    "/logout\n"
                    "/logout all=true\n"
                    "/notification (ручная проверка уведомлений; добавь peek=true чтобы не очищать)\n"
                    "Остальные команды прокидываются в Главный модуль (см. MAIN_COMMAND_PATH)."
                });
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            if (cmd == "ping") {
                out.push_back({chat_id, "pong"});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            if (cmd == "status") {
                out.push_back({chat_id, "Bot Logic работает."});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            // --- ВАЖНО: /notification (ручная проверка)
            if (cmd == "notification") {
                bool peek = parse_peek_notifications(parts);
                std::string access_used = st->access_token;

                auto r = mainc.get_notifications(access_used);
                if (!r) {
                    out.push_back({chat_id, "Ошибка: главный модуль недоступен."});
                    res.set_content(wrap_messages(out).dump(), "application/json");
                    return;
                }

                if (r->status == 401) {
                    auto newpair = auth.refresh_tokens(st->refresh_token);
                    if (!newpair) {
                        redis.del(chat_id);
                        out.push_back({chat_id, auth_hint_text()});
                        res.set_content(wrap_messages(out).dump(), "application/json");
                        return;
                    }

                    redis.set_authorized(chat_id, newpair->first, newpair->second);
                    access_used = newpair->first;

                    r = mainc.get_notifications(access_used);
                    if (!r) {
                        out.push_back({chat_id, "Ошибка: главный модуль недоступен."});
                        res.set_content(wrap_messages(out).dump(), "application/json");
                        return;
                    }
                }

                if (!status_2xx(r->status) && r->status != 401 && r->status != 403) {
                    std::cerr << "[bot_logic] /notification: main status=" << r->status
                              << " body=" << r->body << "\n";
                }

                if (r->status == 403) {
                    out.push_back({chat_id, "Не достаточно прав для этого действия."});
                    res.set_content(wrap_messages(out).dump(), "application/json");
                    return;
                }

                if (status_2xx(r->status)) {
                    auto body_opt = normalize_notifications_body(r->body);
                    if (!body_opt) {
                        out.push_back({chat_id, "Уведомлений нет."});
                    } else {
                        out.push_back({chat_id, "Уведомления:\n" + *body_opt + (peek ? "\n\n(режим peek — список не очищен)" : "")});
                        if (!peek) {
                            auto clr = mainc.clear_notifications(access_used);
                            if (!clr || !status_2xx(clr->status)) {
                                std::cerr << "[bot_logic] WARN: clear_notifications failed, status="
                                          << (clr ? clr->status : 0) << "\n";
                            }
                        }
                    }
                    res.set_content(wrap_messages(out).dump(), "application/json");
                    return;
                }

                out.push_back({chat_id, "Ошибка главного модуля: " + std::to_string(r->status)});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            // --- Прокидываем прочие команды в главный модуль
            auto attempt_main = [&](const std::string& access) -> std::optional<HttpResp> {
                return mainc.forward_command(access, chat_id, text);
            };

            auto r = attempt_main(st->access_token);
            if (!r) {
                out.push_back({chat_id, "Ошибка: главный модуль недоступен."});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            if (r->status == 403) {
                out.push_back({chat_id, "Не достаточно прав для этого действия."});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            if (r->status == 401) {
                auto newpair = auth.refresh_tokens(st->refresh_token);
                if (!newpair) {
                    redis.del(chat_id);
                    out.push_back({chat_id, auth_hint_text()});
                    res.set_content(wrap_messages(out).dump(), "application/json");
                    return;
                }

                redis.set_authorized(chat_id, newpair->first, newpair->second);

                auto r2 = attempt_main(newpair->first);
                if (!r2) {
                    out.push_back({chat_id, "Ошибка: главный модуль недоступен."});
                } else if (r2->status == 403) {
                    out.push_back({chat_id, "Не достаточно прав для этого действия."});
                } else if (status_2xx(r2->status)) {
                    out.push_back({chat_id, r2->body.empty() ? "OK" : r2->body});
                } else if (r2->status == 401) {
                    redis.del(chat_id);
                    out.push_back({chat_id, auth_hint_text()});
                } else {
                    out.push_back({chat_id, "Ошибка главного модуля: " + std::to_string(r2->status)});
                }

                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            if (status_2xx(r->status)) {
                out.push_back({chat_id, r->body.empty() ? "OK" : r->body});
                res.set_content(wrap_messages(out).dump(), "application/json");
                return;
            }

            out.push_back({chat_id, "Ошибка главного модуля: " + std::to_string(r->status)});
            res.set_content(wrap_messages(out).dump(), "application/json");
            return;
        }

        // unexpected status
        redis.del(chat_id);
        out.push_back({chat_id, auth_hint_text()});
        res.set_content(wrap_messages(out).dump(), "application/json");
    });

    // ----------------------------
    // POST /bot/cron/login_check
    // ----------------------------
    srv.Post("/bot/cron/login_check", [&](const httplib::Request&, httplib::Response& res) {
        std::vector<std::pair<long long, std::string>> out;
        if (!cron_enabled()) {
            res.set_content(wrap_messages(out).dump(), "application/json");
            return;
        }

        for (long long chat_id : redis.all_anonymous()) {
            auto st = redis.get(chat_id);
            if (!st || st->status != "anonymous") {
                redis.del(chat_id);
                continue;
            }

            auto chk = auth.check_login(st->login_token);

            if (chk.kind == LoginCheckResult::Kind::ExpiredOrUnknown) {
                redis.del(chat_id);
                continue;
            }

            if (chk.kind == LoginCheckResult::Kind::Denied) {
                redis.del(chat_id);
                out.push_back({chat_id, denied_text()});
                continue;
            }

            if (chk.kind == LoginCheckResult::Kind::Granted) {
                redis.set_authorized(chat_id, chk.access_token, chk.refresh_token);
                out.push_back({chat_id, "Успешная авторизация."});
                continue;
            }
        }

        res.set_content(wrap_messages(out).dump(), "application/json");
    });

    // ----------------------------
    // POST /bot/cron/notifications_check
    // ----------------------------
    srv.Post("/bot/cron/notifications_check", [&](const httplib::Request&, httplib::Response& res) {
        std::vector<std::pair<long long, std::string>> out;
        if (!cron_enabled()) {
            res.set_content(wrap_messages(out).dump(), "application/json");
            return;
        }

        for (long long chat_id : redis.all_authorized()) {
            auto st = redis.get(chat_id);
            if (!st || st->status != "authorized") {
                redis.del(chat_id);
                continue;
            }

            std::string access_used = st->access_token;

            auto r = mainc.get_notifications(access_used);
            if (!r) continue;

            if (r->status == 401) {
                auto newpair = auth.refresh_tokens(st->refresh_token);
                if (!newpair) {
                    redis.del(chat_id);
                    out.push_back({chat_id, auth_hint_text()});
                    continue;
                }

                redis.set_authorized(chat_id, newpair->first, newpair->second);
                access_used = newpair->first;

                r = mainc.get_notifications(access_used);
                if (!r) continue;
            }

            if (!status_2xx(r->status) && r->status != 401 && r->status != 403) {
                std::cerr << "[bot_logic] cron/notifications_check: main status=" << r->status
                          << " body=" << r->body << "\n";
            }

            if (r->status == 403) continue;

            if (status_2xx(r->status)) {
                auto body_opt = normalize_notifications_body(r->body);
                if (body_opt) {
                    out.push_back({chat_id, "Уведомления:\n" + *body_opt});
                    // Cron always consumes notifications to avoid duplicates.
                    auto clr = mainc.clear_notifications(access_used);
                    if (!clr || !status_2xx(clr->status)) {
                        std::cerr << "[bot_logic] WARN: cron clear_notifications failed, status="
                                  << (clr ? clr->status : 0) << "\n";
                    }
                }
            }
        }

        res.set_content(wrap_messages(out).dump(), "application/json");
    });

    srv.Get("/health", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });

    std::cout << "Bot Logic listening on 0.0.0.0:" << port << "\n";
    srv.listen("0.0.0.0", port);
}
