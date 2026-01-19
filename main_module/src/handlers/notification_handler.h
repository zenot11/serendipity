#pragma once
#include "crow.h"
#include "../db/db.h"
#include "../security/jwt.h"
#include "../security/access.h"
#include "../security/auth_guard.h"

inline void registerNotificationRoutes(crow::SimpleApp& app, DB& db) {
    // Получить уведомления
    CROW_ROUTE(app, "/notification").methods("GET"_method)
    ([&db](const crow::request& req) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        crow::json::wvalue res;
        res["notifications"] = db.getUnsentNotifications(ctx.userId);

        return crow::response(200, res);
    });
    // Подтвердить что уведомления получены
    CROW_ROUTE(app, "/notification/confirm-tg").methods("POST"_method)
    ([&db](const crow::request& req) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto body = crow::json::load(req.body);
        if (!body || !body.has("ids")) return crow::response(400, "Invalid JSON");

        std::vector<int> ids;
        for (auto& id : body["ids"]) {
            ids.push_back(id.i());
        }

        db.markNotificationsAsSent(ids, ctx.userId);

        return crow::response(200, "OK");
    });
}