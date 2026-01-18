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

static std::string extract_bearer(const httplib::Request& req) {
    auto auth = req.get_header_value("Authorization");
    const std::string pfx = "Bearer ";
    if (auth.rfind(pfx, 0) == 0 && auth.size() > pfx.size()) return auth.substr(pfx.size());
    return "";
}

static bool token_ok(const std::string& token) {
    // Достаточно для мока: считаем валидными токены вида ACCESS_...
    return !token.empty() && token.rfind("ACCESS_", 0) == 0;
}

int main() {
    const int port = std::stoi(getenv_or("MAIN_PORT", "8082"));

    // notifications per access_token
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
            res.status = 401;
            res.set_content(R"({"error":"unauthorized"})", "application/json");
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
        res.set_content(out.dump(), "application/json");
    });

    // POST /notification/clear (Authorization: Bearer <access>)
    app.Post("/notification/clear", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = extract_bearer(req);
        if (!token_ok(token)) {
            res.status = 401;
            res.set_content(R"({"error":"unauthorized"})", "application/json");
            return;
        }

        {
            std::lock_guard<std::mutex> lk(m);
            notif[token].clear();
        }
        res.set_content(R"({"ok":true})", "application/json");
    });

    // POST /telegram/command (Authorization: Bearer <access>)
    // Body: {"chat_id":123,"text":"..."}
    app.Post("/telegram/command", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = extract_bearer(req);
        if (!token_ok(token)) {
            res.status = 401;
            res.set_content(R"({"error":"unauthorized"})", "application/json");
            return;
        }

        json in;
        try { in = json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"bad json"})", "application/json");
            return;
        }

        long long chat_id = in.value("chat_id", 0LL);
        std::string text = in.value("text", "");
        if (chat_id == 0 || text.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"chat_id and text required"})", "application/json");
            return;
        }

        // Для наглядности: неизвестные команды “обрабатываем” и возвращаем текст
        json out = {
            {"ok", true},
            {"chat_id", chat_id},
            {"result", "MAIN_MOCK handled: " + text}
        };
        res.set_content(out.dump(), "application/json");
    });

    // Удобный тестовый эндпоинт, чтобы вручную “положить” уведомление для токена
    // POST /notification/add (Authorization: Bearer <access>)
    // Body: {"text":"hello"}
    app.Post("/notification/add", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = extract_bearer(req);
        if (!token_ok(token)) {
            res.status = 401;
            res.set_content(R"({"error":"unauthorized"})", "application/json");
            return;
        }

        json in;
        try { in = json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"bad json"})", "application/json");
            return;
        }

        std::string text = in.value("text", "");
        if (text.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"text required"})", "application/json");
            return;
        }

        size_t count = 0;
        {
            std::lock_guard<std::mutex> lk(m);
            notif[token].push_back(text);
            count = notif[token].size();
        }

        json out = {{"ok", true}, {"count", count}};
        res.set_content(out.dump(), "application/json");
    });

    std::cout << "Main Mock listening on 0.0.0.0:" << port << std::endl;
    app.listen("0.0.0.0", port);
    return 0;
}
