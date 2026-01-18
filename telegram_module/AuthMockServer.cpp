#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

static std::string getenv_or(const char* key, const std::string& def) {
    if (const char* v = std::getenv(key)) return std::string(v);
    return def;
}

static std::string mk_access(const std::string& state) {
    std::string s = state;
    if (s.size() > 8) s = s.substr(0, 8);
    return "ACCESS_" + s;
}

static std::string mk_refresh(const std::string& state) {
    std::string s = state;
    if (s.size() > 8) s = s.substr(0, 8);
    return "REFRESH_" + s;
}

int main() {
    const int port = std::stoi(getenv_or("AUTH_PORT", "8081"));
    const int confirm_sec = std::stoi(getenv_or("AUTH_CONFIRM_SEC", "15"));

    httplib::Server app;

    // Храним, когда "стартовали логин" по state/login_token
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> started_at;
    std::mutex mx;

    auto mark_started = [&](const std::string& state) {
        std::lock_guard<std::mutex> lk(mx);
        started_at[state] = std::chrono::steady_clock::now();
    };

    auto check_state = [&](const std::string& state) -> json {
        std::chrono::steady_clock::time_point t0;
        {
            std::lock_guard<std::mutex> lk(mx);
            auto it = started_at.find(state);
            if (it == started_at.end()) {
                return json{{"status","unknown"}};
            }
            t0 = it->second;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - t0
        ).count();

        if (elapsed < confirm_sec) {
            return json{{"status","pending"}};
        }

        // "Подтверждено" — выдаём токены
        // (и можно удалить state, чтобы дальше считалось завершенным)
        {
            std::lock_guard<std::mutex> lk(mx);
            started_at.erase(state);
        }

        return json{
            {"status","granted"},
            {"access_token", mk_access(state)},
            {"refresh_token", mk_refresh(state)}
        };
    };

    // Логи
    app.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout << "[AUTH_MOCK] " << req.method << " " << req.path
                  << " status=" << res.status << std::endl;
    });

    // Health
    app.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });

    // =========================================================
    // СОВМЕСТИМО С BotLogicServer.cpp (там GET /login и GET /check)
    // =========================================================

    // GET /login?type=github&state=....
    app.Get("/login", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("type") || !req.has_param("state")) {
            res.status = 400;
            res.set_content("type and state required", "text/plain");
            return;
        }
        const std::string type  = req.get_param_value("type");
        const std::string state = req.get_param_value("state");
        if (type.empty() || state.empty()) {
            res.status = 400;
            res.set_content("type and state required", "text/plain");
            return;
        }

        mark_started(state);

        // BotLogic просто "перешлет пользователю" body как текст.
        std::string msg =
            "ОК. (mock) Авторизация запущена для type=" + type +
            ". Подтверждение через ~" + std::to_string(confirm_sec) + " секунд.";
        res.set_content(msg, "text/plain");
    });

    // GET /check?state=...
    app.Get("/check", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("state")) {
            res.status = 400;
            res.set_content(R"({"status":"error","error":"state required"})", "application/json");
            return;
        }
        const std::string state = req.get_param_value("state");
        if (state.empty()) {
            res.status = 400;
            res.set_content(R"({"status":"error","error":"state required"})", "application/json");
            return;
        }

        json out = check_state(state);
        res.set_content(out.dump(), "application/json");
    });

    // =========================================================
    // ТВОИ СТАРЫЕ ЭНДПОИНТЫ (для ручных тестов)
    // =========================================================

    // POST /auth/start  { "login_token": "...", "type":"github" }
    app.Post("/auth/start", [&](const httplib::Request& req, httplib::Response& res) {
        json in;
        try { in = json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(R"({"ok":false,"error":"bad json"})", "application/json");
            return;
        }

        const std::string login_token = in.value("login_token", "");
        const std::string type = in.value("type", "");

        if (login_token.empty() || type.empty()) {
            res.status = 400;
            res.set_content(R"({"ok":false,"error":"login_token and type required"})", "application/json");
            return;
        }

        mark_started(login_token);

        json out = {
            {"ok", true},
            {"login_token", login_token},
            {"message", "ОК. (mock) Авторизация запущена. Подтверждение через ~" + std::to_string(confirm_sec) + " секунд."}
        };
        res.set_content(out.dump(), "application/json");
    });

    // POST /auth/check { "login_token":"..." }
    app.Post("/auth/check", [&](const httplib::Request& req, httplib::Response& res) {
        json in;
        try { in = json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(R"({"authorized":false,"error":"bad json"})", "application/json");
            return;
        }

        const std::string login_token = in.value("login_token", "");
        if (login_token.empty()) {
            res.status = 400;
            res.set_content(R"({"authorized":false,"error":"login_token required"})", "application/json");
            return;
        }

        json st = check_state(login_token);
        if (st.value("status","") == "granted") {
            json out = {
                {"authorized", true},
                {"access_token", st.value("access_token","")},
                {"refresh_token", st.value("refresh_token","")}
            };
            res.set_content(out.dump(), "application/json");
            return;
        }

        if (st.value("status","") == "unknown") {
            res.set_content(R"({"authorized":false,"status":"unknown"})", "application/json");
            return;
        }

        res.set_content(R"({"authorized":false,"status":"pending"})", "application/json");
    });

    std::cout << "Auth Mock listening on 0.0.0.0:" << port << std::endl;
    app.listen("0.0.0.0", port);
    return 0;
}
