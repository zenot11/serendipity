#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

static std::string getenv_or(const char* key, const std::string& def) {
    if (const char* v = std::getenv(key)) return std::string(v);
    return def;
}

static int getenv_int(const char* key, int def) {
    if (const char* v = std::getenv(key)) {
        try { return std::stoi(v); } catch (...) { return def; }
    }
    return def;
}

static bool getenv_bool(const char* key, bool def = false) {
    if (const char* v = std::getenv(key)) {
        std::string s(v);
        return (s == "1" || s == "true" || s == "yes");
    }
    return def;
}

static std::string extract_bearer(const httplib::Request& req) {
    auto auth = req.get_header_value("Authorization");
    const std::string pfx = "Bearer ";
    if (auth.rfind(pfx, 0) == 0 && auth.size() > pfx.size()) return auth.substr(pfx.size());
    return "";
}

static bool token_ok(const std::string& token) {
    // Валидны и “старые” access, и “обновленные” после refresh
    return !token.empty() &&
           (token.rfind("ACCESS_", 0) == 0 || token.rfind("ACCESSR_", 0) == 0);
}

static void reply_json(httplib::Response& res, int status, const json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

int main() {
    const int port = getenv_int("MAIN_PORT", 8082);

    // Если MAIN_FORCE_REFRESH=1:
    // - для токенов ACCESS_* отдаём 401 (как будто access устарел)
    // - для ACCESSR_* отдаём 200 (после refresh всё работает)
    const bool force_refresh = getenv_bool("MAIN_FORCE_REFRESH", false);

    // token -> list of notifications
    std::unordered_map<std::string, std::vector<std::string>> notif;
    std::mutex m;

    httplib::Server app;

    app.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout << "[MAIN_MOCK] " << req.method << " " << req.path
                  << " status=" << res.status << std::endl;
    });

    app.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });

    // GET /notification (Authorization: Bearer <access>)
    app.Get("/notification", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = extract_bearer(req);

        if (!token_ok(token)) {
            reply_json(res, 401, json{{"error", "unauthorized"}});
            return;
        }

        if (force_refresh && token.rfind("ACCESS_", 0) == 0) {
            reply_json(res, 401, json{{"error", "access expired (mock)"}});
            return;
        }

        json out = json::array();
        {
            std::lock_guard<std::mutex> lk(m);
            auto it = notif.find(token);
            if (it != notif.end()) {
                for (const auto& s : it->second) out.push_back(s);
            }
        }

        reply_json(res, 200, out);
    });

    // POST /notification/clear (Authorization: Bearer <access>)
    app.Post("/notification/clear", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = extract_bearer(req);

        if (!token_ok(token)) {
            reply_json(res, 401, json{{"error", "unauthorized"}});
            return;
        }

        if (force_refresh && token.rfind("ACCESS_", 0) == 0) {
            reply_json(res, 401, json{{"error", "access expired (mock)"}});
            return;
        }

        {
            std::lock_guard<std::mutex> lk(m);
            notif[token].clear();
        }

        reply_json(res, 200, json{{"ok", true}});
    });

    // POST /telegram/command (Authorization: Bearer <access>)
    // Body: {"chat_id":123,"text":"..."}
    app.Post("/telegram/command", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = extract_bearer(req);

        if (!token_ok(token)) {
            res.status = 401;
            res.set_content("unauthorized", "text/plain");
            return;
        }

        if (force_refresh && token.rfind("ACCESS_", 0) == 0) {
            res.status = 401;
            res.set_content("access expired (mock)", "text/plain");
            return;
        }

        json in;
        try { in = json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content("bad json", "text/plain");
            return;
        }

        long long chat_id = in.value("chat_id", 0LL);
        std::string text = in.value("text", "");

        if (chat_id == 0 || text.empty()) {
            res.status = 400;
            res.set_content("chat_id and text required", "text/plain");
            return;
        }

        // 403 ветка для демонстрации прав
        if (text.find("forbidden") != std::string::npos) {
            res.status = 403;
            res.set_content("forbidden", "text/plain");
            return;
        }

        res.status = 200;
        res.set_content("MAIN_MOCK handled: " + text, "text/plain");
    });

    // POST /notification/add (Authorization: Bearer <access>)
    // Body: {"text":"hello"}
    app.Post("/notification/add", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = extract_bearer(req);

        if (!token_ok(token)) {
            reply_json(res, 401, json{{"error", "unauthorized"}});
            return;
        }

        if (force_refresh && token.rfind("ACCESS_", 0) == 0) {
            reply_json(res, 401, json{{"error", "access expired (mock)"}});
            return;
        }

        json in;
        try { in = json::parse(req.body); }
        catch (...) {
            reply_json(res, 400, json{{"error", "bad json"}});
            return;
        }

        std::string text = in.value("text", "");
        if (text.empty()) {
            reply_json(res, 400, json{{"error", "text required"}});
            return;
        }

        size_t count = 0;
        {
            std::lock_guard<std::mutex> lk(m);
            notif[token].push_back(text);
            count = notif[token].size();
        }

        reply_json(res, 200, json{{"ok", true}, {"count", count}});
    });

    std::cout << "Main Mock listening on 0.0.0.0:" << port << std::endl;
    app.listen("0.0.0.0", port);
    return 0;
}
