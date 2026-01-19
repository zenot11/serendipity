#pragma once
#include "crow.h"
#include "../db/db.h"
#include "../security/jwt.h"
#include "../security/access.h"
#include "../security/auth_guard.h"


inline void registerUserRoutes(crow::SimpleApp& app, DB& db) {
    // Информация о пользователе (курсы, оценки, тесты)
    CROW_ROUTE(app, "/users/<string>/data").methods("GET"_method)
    ([&db](const crow::request& req, std::string targetUserId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        bool isOwner = (ctx.userId == targetUserId);

        PermissionRule readRule{
            "user:data:read", 
            false, 
            nullptr};
        bool hasAdminPermission = (checkAccess(ctx, readRule, "").code == 200);

        if (!isOwner && !hasAdminPermission) {
            return crow::response(403, "Forbidden: You can only view your own data");
        }

        bool includeCourses = req.url_params.get("courses") != nullptr;
        bool includeTests = req.url_params.get("tests") != nullptr;
        bool includeGrades = req.url_params.get("grades") != nullptr;

        if (!includeCourses && !includeTests && !includeGrades) {
            includeCourses = includeTests = includeGrades = true; 
        }

        auto data = db.getUserDataProfile(targetUserId, includeCourses, includeTests, includeGrades);

        std::cout << data.dump(2);

        return crow::response(200, std::move(data));
    });
    // Посмотреть список пользователей
    CROW_ROUTE(app, "/users").methods("GET"_method)
    ([&db](const crow::request& req) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        PermissionRule readRule{
            "user:list:read", 
            false, 
            nullptr
        };

        if (checkAccess(ctx, readRule, "").code != 200) {
            return crow::response(403, "Forbidden");
        }

        cpr::Response r = cpr::Get(
            cpr::Url{"http://auth:8081/userservice/get_user_list"},
            cpr::Header{{"Content-Type", "application/json"},},
            cpr::Timeout{3000}
        );

        if (r.status_code == 200) {
            return crow::response(200, r.text);
        } else {
            return crow::response(500, "Error from auth-service");
        }
    });
    // Посмотреть информацию о пользователе (ФИО)
    CROW_ROUTE(app, "/users/<string>").methods("GET"_method)
    ([&db](const crow::request& req, std::string targetUserId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        crow::json::wvalue cmd;
        cmd["user_id"] = targetUserId;

        cpr::Response r = cpr::Get(
            cpr::Url{"http://auth:8081/userservice/get_user_info"},
            cpr::Body{cmd.dump()},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Timeout{3000}
        );

        std::cout << r.text;

        if (r.status_code == 200) {
            return crow::response(200, r.text);
        } else {
            return crow::response(500, "Error from auth-service");
        }
    });
    // Изменить ФИО пользователя
    CROW_ROUTE(app, "/users/<string>/name").methods("PATCH"_method)
    ([&db](const crow::request& req, std::string targetUserId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        bool isSelf = (ctx.userId == targetUserId);

        PermissionRule readRule{
            "user:fullName:write", 
            false, 
            nullptr
        };

        bool hasAdminRule = (checkAccess(ctx, readRule, "").code == 200);

        if (!isSelf && !hasAdminRule) {
            return crow::response(403, "Forbidden: You can only change your own name");
        }

        auto data = crow::json::load(req.body);
        if (!data || !data.has("full_name")) {
            return crow::response(400, "Invalid JSON: expected { 'full_name': '...' }");
        }
        std::string newName = data["full_name"].s();

        crow::json::wvalue cmd;
        cmd["user_id"] = targetUserId;
        cmd["new_name"] = newName;

        cpr::Response r = cpr::Patch(
            cpr::Url{"http://auth:8081/userservice/update_full_name"},
            cpr::Body{cmd.dump()},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Timeout{3000}
        );

        if (r.status_code == 200) {
            return crow::response(200, "Full name updated successfully");
        } else {
            return crow::response(500, "Error from auth-service");
        }
    });
    // Посмотреть роли пользователя
    CROW_ROUTE(app, "/users/<string>/roles").methods("GET"_method)
    ([&db](const crow::request& req, std::string targetUserId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        PermissionRule readRule{
            "user:roles:read", 
            false, 
            nullptr
        };

        if (checkAccess(ctx, readRule, "").code != 200) {
            return crow::response(403, "Forbidden");
        }

        crow::json::wvalue cmd;
        cmd["user_id"] = targetUserId;

        cpr::Response r = cpr::Get(
            cpr::Url{"http://auth:8081/userservice/get_user_roles"},
            cpr::Body{cmd.dump()},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Timeout{3000}
        );

        std::cout << r.text;

        if (r.status_code == 200) {
            return crow::response(200, r.text);
        } else {
            return crow::response(500, "Error from auth-service");
        }
    });
    // Изменить роли пользователю
    CROW_ROUTE(app, "/users/<string>/roles").methods("PATCH"_method)
    ([&db](const crow::request& req, std::string targetUserId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        PermissionRule readRule{
            "user:roles:write", 
            false, 
            nullptr
        };

        if (checkAccess(ctx, readRule, "").code != 200) {
            return crow::response(403, "Forbidden");
        }

        auto data = crow::json::load(req.body);
        if (!data || !data.has("roles") || data["roles"].t() != crow::json::type::List) {
            return crow::response(400, "Invalid JSON: expected { 'roles': ['role1', 'role2'] }");
        }

        crow::json::wvalue cmd;
        cmd["user_id"] = targetUserId;
        cmd["roles"] = data["roles"];

        cpr::Response r = cpr::Patch(
            cpr::Url{"http://auth:8081/userservice/update_user_roles"},
            cpr::Body{cmd.dump()},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Timeout{3000}
        );

        if (r.status_code == 200) {
            return crow::response(200, "Roles updated successfully");
        } else {
            return crow::response(500, "Error from auth-service");
        }
    });
    // Посмотреть заблокирован ли пользователь
    CROW_ROUTE(app, "/users/<string>/block-status").methods("GET"_method)
    ([&db](const crow::request& req, std::string targetUserId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        PermissionRule readRule{
            "user:block:read", 
            false, 
            nullptr
        };

        if (checkAccess(ctx, readRule, "").code != 200) {
            return crow::response(403, "Forbidden");
        }

        crow::json::wvalue cmd;
        cmd["user_id"] = targetUserId;

        cpr::Response r = cpr::Get(
            cpr::Url{"http://auth:8081/userservice/get_user_block_status"},
            cpr::Body{cmd.dump()},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Timeout{3000}
        );

        if (r.status_code == 200) {
            return crow::response(200, r.text);
        } else {
            return crow::response(500, "Error from auth-service");
        }
    });
    // Заблокировать/Разблокировать пользователя
    CROW_ROUTE(app, "/users/<string>/block").methods("POST"_method)
    ([&db](const crow::request& req, std::string targetUserId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        if (ctx.userId == targetUserId) {
            return crow::response(403, "Forbidden: You cannot block/unblock yourself");
        }

        PermissionRule readRule{
            "user:block:write", 
            false, 
            nullptr
        };

        if (checkAccess(ctx, readRule, "").code != 200) {
            return crow::response(403, "Forbidden");
        }

        auto data = crow::json::load(req.body);
        if (!data || !data.has("is_blocked")) {
            return crow::response(400, "Invalid JSON: expected { 'is_blocked': bool }");
        }
        bool shouldBlock = data["is_blocked"].b();

        crow::json::wvalue cmd;
        cmd["user_id"] = targetUserId;
        cmd["is_blocked"] = shouldBlock;

        cpr::Response r = cpr::Post(
            cpr::Url{"http://auth:8081/userservice/set_block_status"},
            cpr::Body{cmd.dump()},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Timeout{3000}
        );

        if (r.status_code == 200) {
            return crow::response(200, shouldBlock ? "User blocked" : "User unblocked");
        } else {
            return crow::response(500, "Error from auth-service");
        }
    });
}